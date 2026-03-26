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

    ENQUEUE_RENDER_COMMAND(DispatchFoliageComputeShader)(
        [CapturedInstanceBufferUAV,
         CapturedIndirectBuffers, CapturedIndirectBufferUAVs,
         CapturedDepthTexture,
         CapturedCameraPos, CapturedCameraForward, CapturedCameraRight, CapturedCameraUp,
         CapturedOrthoWidth, CapturedOrthoHeight, CapturedNumInstances, CapturedNumIndices,
         CapturedGridCellSize, CapturedSpawnDensity, CapturedVertOffset,
         CapturedScaleMin, CapturedScaleMax]
        (FRHICommandListImmediate& RHICmdList)
        {
            // ------------------------------------------------------------------
            // Step 1 — reset all IndirectArgs buffers before the graph.
            // Each section gets its own IndexCount (pre-baked) and InstanceCount=0.
            // Buffer[0] is the one the compute shader atomically increments.
            // ------------------------------------------------------------------
            for (int32 i = 0; i < CapturedIndirectBuffers.Num(); i++)
            {
                RHICmdList.Transition(FRHITransitionInfo(
                    CapturedIndirectBufferUAVs[i],
                    ERHIAccess::IndirectArgs,
                    ERHIAccess::UAVCompute));

                // Read the current args to preserve IndexCount and StartIndex
                // which were baked at allocation time, then rewrite with InstanceCount=0.
                uint32 SavedArgs[5];
                {
                    const uint32* Src = (const uint32*)RHICmdList.LockBuffer(
                        CapturedIndirectBuffers[i], 0, 5 * sizeof(uint32), RLM_ReadOnly);
                    FMemory::Memcpy(SavedArgs, Src, 5 * sizeof(uint32));
                    RHICmdList.UnlockBuffer(CapturedIndirectBuffers[i]);
                }
                {
                    uint32* Dst = (uint32*)RHICmdList.LockBuffer(
                        CapturedIndirectBuffers[i], 0, 5 * sizeof(uint32), RLM_WriteOnly);
                    Dst[0] = SavedArgs[0]; // IndexCountPerInstance — preserved
                    Dst[1] = 0;            // InstanceCount — reset for atomic increment
                    Dst[2] = SavedArgs[2]; // StartIndexLocation — preserved
                    Dst[3] = 0;
                    Dst[4] = 0;
                    RHICmdList.UnlockBuffer(CapturedIndirectBuffers[i]);
                }
            }

            // Transition instance buffer to UAV
            RHICmdList.Transition(FRHITransitionInfo(
                CapturedInstanceBufferUAV,
                ERHIAccess::SRVMask,
                ERHIAccess::UAVCompute));

            // ------------------------------------------------------------------
            // Step 2 — dispatch compute shader, writing to buffer[0] only.
            // All sections render the same instances so InstanceCount is shared.
            // ------------------------------------------------------------------
            FRDGBuilder GraphBuilder(RHICmdList, RDG_EVENT_NAME("ComputeFoliageSpawn"));

            FInstancesComputeShader::FParameters* Parameters =
                GraphBuilder.AllocParameters<FInstancesComputeShader::FParameters>();

            Parameters->SpawnInstances    = CapturedInstanceBufferUAV;
            Parameters->IndirectArgs      = CapturedIndirectBufferUAVs[0]; // InstanceCount written here
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

            const uint32 GroupsY = FMath::DivideAndRoundUp(CapturedNumInstances, 1000u);
            FComputeShaderUtils::AddPass(
                GraphBuilder,
                RDG_EVENT_NAME("SpawnFoliageInstances"),
                ComputeShader,
                Parameters,
                FIntVector(1, (int32)GroupsY, 1));

            GraphBuilder.Execute();

            // ------------------------------------------------------------------
            // Step 3 — propagate InstanceCount from buffer[0] to all other
            // section buffers, then transition everything back for drawing.
            // ------------------------------------------------------------------
            if (CapturedIndirectBuffers.Num() > 1)
            {
                // Read InstanceCount written by the compute shader from buffer[0]
                RHICmdList.Transition(FRHITransitionInfo(
                    CapturedIndirectBufferUAVs[0],
                    ERHIAccess::UAVCompute,
                    ERHIAccess::SRVGraphics));

                const uint32* Src = (const uint32*)RHICmdList.LockBuffer(
                    CapturedIndirectBuffers[0], 0, 5 * sizeof(uint32), RLM_ReadOnly);
                uint32 InstanceCount = Src[1];
                RHICmdList.UnlockBuffer(CapturedIndirectBuffers[0]);

                // Write InstanceCount into all remaining section buffers,
                // preserving their baked IndexCount and StartIndex.
                for (int32 i = 1; i < CapturedIndirectBuffers.Num(); i++)
                {
                    uint32 SavedArgs[5];
                    {
                        const uint32* SectionSrc = (const uint32*)RHICmdList.LockBuffer(
                            CapturedIndirectBuffers[i], 0, 5 * sizeof(uint32), RLM_ReadOnly);
                        FMemory::Memcpy(SavedArgs, SectionSrc, 5 * sizeof(uint32));
                        RHICmdList.UnlockBuffer(CapturedIndirectBuffers[i]);
                    }
                    {
                        uint32* Dst = (uint32*)RHICmdList.LockBuffer(
                            CapturedIndirectBuffers[i], 0, 5 * sizeof(uint32), RLM_WriteOnly);
                        Dst[0] = SavedArgs[0]; // IndexCountPerInstance — preserved
                        Dst[1] = InstanceCount;
                        Dst[2] = SavedArgs[2]; // StartIndexLocation — preserved
                        Dst[3] = 0;
                        Dst[4] = 0;
                        RHICmdList.UnlockBuffer(CapturedIndirectBuffers[i]);
                    }

                    RHICmdList.Transition(FRHITransitionInfo(
                        CapturedIndirectBufferUAVs[i],
                        ERHIAccess::UAVCompute,
                        ERHIAccess::IndirectArgs));
                }

                // Transition buffer[0] to IndirectArgs
                RHICmdList.Transition(FRHITransitionInfo(
                    CapturedIndirectBufferUAVs[0],
                    ERHIAccess::SRVGraphics,
                    ERHIAccess::IndirectArgs));
            }
            else
            {
                RHICmdList.Transition(FRHITransitionInfo(
                    CapturedIndirectBufferUAVs[0],
                    ERHIAccess::UAVCompute,
                    ERHIAccess::IndirectArgs));
            }

            // Transition instance buffer back to SRV for the vertex factory
            RHICmdList.Transition(FRHITransitionInfo(
                CapturedInstanceBufferUAV,
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
