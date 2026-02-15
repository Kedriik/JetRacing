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

   
    // DEPTH CAPTURE COMPONENT
    SceneCaptureComponent = NewObject<USceneCaptureComponent2D>(GetOwner(), TEXT("DepthCaptureComp"));
    SceneCaptureComponent->RegisterComponent();
    SceneCaptureComponent->AttachToComponent(GetOwner()->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);

    SceneCaptureComponent->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
    
    SceneCaptureComponent->TextureTarget = DepthRenderTarget;
    SceneCaptureComponent->CaptureSource = SCS_SceneDepth;
    SceneCaptureComponent->bCaptureEveryFrame = false;
    SceneCaptureComponent->bCaptureOnMovement = false;

    // CRITICAL: Disable ALL unnecessary rendering features
   /* SceneCaptureComponent->ShowFlags.SetAntiAliasing(false);
    SceneCaptureComponent->ShowFlags.SetAtmosphere(false);
    SceneCaptureComponent->ShowFlags.SetFog(false);
    SceneCaptureComponent->ShowFlags.SetVolumetricFog(false);
    SceneCaptureComponent->ShowFlags.SetMotionBlur(false);
    SceneCaptureComponent->ShowFlags.SetBloom(false);
    SceneCaptureComponent->ShowFlags.SetAmbientOcclusion(false);
    SceneCaptureComponent->ShowFlags.SetDynamicShadows(false);
    SceneCaptureComponent->ShowFlags.SetContactShadows(false);
    SceneCaptureComponent->ShowFlags.SetDirectionalLights(false);
    SceneCaptureComponent->ShowFlags.SetPointLights(false);
    SceneCaptureComponent->ShowFlags.SetSpotLights(false);
    SceneCaptureComponent->ShowFlags.SetRectLights(false);
    SceneCaptureComponent->ShowFlags.SetSkyLighting(false);
    SceneCaptureComponent->ShowFlags.SetIndirectLightingCache(false);
    SceneCaptureComponent->ShowFlags.SetReflectionEnvironment(false);
    SceneCaptureComponent->ShowFlags.SetScreenSpaceReflections(false);
    SceneCaptureComponent->ShowFlags.SetTexturedLightProfiles(false);
    SceneCaptureComponent->ShowFlags.SetAmbientCubemap(false);
    SceneCaptureComponent->ShowFlags.SetDistanceFieldAO(false);
    SceneCaptureComponent->ShowFlags.SetLightFunctions(false);
    SceneCaptureComponent->ShowFlags.SetLightShafts(false);
    SceneCaptureComponent->ShowFlags.SetPostProcessing(false);
    SceneCaptureComponent->ShowFlags.SetTranslucency(false);
    SceneCaptureComponent->ShowFlags.SetScreenPercentage(false);
    SceneCaptureComponent->ShowFlags.SetTemporalAA(false);
    SceneCaptureComponent->ShowFlags.SetGlobalIllumination(false);
    SceneCaptureComponent->ShowFlags.SetDiffuse(false);
    SceneCaptureComponent->ShowFlags.SetSpecular(false);
    */
    
    SceneCaptureComponent->SetWorldLocation(CameraLocation);
    SceneCaptureComponent->SetWorldRotation(CameraRotation);
    
    SceneCaptureComponent->ProjectionType = ECameraProjectionMode::Orthographic;
    SceneCaptureComponent->OrthoWidth = OrthoWidth;
    
}

void UComputeShaderMeshSpawner::CaptureDepth()
{
    if (bCaptureInProgress)
        return; // Skip if previous capture still in progress

    if (!SceneCaptureComponent)
        return;

    SceneCaptureComponent->ShowOnlyComponents.Remove(nullptr);
    SceneCaptureComponent->SetWorldLocation(CameraLocation);
    SceneCaptureComponent->SetWorldRotation(CameraRotation);
    SceneCaptureComponent->OrthoWidth = OrthoWidth;

    bCaptureInProgress = true;

    // Capture async
    AsyncTask(ENamedThreads::GameThread, [this]()
        {
            if (SceneCaptureComponent)
            {
                SceneCaptureComponent->CaptureScene();
            }
            bCaptureInProgress = false;
        });
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

    if (!PositionBuffer.IsValid() || !PositionBufferUAV.IsValid() || !DepthRenderTarget)
        return;



    FBufferRHIRef CapturedPositionBuffer = PositionBuffer;
    FUnorderedAccessViewRHIRef CapturedPositionBufferUAV = PositionBufferUAV;
    FTextureRHIRef CapturedDepthTexture = DepthRenderTarget->GetResource()->TextureRHI;
    
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

    ENQUEUE_RENDER_COMMAND(ExecuteRaymarchingSpawn)(
        [CapturedPositionBuffer, CapturedPositionBufferUAV, CapturedDepthTexture,
         CapturedCameraPos, CapturedCameraForward, CapturedCameraRight, CapturedCameraUp,
         CapturedOrthoWidth, CapturedOrthoHeight, CapturedNumInstances, CapturedGridCellSize,
         CapturedSpawnDensity, CapturedVerticalOffset]
        (FRHICommandListImmediate& RHICmdList)
        {
            FRDGBuilder GraphBuilder(RHICmdList, RDG_EVENT_NAME("RaymarchingFoliageSpawn"));

            FInstancesComputeShader::FParameters* Parameters = GraphBuilder.AllocParameters<FInstancesComputeShader::FParameters>();
            Parameters->SpawnPositions = CapturedPositionBufferUAV;
            Parameters->SceneDepthTexture = CapturedDepthTexture;
            Parameters->SceneDepthSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
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

            TShaderMapRef<FInstancesComputeShader> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));

            FUintVector2 CapturedGridDimensions(8, 8);
            FIntVector ThreadGroupCount(
                10,
                100,
                1
            );

            FComputeShaderUtils::AddPass(
                GraphBuilder,
                RDG_EVENT_NAME("ComputeSpawnPass"),
                ComputeShader,
                Parameters,
                ThreadGroupCount
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
            FTransform Transform;
            Transform.SetLocation(FVector(Positions[i].X, Positions[i].Y, Positions[i].Z));
            Transform.SetScale3D(FVector(Positions[i].W));

            InstancedMeshComponent->AddInstance(Transform, true);
    };
    
    // THIS IS THE KEY - properly trigger the update
    InstancedMeshComponent->MarkRenderStateDirty();
    InstancedMeshComponent->MarkRenderTransformDirty();
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
        ExecuteComputeShader();
    }
}

void UComputeShaderMeshSpawner::UpdateVoxelComponentList()
{
    if (!SceneCaptureComponent)
        return;

    SceneCaptureComponent->ShowOnlyComponents.Empty();

    int32 ComponentCount = 0;

    for (TObjectIterator<UPrimitiveComponent> It; It; ++It)
    {
        UPrimitiveComponent* Comp = *It;

        if (Comp &&
            Comp->GetWorld() == GetWorld() &&
            Comp->ComponentHasTag(VoxelMeshComponentTag))
        {
            SceneCaptureComponent->ShowOnlyComponents.Add(Comp);
            ComponentCount++;
        }
    }

}

void UComputeShaderMeshSpawner::RegisterVoxelMeshComponent(UPrimitiveComponent* Component)
{
    if (!SceneCaptureComponent && !Component)
        return;
    
    SceneCaptureComponent->ShowOnlyComponents.Add(Component);
}

void UComputeShaderMeshSpawner::UnregisterVoxelMeshComponent(UPrimitiveComponent* Component)
{
    if (!SceneCaptureComponent && !Component)
        return;

    SceneCaptureComponent->ShowOnlyComponents.Remove(Component);
}