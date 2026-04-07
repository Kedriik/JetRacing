#pragma once

#include "CoreMinimal.h"
#include "NiagaraCommon.h"
#include "NiagaraDataInterface.h"
#include "NiagaraShaderParametersBuilder.h"
#include "RHI.h"
#include "RHIResources.h"

#include "NiagaraDataInterfaceSpawnBuffer.generated.h"

class UComputeShaderMeshSpawner;

// ─────────────────────────────────────────────────────────────────────────────
// Shader parameter struct
// ─────────────────────────────────────────────────────────────────────────────
BEGIN_SHADER_PARAMETER_STRUCT(FSpawnBufferShaderParameters, )
    SHADER_PARAMETER(int32,           NumInstances)
    SHADER_PARAMETER_SRV(StructuredBuffer<float4>, SpawnPositions)
END_SHADER_PARAMETER_STRUCT()

// ─────────────────────────────────────────────────────────────────────────────
// Render-thread proxy
// ─────────────────────────────────────────────────────────────────────────────
struct FSpawnBufferProxy : public FNiagaraDataInterfaceProxy
{
    FShaderResourceViewRHIRef SpawnPositionsSRV;
    int32                     NumInstances = 0;

    // Dummy structured buffer — safe fallback for frames before the real SRV
    // arrives. Must be a StructuredBuffer, not a plain vertex buffer.
    FBufferRHIRef             DummyBuffer;
    FShaderResourceViewRHIRef DummySRV;

    void InitDummyBuffer_RenderThread(FRHICommandListImmediate& RHICmdList)
    {
        if (DummySRV.IsValid())
            return;

        // One zero-filled element. BulkData must outlive CreateBuffer so we
        // keep it on the stack — CreateBuffer copies it synchronously.
        FVector4f ZeroElement(0.f, 0.f, 0.f, 0.f);
        TResourceArray<FVector4f, VERTEXBUFFER_ALIGNMENT> InitData;
        InitData.Add(ZeroElement);

        FRHIResourceCreateInfo Info(TEXT("SpawnBufferDummy"), &InitData);

        DummyBuffer = RHICmdList.CreateBuffer(
            sizeof(FVector4f),
            BUF_ShaderResource | BUF_StructuredBuffer | BUF_Static,
            sizeof(FVector4f),
            ERHIAccess::SRVMask,
            Info);

        if (DummyBuffer.IsValid())
        {
            FRHIViewDesc::FBufferSRV::FInitializer SRVDesc =
                FRHIViewDesc::CreateBufferSRV()
                    .SetType(FRHIViewDesc::EBufferType::Structured);
            DummySRV = RHICmdList.CreateShaderResourceView(DummyBuffer, SRVDesc);
        }
    }

    virtual void ConsumePerInstanceDataFromGameThread(
        void* PerInstanceData, const FNiagaraSystemInstanceID& Instance) override {}
    virtual int32 PerInstanceDataPassedToRenderThreadSize() const override { return 0; }
};

// ─────────────────────────────────────────────────────────────────────────────
// Per-instance game-thread data
// ─────────────────────────────────────────────────────────────────────────────
struct FSpawnBufferInstanceData
{
    TWeakObjectPtr<UComputeShaderMeshSpawner> Spawner;
};

// ─────────────────────────────────────────────────────────────────────────────
// The Data Interface UObject
// ─────────────────────────────────────────────────────────────────────────────
UCLASS(EditInlineNew, Category = "Spawning", meta = (DisplayName = "Spawn Position Buffer"))
class JETRACING_API UNiagaraDataInterfaceSpawnBuffer : public UNiagaraDataInterface
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning",
        meta = (AllowedClasses = "Actor", AllowAnyActor))
    TObjectPtr<AActor> SpawnerActor;

    virtual void PostInitProperties() override;

    virtual void GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions) override;
    virtual void GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo,
                                       void* InstanceData,
                                       FVMExternalFunction& OutFunction) override;

    // GPU path
    virtual void GetParameterDefinitionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo,
                                             FString& OutHLSL) override;
    virtual bool GetFunctionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo,
                                  const FNiagaraDataInterfaceGeneratedFunction& FunctionInfo,
                                  int32 FunctionInstanceIndex, FString& OutHLSL) override;
    virtual void BuildShaderParameters(FNiagaraShaderParametersBuilder& ShaderParametersBuilder) const override;
    virtual void SetShaderParameters(const FNiagaraDataInterfaceSetShaderParametersContext& Context) const override;

    // Per-instance lifecycle
    virtual bool InitPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance) override;
    virtual void DestroyPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance) override;
    virtual bool PerInstanceTick(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance, float DeltaSeconds) override;
    virtual bool PerInstanceTickPostSimulate(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance, float DeltaSeconds) override;
    virtual int32 PerInstanceDataSize() const override { return sizeof(FSpawnBufferInstanceData); }

    virtual bool CanExecuteOnTarget(ENiagaraSimTarget Target) const override
    {
        return Target == ENiagaraSimTarget::GPUComputeSim;
    }
    virtual bool HasPreSimulateTick() const override { return true; }

    virtual bool Equals(const UNiagaraDataInterface* Other) const override;
    virtual bool CopyToInternal(UNiagaraDataInterface* Destination) const override;

#if WITH_EDITORONLY_DATA
    virtual bool UpgradeFunctionCall(FNiagaraFunctionSignature& FunctionSignature) override { return false; }
#endif

    // No proxy member here — the base class UNiagaraDataInterface owns
    // a TUniquePtr<FNiagaraDataInterfaceProxy> called Proxy. We create our
    // FSpawnBufferProxy into it in PostInitProperties and access it via the
    // base-class GetProxyAs<FSpawnBufferProxy>() helper.
};
