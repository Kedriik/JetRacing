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

    InstancedMeshComponent = NewObject<UInstancedStaticMeshComponent>(GetOwner(), TEXT("ComputeShaderISMC"));
    InstancedMeshComponent->RegisterComponent();
    InstancedMeshComponent->AttachToComponent(GetOwner()->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
    
    if (FoliageMesh)
    {
        InstancedMeshComponent->SetStaticMesh(FoliageMesh);
    }

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

void UComputeShaderMeshSpawner::SetupDepthCapture()
{
    if (!DepthRenderTarget)
    {
        DepthRenderTarget = NewObject<UTextureRenderTarget2D>();
        DepthRenderTarget->RenderTargetFormat = RTF_R32f;
        DepthRenderTarget->InitAutoFormat(2048, 2048);
        DepthRenderTarget->UpdateResourceImmediate(true);
    }

     // Create stencil render target
    if (!StencilRenderTarget)
    {
        StencilRenderTarget = NewObject<UTextureRenderTarget2D>(this);
        StencilRenderTarget->RenderTargetFormat = RTF_R32f;
        StencilRenderTarget->InitAutoFormat(2048, 2048);
        StencilRenderTarget->UpdateResourceImmediate(true);
    }
    // Setup stencil depth capture
    SceneCaptureComponent = NewObject<USceneCaptureComponent2D>(GetOwner(), TEXT("DepthCaptureComp"));
    SceneCaptureComponent->RegisterComponent();
    SceneCaptureComponent->AttachToComponent(GetOwner()->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
    
    SceneCaptureComponent->TextureTarget = DepthRenderTarget;
    SceneCaptureComponent->CaptureSource = SCS_SceneDepth;
    SceneCaptureComponent->bCaptureEveryFrame = true;
    SceneCaptureComponent->bCaptureOnMovement = false;
    
    SceneCaptureComponent->SetWorldLocation(CameraLocation);
    SceneCaptureComponent->SetWorldRotation(CameraRotation);
    
    SceneCaptureComponent->ProjectionType = ECameraProjectionMode::Orthographic;
    SceneCaptureComponent->OrthoWidth = OrthoWidth;

    // === STENCIL CAPTURE COMPONENT ===
    StencilCaptureComponent = NewObject<USceneCaptureComponent2D>(GetOwner(), TEXT("StencilCaptureComp"));
    StencilCaptureComponent->RegisterComponent();
    StencilCaptureComponent->AttachToComponent(GetOwner()->GetRootComponent(),
        FAttachmentTransformRules::KeepRelativeTransform);

    StencilCaptureComponent->TextureTarget = StencilRenderTarget;
    StencilCaptureComponent->CaptureSource = SCS_FinalColorLDR;
    StencilCaptureComponent->bCaptureEveryFrame = true;
    StencilCaptureComponent->bCaptureOnMovement = false;

    // Apply stencil visualization material - CORRECTED METHOD
    if (StencilVisualizationMaterial)
    {
        StencilCaptureComponent->PostProcessSettings.WeightedBlendables.Array.Add(
            FWeightedBlendable(1.0f, StencilVisualizationMaterial)
        );
    }

    StencilCaptureComponent->SetWorldLocation(CameraLocation);
    StencilCaptureComponent->SetWorldRotation(CameraRotation);
    StencilCaptureComponent->ProjectionType = ECameraProjectionMode::Orthographic;
    StencilCaptureComponent->OrthoWidth = OrthoWidth;

    UE_LOG(LogTemp, Log, TEXT("Depth capture setup complete"));
}

void UComputeShaderMeshSpawner::CaptureDepth()
{
    if (SceneCaptureComponent)
    {
        SceneCaptureComponent->SetWorldLocation(CameraLocation);
        SceneCaptureComponent->SetWorldRotation(CameraRotation);
        SceneCaptureComponent->OrthoWidth = OrthoWidth;
        
        SceneCaptureComponent->CaptureScene();
    }
}

void UComputeShaderMeshSpawner::CaptureStencil()
{
    if (StencilCaptureComponent)
    {
        StencilCaptureComponent->SetWorldLocation(CameraLocation);
        StencilCaptureComponent->SetWorldRotation(CameraRotation);
        StencilCaptureComponent->OrthoWidth = OrthoWidth;
        
        StencilCaptureComponent->CaptureScene();
    }
}

void UComputeShaderMeshSpawner::CreateBuffers()
{
    if (NumInstances <= 0)
        return;

    const uint32 BufferSize = sizeof(FVector4f) * NumInstances;
    const uint32 BufferStride = sizeof(FVector4f);

    ENQUEUE_RENDER_COMMAND(CreatePositionBuffer)(
        [this, BufferStride, BufferSize](FRHICommandListImmediate& RHICmdList)
        {
            FRHIResourceCreateInfo CreateInfo(TEXT("SpawnPositionBuffer"));
            
            FBufferRHIRef TempBuffer = RHICmdList.CreateBuffer(
                BufferSize,
                BUF_UnorderedAccess | BUF_ShaderResource | BUF_StructuredBuffer,
                BufferStride,
                ERHIAccess::UAVCompute,
                CreateInfo
            );
            
            PositionBuffer = TempBuffer;
            
            if (PositionBuffer.IsValid())
            {
                PositionBufferUAV = RHICmdList.CreateUnorderedAccessView(PositionBuffer, false, false);
            }
        }
    );
    
    FlushRenderingCommands();
}

void UComputeShaderMeshSpawner::ReleaseBuffers()
{
    ENQUEUE_RENDER_COMMAND(ReleasePositionBuffer)(
        [this](FRHICommandListImmediate& RHICmdList)
        {
            PositionBufferUAV.SafeRelease();
            PositionBuffer.SafeRelease();
        }
    );
    
    FlushRenderingCommands();
}

void UComputeShaderMeshSpawner::RunComputeShader()
{
    if (!PositionBuffer.IsValid() || !PositionBufferUAV.IsValid() || !DepthRenderTarget || !StencilRenderTarget)
        return;

    FBufferRHIRef CapturedPositionBuffer = PositionBuffer;
    FUnorderedAccessViewRHIRef CapturedPositionBufferUAV = PositionBufferUAV;
    FTextureRHIRef CapturedDepthTexture = DepthRenderTarget->GetResource()->TextureRHI;
    FTextureRHIRef CapturedStencilTexture = StencilRenderTarget->GetResource()->TextureRHI;
    
    FRotationMatrix RotMatrix(CameraRotation);
    FVector Forward = RotMatrix.GetScaledAxis(EAxis::X);
    FVector Right = RotMatrix.GetScaledAxis(EAxis::Y);
    FVector Up = RotMatrix.GetScaledAxis(EAxis::Z);
    
    FVector3f CapturedCameraPos = FVector3f(CameraLocation);
    FVector3f CapturedCameraForward = FVector3f(Forward);
    FVector3f CapturedCameraRight = FVector3f(Right);
    FVector3f CapturedCameraUp = FVector3f(Up);
    
    float CapturedOrthoWidth = OrthoWidth;
    float CapturedOrthoHeight = OrthoWidth;
    uint32 CapturedNumInstances = NumInstances;
    float CapturedGridCellSize = GridCellSize;
    float CapturedSpawnDensity = SpawnDensity;
    float CapturedVerticalOffset = VerticalOffset;
    float CapturedMaxRayDistance = MaxRayDistance;
    float CapturedRaymarchStepSize = RaymarchStepSize;
    uint32 CapturedMaxRaymarchSteps = MaxRaymarchSteps;
    uint32 CapturedTargetStencilValue = TargetStencilValue;

    ENQUEUE_RENDER_COMMAND(ExecuteRaymarchingSpawn)(
        [CapturedPositionBuffer, CapturedPositionBufferUAV, CapturedDepthTexture,
         CapturedCameraPos, CapturedCameraForward, CapturedCameraRight, CapturedCameraUp,
         CapturedOrthoWidth, CapturedOrthoHeight, CapturedNumInstances, CapturedGridCellSize,
         CapturedSpawnDensity, CapturedVerticalOffset, CapturedMaxRayDistance,
         CapturedRaymarchStepSize, CapturedMaxRaymarchSteps,CapturedStencilTexture, CapturedTargetStencilValue]
        (FRHICommandListImmediate& RHICmdList)
        {
            FRDGBuilder GraphBuilder(RHICmdList, RDG_EVENT_NAME("RaymarchingFoliageSpawn"));

            FInstancesComputeShader::FParameters* Parameters = GraphBuilder.AllocParameters<FInstancesComputeShader::FParameters>();
            Parameters->SpawnPositions = CapturedPositionBufferUAV;
            Parameters->SceneDepthTexture = CapturedDepthTexture;
            Parameters->SceneDepthSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
            Parameters->StencilTexture = CapturedStencilTexture;
            Parameters->StencilSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
            Parameters->CameraPosition = CapturedCameraPos;
            Parameters->CameraForward = CapturedCameraForward;
            Parameters->CameraRight = CapturedCameraRight;
            Parameters->CameraUp = CapturedCameraUp;
            Parameters->OrthoWidth = CapturedOrthoWidth;
            Parameters->OrthoHeight = CapturedOrthoHeight;
            Parameters->NumInstances = CapturedNumInstances;
            Parameters->GridCellSize = CapturedGridCellSize;
            Parameters->SpawnDensity = CapturedSpawnDensity;
            Parameters->VerticalOffset = CapturedVerticalOffset;
            Parameters->MaxRayDistance = CapturedMaxRayDistance;
            Parameters->RaymarchStepSize = CapturedRaymarchStepSize;
            Parameters->MaxRaymarchSteps = CapturedMaxRaymarchSteps;
            Parameters->TargetStencilValue = CapturedTargetStencilValue;

            TShaderMapRef<FInstancesComputeShader> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));

            FComputeShaderUtils::AddPass(
                GraphBuilder,
                RDG_EVENT_NAME("RaymarchingSpawnPass"),
                ComputeShader,
                Parameters,
                FIntVector(FMath::DivideAndRoundUp((int32)CapturedNumInstances, 64), 1, 1)
            );

            GraphBuilder.Execute();
        }
    );

    FlushRenderingCommands();
    UpdateMeshInstances();
}

void UComputeShaderMeshSpawner::UpdateMeshInstances()
{
    if (!PositionBuffer.IsValid() || !InstancedMeshComponent)
        return;

    TArray<FVector4f> Positions;
    Positions.SetNum(NumInstances);

    FBufferRHIRef CapturedBuffer = PositionBuffer;
    int32 CapturedNumInstances = NumInstances;
    
    ENQUEUE_RENDER_COMMAND(ReadBackPositions)(
        [CapturedBuffer, &Positions, CapturedNumInstances](FRHICommandListImmediate& RHICmdList)
        {
            void* BufferData = RHICmdList.LockBuffer(CapturedBuffer, 0, sizeof(FVector4f) * CapturedNumInstances, RLM_ReadOnly);
            FMemory::Memcpy(Positions.GetData(), BufferData, sizeof(FVector4f) * CapturedNumInstances);
            RHICmdList.UnlockBuffer(CapturedBuffer);
        }
    );
    
    FlushRenderingCommands();

    InstancedMeshComponent->ClearInstances();

    for (int32 i = 0; i < NumInstances; i++)
    {
        if (Positions[i].Z < -50000.0f)
            continue;

        FTransform Transform;
        Transform.SetLocation(FVector(Positions[i].X, Positions[i].Y, Positions[i].Z));
        Transform.SetScale3D(FVector(Positions[i].W));
        
        InstancedMeshComponent->AddInstance(Transform, true);
    }
    
    InstancedMeshComponent->MarkRenderStateDirty();
}

void UComputeShaderMeshSpawner::ExecuteComputeShader()
{
    RunComputeShader();
}

void UComputeShaderMeshSpawner::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bUpdateEveryFrame)
    {
        CaptureDepth();
        CaptureStencil();
        ExecuteComputeShader();
    }
}