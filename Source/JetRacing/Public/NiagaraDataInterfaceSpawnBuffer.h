// NiagaraDataInterfaceSpawnBuffer.h
#pragma once

#include "CoreMinimal.h"
#include "NiagaraCommon.h"
#include "NiagaraDataInterface.h"

// Pull in render-thread types BEFORE the generated header.
// This file contains BEGIN_SHADER_PARAMETER_STRUCT which must NOT be in a
// header processed by UHT (i.e. any header that also has UCLASS/USTRUCT).
#include "NiagaraDataInterfaceSpawnBuffer_RenderTypes.h"

#include "NiagaraDataInterfaceSpawnBuffer.generated.h"

class UComputeShaderMeshSpawner;

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

    virtual void GetParameterDefinitionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo,
                                             FString& OutHLSL) override;
    virtual bool GetFunctionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo,
                                  const FNiagaraDataInterfaceGeneratedFunction& FunctionInfo,
                                  int32 FunctionInstanceIndex, FString& OutHLSL) override;
    virtual void BuildShaderParameters(FNiagaraShaderParametersBuilder& ShaderParametersBuilder) const override;
    virtual void SetShaderParameters(const FNiagaraDataInterfaceSetShaderParametersContext& Context) const override;

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
};