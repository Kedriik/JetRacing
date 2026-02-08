#include "ComputeShaderMeshSpawner.h"
#include "ComputeShaderDeclaration.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RHICommandList.h"
#include "RHIResources.h"
#include "GlobalShader.h"
#include "RenderingThread.h"

UComputeShaderMeshSpawner::UComputeShaderMeshSpawner()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UComputeShaderMeshSpawner::BeginPlay()
{
    Super::BeginPlay();

    InstancedMeshComponent = NewObject<UInstancedStaticMeshComponent>(GetOwner(), TEXT("ComputeShaderISMC"));
    InstancedMeshComponent->RegisterComponent();
    InstancedMeshComponent->AttachToComponent(
        GetOwner()->GetRootComponent(),
        FAttachmentTransformRules::KeepRelativeTransform
    );

    if (MeshToSpawn)
    {
        InstancedMeshComponent->SetStaticMesh(MeshToSpawn);
    }

    CreateBuffers();
    ExecuteComputeShader();
}

void UComputeShaderMeshSpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);
    ReleaseBuffers();
}

void UComputeShaderMeshSpawner::CreateBuffers()
{
    if (NumInstances <= 0)
    {
        return;
    }

    const uint32 BufferSize = sizeof(FVector4f) * NumInstances;
    const uint32 BufferStride = sizeof(FVector4f);

    ENQUEUE_RENDER_COMMAND(CreatePositionBuffer)(
        [this, BufferStride, BufferSize](FRHICommandListImmediate& RHICmdList)
        {
            FRHIResourceCreateInfo CreateInfo(TEXT("SpawnPositionBuffer"));

            // UE 5.5 proper API - CreateBuffer with descriptor
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

void UComputeShaderMeshSpawner::RunComputeShader(float Time)
{
    if (!PositionBuffer.IsValid() || !PositionBufferUAV.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("ComputeShaderMeshSpawner: Buffers not valid"));
        return;
    }

    FBufferRHIRef CapturedPositionBuffer = PositionBuffer;
    FUnorderedAccessViewRHIRef CapturedPositionBufferUAV = PositionBufferUAV;
    float CapturedTime = Time;
    float CapturedSpawnRadius = SpawnRadius;
    uint32 CapturedNumInstances = NumInstances;

    ENQUEUE_RENDER_COMMAND(ExecuteMyComputeShader)(
        [CapturedPositionBuffer, CapturedPositionBufferUAV, CapturedTime, CapturedSpawnRadius, CapturedNumInstances]
    (FRHICommandListImmediate& RHICmdList)
        {
            FRDGBuilder GraphBuilder(RHICmdList, RDG_EVENT_NAME("MeshSpawnCompute"));

            FInstancesComputeShader::FParameters* Parameters = GraphBuilder.AllocParameters<FInstancesComputeShader::FParameters>();
            Parameters->SpawnPositions = CapturedPositionBufferUAV;
            Parameters->Time = CapturedTime;
            Parameters->SpawnRadius = CapturedSpawnRadius;
            Parameters->NumInstances = CapturedNumInstances;

            TShaderMapRef<FInstancesComputeShader> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));

            FComputeShaderUtils::AddPass(
                GraphBuilder,
                RDG_EVENT_NAME("MyMeshSpawnComputePass"),
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
    {
        return;
    }

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
        Transform.SetScale3D(FVector(1.0f));

        InstancedMeshComponent->AddInstance(Transform, true);
    }

    InstancedMeshComponent->MarkRenderStateDirty();
}

void UComputeShaderMeshSpawner::ExecuteComputeShader()
{
    if (GetWorld())
    {
        float Time = GetWorld()->GetTimeSeconds();
        RunComputeShader(Time);
    }
}

void UComputeShaderMeshSpawner::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bUpdateEveryFrame)
    {
        ExecuteComputeShader();
    }
}