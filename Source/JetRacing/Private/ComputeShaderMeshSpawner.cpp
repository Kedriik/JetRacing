#include "ComputeShaderMeshSpawner.h"
#include "ComputeShaderDeclaration.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RHICommandList.h"
#include "RHIResources.h"
#include "GlobalShader.h"
#include "RenderingThread.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/SceneCaptureComponent2D.h"

UComputeShaderMeshSpawner::UComputeShaderMeshSpawner()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UComputeShaderMeshSpawner::BeginPlay()
{
    Super::BeginPlay();

    SetupDepthCapture();
    CreateBuffers();

    CaptureDepth();
    ExecuteComputeShader();
}

void UComputeShaderMeshSpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);
    ReleaseBuffers();
}

// ─────────────────────────────────────────────────────────────────────────────
// Depth capture (unchanged from original)
// ─────────────────────────────────────────────────────────────────────────────

void UComputeShaderMeshSpawner::SetupDepthCapture()
{
    if (!DepthRenderTarget)
    {
        DepthRenderTarget = NewObject<UTextureRenderTarget2D>();
        DepthRenderTarget->RenderTargetFormat = RTF_R32f;
        DepthRenderTarget->InitAutoFormat(2048, 2048);
        DepthRenderTarget->UpdateResourceImmediate(true);
    }

    SceneCaptureComponent = NewObject<USceneCaptureComponent2D>(GetOwner(), TEXT("DepthCaptureComp"));
    SceneCaptureComponent->RegisterComponent();
    SceneCaptureComponent->AttachToComponent(GetOwner()->GetRootComponent(),
                                              FAttachmentTransformRules::KeepRelativeTransform);

    SceneCaptureComponent->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
    SceneCaptureComponent->TextureTarget       = DepthRenderTarget;
    SceneCaptureComponent->CaptureSource       = SCS_SceneDepth;
    SceneCaptureComponent->bCaptureEveryFrame  = false;
    SceneCaptureComponent->bCaptureOnMovement  = false;
    SceneCaptureComponent->ShowFlags.SetDynamicShadows(false);
    SceneCaptureComponent->ShowFlags.SetCapsuleShadows(false);
    SceneCaptureComponent->ShowFlags.SetContactShadows(false);
    SceneCaptureComponent->ShowFlags.SetLighting(false);
    SceneCaptureComponent->ShowFlags.SetGlobalIllumination(false);
    SceneCaptureComponent->ShowFlags.SetReflectionEnvironment(false);
    SceneCaptureComponent->ShowFlags.SetScreenSpaceAO(false);
    SceneCaptureComponent->ShowFlags.SetAmbientOcclusion(false);
    SceneCaptureComponent->ShowFlags.SetPostProcessing(false);
    SceneCaptureComponent->ShowFlags.SetAtmosphere(false);
    SceneCaptureComponent->ShowFlags.SetFog(false);
    SceneCaptureComponent->ShowFlags.SetVolumetricFog(false);
    SceneCaptureComponent->ShowFlags.SetSkyLighting(false);

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

// ─────────────────────────────────────────────────────────────────────────────
// Buffer management – now also creates an SRV for Niagara
// ─────────────────────────────────────────────────────────────────────────────

void UComputeShaderMeshSpawner::CreateBuffers()
{
    if (NumInstances <= 0)
        return;

    const uint32 BufferSize   = sizeof(FVector4f) * NumInstances;
    const uint32 BufferStride = sizeof(FVector4f);

    // Capture member pointers so the lambda can write back results
    FBufferRHIRef*              OutBuffer    = &PositionBuffer;
    FUnorderedAccessViewRHIRef* OutUAV       = &PositionBufferUAV;
    FShaderResourceViewRHIRef*  OutSRV       = &PositionBufferSRV;

    ENQUEUE_RENDER_COMMAND(CreatePositionBuffer)(
        [OutBuffer, OutUAV, OutSRV, BufferSize, BufferStride](FRHICommandListImmediate& RHICmdList)
        {
            FRHIResourceCreateInfo CreateInfo(TEXT("SpawnPositionBuffer"));

            *OutBuffer = RHICmdList.CreateBuffer(
                BufferSize,
                BUF_UnorderedAccess | BUF_ShaderResource | BUF_StructuredBuffer,
                BufferStride,
                ERHIAccess::UAVCompute,
                CreateInfo
            );

            if (OutBuffer->IsValid())
            {
                // UAV for the compute shader write pass
                *OutUAV = RHICmdList.CreateUnorderedAccessView(*OutBuffer, false, false);

                // SRV for Niagara's read-only access.
                // Use the structured buffer descriptor — PF_Unknown asserts on UE5.5+.
                FRHIViewDesc::FBufferSRV::FInitializer SRVDesc =
                    FRHIViewDesc::CreateBufferSRV()
                        .SetType(FRHIViewDesc::EBufferType::Structured);
                *OutSRV = RHICmdList.CreateShaderResourceView(*OutBuffer, SRVDesc);
            }
        }
    );

    FlushRenderingCommands();
}

void UComputeShaderMeshSpawner::ReleaseBuffers()
{
    ENQUEUE_RENDER_COMMAND(ReleasePositionBuffer)(
        [this](FRHICommandListImmediate&)
        {
            PositionBufferSRV.SafeRelease();
            PositionBufferUAV.SafeRelease();
            PositionBuffer.SafeRelease();
        }
    );

    FlushRenderingCommands();
}

// ─────────────────────────────────────────────────────────────────────────────
// Compute shader dispatch (no readback, no ISMC update)
// ─────────────────────────────────────────────────────────────────────────────

void UComputeShaderMeshSpawner::RunComputeShader()
{
    if (!PositionBuffer.IsValid() || !PositionBufferUAV.IsValid() || !DepthRenderTarget)
        return;

    FBufferRHIRef              CapturedBuffer    = PositionBuffer;
    FUnorderedAccessViewRHIRef CapturedUAV       = PositionBufferUAV;
    FTextureRHIRef             CapturedDepth     = DepthRenderTarget->GetResource()->TextureRHI;

    FRotationMatrix RotMatrix(CameraRotation);
    FVector3f CapturedCameraPos     = FVector3f(CameraLocation);
    FVector3f CapturedCameraForward = FVector3f(RotMatrix.GetScaledAxis(EAxis::X));
    FVector3f CapturedCameraRight   = FVector3f(RotMatrix.GetScaledAxis(EAxis::Y));
    FVector3f CapturedCameraUp      = FVector3f(RotMatrix.GetScaledAxis(EAxis::Z));

    float    CapturedOrthoWidth    = OrthoWidth;
    float    CapturedOrthoHeight   = OrthoWidth;
    uint32   CapturedNumInstances  = NumInstances;
    float    CapturedGridCellSize  = GridCellSize;
    float    CapturedSpawnDensity  = SpawnDensity;
    float    CapturedVerticalOffset = VerticalOffset;

    ENQUEUE_RENDER_COMMAND(ExecuteSpawnComputeShader)(
        [CapturedBuffer, CapturedUAV, CapturedDepth,
         CapturedCameraPos, CapturedCameraForward, CapturedCameraRight, CapturedCameraUp,
         CapturedOrthoWidth, CapturedOrthoHeight, CapturedNumInstances, CapturedGridCellSize,
         CapturedSpawnDensity, CapturedVerticalOffset]
        (FRHICommandListImmediate& RHICmdList)
        {
            FRDGBuilder GraphBuilder(RHICmdList, RDG_EVENT_NAME("SpawnPositionCompute"));
            const uint32 GroupsY = FMath::DivideAndRoundUp(CapturedNumInstances, 1000u);
            FInstancesComputeShader::FParameters* Parameters =
                GraphBuilder.AllocParameters<FInstancesComputeShader::FParameters>();

            Parameters->SpawnPositions      = CapturedUAV;
            Parameters->SceneDepthTexture   = CapturedDepth;
            Parameters->SceneDepthSampler   = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
            Parameters->CameraPosition      = CapturedCameraPos;
            Parameters->CameraForward       = CapturedCameraForward;
            Parameters->CameraRight         = CapturedCameraRight;
            Parameters->CameraUp            = CapturedCameraUp;
            Parameters->OrthoWidth          = CapturedOrthoWidth;
            Parameters->OrthoHeight         = CapturedOrthoHeight;
            Parameters->NumInstances        = CapturedNumInstances;
            Parameters->GridCellSize        = CapturedGridCellSize;
            Parameters->SpawnDensity        = CapturedSpawnDensity;
            Parameters->VerticalOffset      = CapturedVerticalOffset;

            TShaderMapRef<FInstancesComputeShader> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));

            FComputeShaderUtils::AddPass(
                GraphBuilder,
                RDG_EVENT_NAME("SpawnPositionPass"),
                ComputeShader,
                Parameters,
                FIntVector(1, (int32)GroupsY, 1)
            );

            GraphBuilder.Execute();

            // Transition the buffer to SRV so Niagara can read it this frame
            // without a pipeline stall.
            RHICmdList.Transition(FRHITransitionInfo(CapturedBuffer, ERHIAccess::UAVCompute, ERHIAccess::SRVMask));
        }
    );

    // No FlushRenderingCommands here – we intentionally let the RT run ahead.
    // Niagara will read the SRV on the same render thread, after this command,
    // so ordering is guaranteed without a CPU stall.
}

void UComputeShaderMeshSpawner::ExecuteComputeShader()
{
    RunComputeShader();
}

// ─────────────────────────────────────────────────────────────────────────────
// Tick
// ─────────────────────────────────────────────────────────────────────────────

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

// ─────────────────────────────────────────────────────────────────────────────
// Voxel component list helpers (unchanged)
// ─────────────────────────────────────────────────────────────────────────────

void UComputeShaderMeshSpawner::UpdateVoxelComponentList()
{
    if (!SceneCaptureComponent)
        return;

    SceneCaptureComponent->ShowOnlyComponents.Empty();

    for (TObjectIterator<UPrimitiveComponent> It; It; ++It)
    {
        UPrimitiveComponent* Comp = *It;
        if (Comp && Comp->GetWorld() == GetWorld() && Comp->ComponentHasTag(VoxelMeshComponentTag))
            SceneCaptureComponent->ShowOnlyComponents.Add(Comp);
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
