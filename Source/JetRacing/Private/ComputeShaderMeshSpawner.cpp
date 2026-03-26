#include "ComputeShaderMeshSpawner.h"
#include "ComputeShaderDeclaration.h"
#include "ComputeDrivenIndirectInstancingComponent.h"

#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RHICommandList.h"
#include "RHIResources.h"
#include "GlobalShader.h"
#include "RenderingThread.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

UComputeShaderMeshSpawner::UComputeShaderMeshSpawner()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UComputeShaderMeshSpawner::BeginPlay()
{
    Super::BeginPlay();

    IndirectInstancingComponent = NewObject<UComputeDrivenIndirectInstancingComponent>(
        GetOwner(), TEXT("ComputeDrivenISMC"));
    IndirectInstancingComponent->RegisterComponent();
    IndirectInstancingComponent->AttachToComponent(
        GetOwner()->GetRootComponent(),
        FAttachmentTransformRules::KeepRelativeTransform);

    IndirectInstancingComponent->MaxInstances = NumInstances;

    if (FoliageMesh)
        IndirectInstancingComponent->Mesh = FoliageMesh;
    if (FoliageMaterial)
        IndirectInstancingComponent->Material = FoliageMaterial;

    SetupDepthCapture();
    UpdateVoxelComponentList();

    // Force a render frame so CreateRenderThreadResources fires and GPU buffers
    // are allocated before we try to dispatch into them.
    FlushRenderingCommands();

    CaptureDepth();

    // Block until the capture is done then dispatch once synchronously.
    FlushRenderingCommands();
    ExecuteComputeShader();
}

void UComputeShaderMeshSpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);
}

// ---------------------------------------------------------------------------
// Depth capture
// ---------------------------------------------------------------------------
void UComputeShaderMeshSpawner::SetupDepthCapture()
{
    if (!DepthRenderTarget)
    {
        DepthRenderTarget = NewObject<UTextureRenderTarget2D>();
        DepthRenderTarget->RenderTargetFormat = RTF_R32f;
        DepthRenderTarget->InitAutoFormat(2048, 2048);
        DepthRenderTarget->UpdateResourceImmediate(true);
    }

    SceneCaptureComponent = NewObject<USceneCaptureComponent2D>(
        GetOwner(), TEXT("DepthCaptureComp"));
    SceneCaptureComponent->RegisterComponent();
    SceneCaptureComponent->AttachToComponent(
        GetOwner()->GetRootComponent(),
        FAttachmentTransformRules::KeepRelativeTransform);

    SceneCaptureComponent->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
    SceneCaptureComponent->TextureTarget       = DepthRenderTarget;
    SceneCaptureComponent->CaptureSource       = SCS_SceneDepth;
    SceneCaptureComponent->bCaptureEveryFrame  = false;
    SceneCaptureComponent->bCaptureOnMovement  = false;

    SceneCaptureComponent->SetWorldLocation(CameraLocation);
    SceneCaptureComponent->SetWorldRotation(CameraRotation);
    SceneCaptureComponent->ProjectionType = ECameraProjectionMode::Orthographic;
    SceneCaptureComponent->OrthoWidth     = OrthoWidth;
}

void UComputeShaderMeshSpawner::CaptureDepth()
{
    if (bCaptureInProgress || !SceneCaptureComponent)
        return;

    SceneCaptureComponent->ShowOnlyComponents.Remove(nullptr);
    SceneCaptureComponent->SetWorldLocation(CameraLocation);
    SceneCaptureComponent->SetWorldRotation(CameraRotation);
    SceneCaptureComponent->OrthoWidth = OrthoWidth;

    // CaptureScene() must be called on the game thread — it already is here,
    // so call it directly instead of dispatching another AsyncTask.
    SceneCaptureComponent->CaptureScene();
}

// ---------------------------------------------------------------------------
// Compute dispatch
// ---------------------------------------------------------------------------
void UComputeShaderMeshSpawner::RunComputeShader()
{
    if (!IndirectInstancingComponent || !DepthRenderTarget)
        return;

    // GPU buffers are allocated by the scene proxy in CreateRenderThreadResources.
    // Guard until they are ready.
    FBufferRHIRef              CapturedInstanceBuffer    = IndirectInstancingComponent->GpuInstanceBuffer;
    FUnorderedAccessViewRHIRef CapturedInstanceBufferUAV = IndirectInstancingComponent->GpuInstanceBufferUAV;
    FBufferRHIRef              CapturedIndirectBuffer    = IndirectInstancingComponent->GpuIndirectArgsBuffer;
    FUnorderedAccessViewRHIRef CapturedIndirectBufferUAV = IndirectInstancingComponent->GpuIndirectArgsBufferUAV;
    int32                      CapturedMeshNumIndices    = IndirectInstancingComponent->GpuMeshNumIndices;

    if (!CapturedInstanceBuffer.IsValid()    || !CapturedInstanceBufferUAV.IsValid() ||
        !CapturedIndirectBuffer.IsValid()    || !CapturedIndirectBufferUAV.IsValid() ||
        CapturedMeshNumIndices <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("UComputeShaderMeshSpawner: GPU buffers not ready yet."));
        return;
    }

    FTextureRHIRef CapturedDepthTexture = DepthRenderTarget->GetResource()->TextureRHI;
    if (!CapturedDepthTexture.IsValid())
        return;

    FRotationMatrix RotMatrix(CameraRotation);
    FVector3f CapturedCameraPos     = FVector3f(CameraLocation);
    FVector3f CapturedCameraForward = FVector3f(RotMatrix.GetScaledAxis(EAxis::X));
    FVector3f CapturedCameraRight   = FVector3f(RotMatrix.GetScaledAxis(EAxis::Y));
    FVector3f CapturedCameraUp      = FVector3f(RotMatrix.GetScaledAxis(EAxis::Z));

    float  CapturedOrthoWidth   = OrthoWidth;
    float  CapturedOrthoHeight  = OrthoWidth;
    uint32 CapturedNumInstances = (uint32)NumInstances;
    uint32 CapturedNumIndices   = (uint32)CapturedMeshNumIndices;
    float  CapturedGridCellSize = GridCellSize;
    float  CapturedSpawnDensity = SpawnDensity;
    float  CapturedVertOffset   = VerticalOffset;
    float  CapturedScaleMin     = ScaleMin;
    float  CapturedScaleMax     = ScaleMax;

    ENQUEUE_RENDER_COMMAND(DispatchFoliageComputeShader)(
        [CapturedInstanceBufferUAV,
         CapturedIndirectBuffer, CapturedIndirectBufferUAV,
         CapturedDepthTexture,
         CapturedCameraPos, CapturedCameraForward, CapturedCameraRight, CapturedCameraUp,
         CapturedOrthoWidth, CapturedOrthoHeight, CapturedNumInstances, CapturedNumIndices,
         CapturedGridCellSize, CapturedSpawnDensity, CapturedVertOffset,
         CapturedScaleMin, CapturedScaleMax]
        (FRHICommandListImmediate& RHICmdList)
        {
            // ------------------------------------------------------------------
            // Step 1 — reset IndirectArgs BEFORE opening the RDG graph.
            // LockBuffer is a stalling CPU-write operation and must NOT be
            // called inside an AddPass lambda (which runs inside Execute()).
            // ------------------------------------------------------------------
            RHICmdList.Transition(FRHITransitionInfo(
                CapturedIndirectBufferUAV,
                ERHIAccess::IndirectArgs,
                ERHIAccess::UAVCompute));

            uint32* Data = (uint32*)RHICmdList.LockBuffer(
                CapturedIndirectBuffer, 0, 5 * sizeof(uint32), RLM_WriteOnly);
            Data[0] = CapturedNumIndices; // IndexCountPerInstance — constant
            Data[1] = 0;                  // InstanceCount — compute atomically increments this
            Data[2] = 0;
            Data[3] = 0;
            Data[4] = 0;
            RHICmdList.UnlockBuffer(CapturedIndirectBuffer);

            // Transition instance buffer to UAV before the RDG graph too.
            RHICmdList.Transition(FRHITransitionInfo(
                CapturedInstanceBufferUAV,
                ERHIAccess::SRVMask,
                ERHIAccess::UAVCompute));

            // ------------------------------------------------------------------
            // Step 2 — RDG graph: dispatch the compute shader only.
            // ------------------------------------------------------------------
            FRDGBuilder GraphBuilder(RHICmdList, RDG_EVENT_NAME("ComputeFoliageSpawn"));

            FInstancesComputeShader::FParameters* Parameters =
                GraphBuilder.AllocParameters<FInstancesComputeShader::FParameters>();

            Parameters->SpawnInstances    = CapturedInstanceBufferUAV;
            Parameters->IndirectArgs      = CapturedIndirectBufferUAV;
            Parameters->SceneDepthTexture = CapturedDepthTexture;
            Parameters->SceneDepthSampler =
                TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
            Parameters->CameraPosition    = CapturedCameraPos;
            Parameters->CameraForward     = CapturedCameraForward;
            Parameters->CameraRight       = CapturedCameraRight;
            Parameters->CameraUp          = CapturedCameraUp;
            Parameters->OrthoWidth        = CapturedOrthoWidth;
            Parameters->OrthoHeight       = CapturedOrthoHeight;
            Parameters->NumInstances      = CapturedNumInstances;
            Parameters->MeshNumIndices    = CapturedNumIndices;
            Parameters->GridCellSize      = CapturedGridCellSize;
            Parameters->SpawnDensity      = CapturedSpawnDensity;
            Parameters->VerticalOffset    = CapturedVertOffset;
            Parameters->ScaleMin          = CapturedScaleMin;
            Parameters->ScaleMax          = CapturedScaleMax;

            TShaderMapRef<FInstancesComputeShader> ComputeShader(
                GetGlobalShaderMap(GMaxRHIFeatureLevel));

            // [numthreads(10,100,1)] = 1000 threads per group
            const uint32 GroupsY = FMath::DivideAndRoundUp(CapturedNumInstances, 1000u);
            FComputeShaderUtils::AddPass(
                GraphBuilder,
                RDG_EVENT_NAME("SpawnFoliageInstances"),
                ComputeShader,
                Parameters,
                FIntVector(1, (int32)GroupsY, 1));

            GraphBuilder.Execute();

            // ------------------------------------------------------------------
            // Step 3 — transition buffers back AFTER the graph has executed.
            // ------------------------------------------------------------------
            RHICmdList.Transition(FRHITransitionInfo(
                CapturedInstanceBufferUAV,
                ERHIAccess::UAVCompute,
                ERHIAccess::SRVMask));

            RHICmdList.Transition(FRHITransitionInfo(
                CapturedIndirectBufferUAV,
                ERHIAccess::UAVCompute,
                ERHIAccess::IndirectArgs));
        }
    );

    // NOTE: No FlushRenderingCommands() here.
    // On the tick path the game thread must not stall waiting for the render
    // thread every frame — that's exactly what causes the hang.
    // The render commands are fire-and-forget; the GPU draws whatever the most
    // recently completed dispatch wrote.
}

void UComputeShaderMeshSpawner::ExecuteComputeShader()
{
    RunComputeShader();
}

void UComputeShaderMeshSpawner::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bUpdateEveryFrame)
    {
        CaptureDepth();
        RunComputeShader();
        // No flush — enqueue and move on. The render thread processes it
        // asynchronously without stalling the game thread.
    }
}

void UComputeShaderMeshSpawner::UpdateVoxelComponentList()
{
    if (!SceneCaptureComponent)
        return;

    SceneCaptureComponent->ShowOnlyComponents.Empty();

    for (TObjectIterator<UPrimitiveComponent> It; It; ++It)
    {
        UPrimitiveComponent* Comp = *It;
        if (Comp && Comp->GetWorld() == GetWorld() &&
            Comp->ComponentHasTag(VoxelMeshComponentTag))
        {
            SceneCaptureComponent->ShowOnlyComponents.Add(Comp);
        }
    }
}

void UComputeShaderMeshSpawner::RegisterVoxelMeshComponent(UPrimitiveComponent* Component)
{
    if (SceneCaptureComponent && Component)
        SceneCaptureComponent->ShowOnlyComponents.Add(Component);
}

void UComputeShaderMeshSpawner::UnregisterVoxelMeshComponent(UPrimitiveComponent* Component)
{
    if (SceneCaptureComponent && Component)
        SceneCaptureComponent->ShowOnlyComponents.Remove(Component);
}
