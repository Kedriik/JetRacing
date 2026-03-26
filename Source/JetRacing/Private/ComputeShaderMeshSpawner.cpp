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

    // Create the indirect instancing component on the same actor,
    // exactly like the original code created UInstancedStaticMeshComponent.
    IndirectInstancingComponent = NewObject<UComputeDrivenIndirectInstancingComponent>(
        GetOwner(), TEXT("ComputeDrivenISMC"));
    IndirectInstancingComponent->RegisterComponent();
    IndirectInstancingComponent->AttachToComponent(
        GetOwner()->GetRootComponent(),
        FAttachmentTransformRules::KeepRelativeTransform);

    IndirectInstancingComponent->MaxInstances = NumInstances;

    if (FoliageMesh)
    {
        IndirectInstancingComponent->Mesh = FoliageMesh;
    }
    if (FoliageMaterial)
    {
        IndirectInstancingComponent->Material = FoliageMaterial;
    }

    SetupDepthCapture();
    UpdateVoxelComponentList();

    CaptureDepth();
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

    bCaptureInProgress = true;

    AsyncTask(ENamedThreads::GameThread, [this]()
    {
        if (SceneCaptureComponent)
            SceneCaptureComponent->CaptureScene();
        bCaptureInProgress = false;
    });
}

// ---------------------------------------------------------------------------
// Compute dispatch
// ---------------------------------------------------------------------------
void UComputeShaderMeshSpawner::RunComputeShader()
{
    if (!IndirectInstancingComponent)
        return;

    if (!DepthRenderTarget)
        return;

    // GPU buffers are allocated by the scene proxy in CreateRenderThreadResources.
    // They won't be valid until after the first render frame — guard here.
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
            FRDGBuilder GraphBuilder(RHICmdList, RDG_EVENT_NAME("ComputeFoliageSpawn"));

            // ------------------------------------------------------------------
            // Pass 1 — reset IndirectArgs, write IndexCount into slot 0,
            //           zero slot 1 (InstanceCount) so the atomic starts at 0.
            // ------------------------------------------------------------------
            AddPass(GraphBuilder,
                RDG_EVENT_NAME("ClearIndirectArgs"),
                [CapturedIndirectBuffer, CapturedIndirectBufferUAV, CapturedNumIndices]
                (FRHICommandList& InRHICmdList)
                {
                    InRHICmdList.Transition(FRHITransitionInfo(
                        CapturedIndirectBufferUAV,
                        ERHIAccess::IndirectArgs,
                        ERHIAccess::UAVCompute));

                    uint32* Data = (uint32*)InRHICmdList.LockBuffer(
                        CapturedIndirectBuffer, 0, 5 * sizeof(uint32), RLM_WriteOnly);
                    Data[0] = CapturedNumIndices; // IndexCountPerInstance
                    Data[1] = 0;                  // InstanceCount — incremented by compute
                    Data[2] = 0;
                    Data[3] = 0;
                    Data[4] = 0;
                    InRHICmdList.UnlockBuffer(CapturedIndirectBuffer);
                });

            // ------------------------------------------------------------------
            // Pass 2 — transition instance buffer to UAV
            // ------------------------------------------------------------------
            AddPass(GraphBuilder,
                RDG_EVENT_NAME("TransitionInstanceBufferToUAV"),
                [CapturedInstanceBufferUAV](FRHICommandList& InRHICmdList)
                {
                    InRHICmdList.Transition(FRHITransitionInfo(
                        CapturedInstanceBufferUAV,
                        ERHIAccess::SRVMask,
                        ERHIAccess::UAVCompute));
                });

            // ------------------------------------------------------------------
            // Pass 3 — dispatch compute shader
            // ------------------------------------------------------------------
            FInstancesComputeShader::FParameters* Parameters =
                GraphBuilder.AllocParameters<FInstancesComputeShader::FParameters>();

            Parameters->SpawnInstances       = CapturedInstanceBufferUAV;
            Parameters->IndirectArgs         = CapturedIndirectBufferUAV;
            Parameters->SceneDepthTexture    = CapturedDepthTexture;
            Parameters->SceneDepthSampler    =
                TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
            Parameters->CameraPosition       = CapturedCameraPos;
            Parameters->CameraForward        = CapturedCameraForward;
            Parameters->CameraRight          = CapturedCameraRight;
            Parameters->CameraUp             = CapturedCameraUp;
            Parameters->OrthoWidth           = CapturedOrthoWidth;
            Parameters->OrthoHeight          = CapturedOrthoHeight;
            Parameters->NumInstances         = CapturedNumInstances;
            Parameters->MeshNumIndices       = CapturedNumIndices;
            Parameters->GridCellSize         = CapturedGridCellSize;
            Parameters->SpawnDensity         = CapturedSpawnDensity;
            Parameters->VerticalOffset       = CapturedVertOffset;
            Parameters->ScaleMin             = CapturedScaleMin;
            Parameters->ScaleMax             = CapturedScaleMax;

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

            // ------------------------------------------------------------------
            // Pass 4 — transition buffers back for draw
            // ------------------------------------------------------------------
            AddPass(GraphBuilder,
                RDG_EVENT_NAME("TransitionBuffersToRead"),
                [CapturedInstanceBufferUAV, CapturedIndirectBufferUAV]
                (FRHICommandList& InRHICmdList)
                {
                    InRHICmdList.Transition(FRHITransitionInfo(
                        CapturedInstanceBufferUAV,
                        ERHIAccess::UAVCompute,
                        ERHIAccess::SRVMask));

                    InRHICmdList.Transition(FRHITransitionInfo(
                        CapturedIndirectBufferUAV,
                        ERHIAccess::UAVCompute,
                        ERHIAccess::IndirectArgs));
                });

            GraphBuilder.Execute();
        }
    );

    FlushRenderingCommands();

    IndirectInstancingComponent->MarkRenderStateDirty();
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
        ExecuteComputeShader();
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
