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
    // Materials are read automatically from the mesh's material slots —
    // no manual assignment needed.

    SetupDepthCapture();
    UpdateVoxelComponentList();

    // Let the renderer tick once so CreateRenderThreadResources allocates
    // the GPU buffers before we dispatch into them.
    FlushRenderingCommands();

    CaptureDepth();
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

    SceneCaptureComponent->CaptureScene();
}

// ---------------------------------------------------------------------------
// Compute dispatch
// ---------------------------------------------------------------------------
void UComputeShaderMeshSpawner::RunComputeShader()
{
    if (!IndirectInstancingComponent || !DepthRenderTarget)
        return;

    // Keep the component's bounds centred on where instances will actually live
    // (the capture camera position).  CalcBounds uses both fields to build a
    // sphere that covers the entire possible instance grid.
    IndirectInstancingComponent->GridCellSize  = GridCellSize;
    IndirectInstancingComponent->CaptureOrigin = CameraLocation;
    IndirectInstancingComponent->UpdateBounds();  // re-evaluates CalcBounds, pushes to renderer

    FBufferRHIRef              CapturedInstanceBuffer    = IndirectInstancingComponent->GpuInstanceBuffer;
    FUnorderedAccessViewRHIRef CapturedInstanceBufferUAV = IndirectInstancingComponent->GpuInstanceBufferUAV;
    int32                      CapturedMeshNumIndices    = IndirectInstancingComponent->GpuMeshNumIndices;

    // All section IndirectArgs buffers — the compute shader writes InstanceCount
    // to buffer[0], then we copy it to the rest.
    TArray<FBufferRHIRef>              CapturedIndirectBuffers   = IndirectInstancingComponent->GpuIndirectArgsBuffers;
    TArray<FUnorderedAccessViewRHIRef> CapturedIndirectBufferUAVs = IndirectInstancingComponent->GpuIndirectArgsBufferUAVs;

    if (!CapturedInstanceBuffer.IsValid() || !CapturedInstanceBufferUAV.IsValid() ||
        CapturedIndirectBuffers.Num() == 0 || CapturedMeshNumIndices <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("UComputeShaderMeshSpawner: GPU buffers not ready yet."));
        return;
    }

    for (const FBufferRHIRef& Buf : CapturedIndirectBuffers)
    {
        if (!Buf.IsValid())
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

    TArray<FBufferRHIRef> CapturedResetBuffers = IndirectInstancingComponent->GpuIndirectArgsResetBuffers;
    if (CapturedResetBuffers.Num() != CapturedIndirectBuffers.Num())
    {
        UE_LOG(LogTemp, Warning, TEXT("UComputeShaderMeshSpawner: Reset buffers not ready yet."));
        return;
    }

    ENQUEUE_RENDER_COMMAND(DispatchFoliageComputeShader)(
        [CapturedInstanceBufferUAV,
         CapturedIndirectBuffers, CapturedIndirectBufferUAVs,
         CapturedResetBuffers,
         CapturedDepthTexture,
         CapturedCameraPos, CapturedCameraForward, CapturedCameraRight, CapturedCameraUp,
         CapturedOrthoWidth, CapturedOrthoHeight, CapturedNumInstances, CapturedNumIndices,
         CapturedGridCellSize, CapturedSpawnDensity, CapturedVertOffset,
         CapturedScaleMin, CapturedScaleMax]
        (FRHICommandListImmediate& RHICmdList)
        {
            // ------------------------------------------------------------------
            // All transitions use the appropriate handle:
            //   - CopyDest / CopySrc  → transition the BUFFER (FRHIBuffer*)
            //   - UAVCompute          → transition the UAV    (FRHIUnorderedAccessView*)
            //   - IndirectArgs        → transition the BUFFER (FRHIBuffer*)
            //   - SRVMask             → transition the UAV    (FRHIUnorderedAccessView*)
            // D3D12 transitions operate on the underlying resource; using the wrong
            // view handle for a state that doesn't match the view type causes
            // DXGI_ERROR_INVALID_CALL / device removal.
            // ------------------------------------------------------------------

            // Step 1 — Reset each IndirectArgs buffer from the pre-baked reset buffer.
            for (int32 i = 0; i < CapturedIndirectBuffers.Num(); i++)
            {
                // Transition the BUFFER to CopyDest (not the UAV view).
                RHICmdList.Transition(FRHITransitionInfo(
                    CapturedIndirectBuffers[i].GetReference(),
                    ERHIAccess::IndirectArgs,
                    ERHIAccess::CopyDest));

                RHICmdList.CopyBufferRegion(
                    CapturedIndirectBuffers[i],  0,
                    CapturedResetBuffers[i],      0,
                    5 * sizeof(uint32));

                // Transition via UAV handle to UAVCompute (correct for UAV states).
                RHICmdList.Transition(FRHITransitionInfo(
                    CapturedIndirectBufferUAVs[i].GetReference(),
                    ERHIAccess::CopyDest,
                    ERHIAccess::UAVCompute));
            }

            // Step 2 — Transition instance buffer to UAV.
            RHICmdList.Transition(FRHITransitionInfo(
                CapturedInstanceBufferUAV.GetReference(),
                ERHIAccess::SRVMask,
                ERHIAccess::UAVCompute));

            // Step 3 — Dispatch the foliage compute shader.
            // Writes instance matrices into the instance buffer and atomically
            // increments IndirectArgs[0][1] (InstanceCount).
            // FComputeShaderUtils::Dispatch takes FRHICommandList& (base class).
            {
                TShaderMapRef<FInstancesComputeShader> ComputeShader(
                    GetGlobalShaderMap(GMaxRHIFeatureLevel));

                FInstancesComputeShader::FParameters Params;
                Params.SpawnInstances    = CapturedInstanceBufferUAV;
                Params.IndirectArgs      = CapturedIndirectBufferUAVs[0];
                Params.SceneDepthTexture = CapturedDepthTexture;
                Params.SceneDepthSampler =
                    TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
                Params.CameraPosition    = CapturedCameraPos;
                Params.CameraForward     = CapturedCameraForward;
                Params.CameraRight       = CapturedCameraRight;
                Params.CameraUp          = CapturedCameraUp;
                Params.OrthoWidth        = CapturedOrthoWidth;
                Params.OrthoHeight       = CapturedOrthoHeight;
                Params.NumInstances      = CapturedNumInstances;
                Params.MeshNumIndices    = CapturedNumIndices;
                Params.GridCellSize      = CapturedGridCellSize;
                Params.SpawnDensity      = CapturedSpawnDensity;
                Params.VerticalOffset    = CapturedVertOffset;
                Params.ScaleMin          = CapturedScaleMin;
                Params.ScaleMax          = CapturedScaleMax;

                const uint32 GroupsY = FMath::DivideAndRoundUp(CapturedNumInstances, 1000u);

                FComputeShaderUtils::Dispatch(
                    static_cast<FRHICommandList&>(RHICmdList),
                    ComputeShader, Params,
                    FIntVector(1, (int32)GroupsY, 1));
            }

            // Step 4 — Propagate InstanceCount from IndirectArgs[0] to other sections.
            if (CapturedIndirectBuffers.Num() > 1)
            {
                // Transition buffer[0] UAVCompute → CopySrc (buffer handle).
                RHICmdList.Transition(FRHITransitionInfo(
                    CapturedIndirectBuffers[0].GetReference(),
                    ERHIAccess::UAVCompute,
                    ERHIAccess::CopySrc));

                for (int32 i = 1; i < CapturedIndirectBuffers.Num(); i++)
                {
                    // buffer[i] is still in UAVCompute from Step 1.
                    RHICmdList.Transition(FRHITransitionInfo(
                        CapturedIndirectBuffers[i].GetReference(),
                        ERHIAccess::UAVCompute,
                        ERHIAccess::CopyDest));

                    RHICmdList.CopyBufferRegion(
                        CapturedIndirectBuffers[i], 4,
                        CapturedIndirectBuffers[0], 4,
                        sizeof(uint32));

                    RHICmdList.Transition(FRHITransitionInfo(
                        CapturedIndirectBuffers[i].GetReference(),
                        ERHIAccess::CopyDest,
                        ERHIAccess::IndirectArgs));
                }

                RHICmdList.Transition(FRHITransitionInfo(
                    CapturedIndirectBuffers[0].GetReference(),
                    ERHIAccess::CopySrc,
                    ERHIAccess::IndirectArgs));
            }
            else
            {
                // Single section: UAVCompute → IndirectArgs (buffer handle).
                RHICmdList.Transition(FRHITransitionInfo(
                    CapturedIndirectBuffers[0].GetReference(),
                    ERHIAccess::UAVCompute,
                    ERHIAccess::IndirectArgs));
            }

            // Step 5 — Transition instance buffer back to SRV.
            RHICmdList.Transition(FRHITransitionInfo(
                CapturedInstanceBufferUAV.GetReference(),
                ERHIAccess::UAVCompute,
                ERHIAccess::SRVMask));
        }
    );
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
