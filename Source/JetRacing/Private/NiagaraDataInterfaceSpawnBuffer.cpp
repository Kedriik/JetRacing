#include "NiagaraDataInterfaceSpawnBuffer.h"

#include "ComputeShaderMeshSpawner.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraShaderParametersBuilder.h"
#include "NiagaraTypes.h"
#include "RHICommandList.h"

namespace SpawnBufferDIStrings
{
    static const FName   ReadSpawnPositionName(TEXT("ReadSpawnPosition"));
    static const FString SpawnPositionsSuffix(TEXT("SpawnPositions"));
    static const FString NumInstancesSuffix(TEXT("NumInstances"));
}

// ─────────────────────────────────────────────────────────────────────────────
// PostInitProperties
// ─────────────────────────────────────────────────────────────────────────────

void UNiagaraDataInterfaceSpawnBuffer::PostInitProperties()
{
    Super::PostInitProperties();

    // Assign our proxy into the base-class TUniquePtr<FNiagaraDataInterfaceProxy>
    // called Proxy. This is what Niagara reads internally for GPU ticks.
    // MakeUnique is correct here — the base class owns the lifetime.
    // Copies made by Niagara via CopyToInternal each get their own proxy
    // instance pointing at the same underlying RHI resources via the
    // TSharedPtr<FSpawnBufferProxyData> we keep separately for the RHI handles.
    Proxy = MakeUnique<FSpawnBufferProxy>();

    if (HasAnyFlags(RF_ClassDefaultObject))
    {
        ENiagaraTypeRegistryFlags DIFlags =
            ENiagaraTypeRegistryFlags::AllowAnyVariable |
            ENiagaraTypeRegistryFlags::AllowParameter;
        FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(GetClass()), DIFlags);
    }

    if (!HasAnyFlags(RF_ClassDefaultObject))
    {
        FSpawnBufferProxy* P = GetProxyAs<FSpawnBufferProxy>();
        ENQUEUE_RENDER_COMMAND(InitSpawnBufferDummy)(
            [P](FRHICommandListImmediate& RHICmdList)
            {
                P->InitDummyBuffer_RenderThread(RHICmdList);
            });
    }

    MarkRenderDataDirty();
}

// ─────────────────────────────────────────────────────────────────────────────
// GetFunctions
// ─────────────────────────────────────────────────────────────────────────────

void UNiagaraDataInterfaceSpawnBuffer::GetFunctions(
    TArray<FNiagaraFunctionSignature>& OutFunctions)
{
    FNiagaraFunctionSignature Sig;
    Sig.Name             = SpawnBufferDIStrings::ReadSpawnPositionName;
    Sig.bMemberFunction  = true;
    Sig.bRequiresContext = false;
    Sig.bSupportsCPU    = false;
    Sig.bSupportsGPU    = true;

    Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("SpawnBuffer")));
    Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(),   TEXT("Index")));

    Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(),  TEXT("Position")));
    Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Scale")));
    Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(),  TEXT("Valid")));

    OutFunctions.Add(Sig);
}

// ─────────────────────────────────────────────────────────────────────────────
// CPU stub
// ─────────────────────────────────────────────────────────────────────────────

void UNiagaraDataInterfaceSpawnBuffer::GetVMExternalFunction(
    const FVMExternalFunctionBindingInfo&, void*, FVMExternalFunction&)
{
    UE_LOG(LogTemp, Error,
        TEXT("UNiagaraDataInterfaceSpawnBuffer: GPU only. Set emitter Sim Target to GPU Compute Sim."));
}

// ─────────────────────────────────────────────────────────────────────────────
// HLSL emission
// ─────────────────────────────────────────────────────────────────────────────

void UNiagaraDataInterfaceSpawnBuffer::GetParameterDefinitionHLSL(
    const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL)
{
    Super::GetParameterDefinitionHLSL(ParamInfo, OutHLSL);

    OutHLSL.Appendf(TEXT("StructuredBuffer<float4> %s_%s;\n"),
        *ParamInfo.DataInterfaceHLSLSymbol, *SpawnBufferDIStrings::SpawnPositionsSuffix);
    OutHLSL.Appendf(TEXT("int %s_%s;\n"),
        *ParamInfo.DataInterfaceHLSLSymbol, *SpawnBufferDIStrings::NumInstancesSuffix);
}

bool UNiagaraDataInterfaceSpawnBuffer::GetFunctionHLSL(
    const FNiagaraDataInterfaceGPUParamInfo& ParamInfo,
    const FNiagaraDataInterfaceGeneratedFunction& FunctionInfo,
    int32 FunctionInstanceIndex, FString& OutHLSL)
{
    if (Super::GetFunctionHLSL(ParamInfo, FunctionInfo, FunctionInstanceIndex, OutHLSL))
        return true;

    if (FunctionInfo.DefinitionName != SpawnBufferDIStrings::ReadSpawnPositionName)
        return false;

    const FString BufferVar = FString::Printf(TEXT("%s_%s"),
        *ParamInfo.DataInterfaceHLSLSymbol, *SpawnBufferDIStrings::SpawnPositionsSuffix);
    const FString CountVar = FString::Printf(TEXT("%s_%s"),
        *ParamInfo.DataInterfaceHLSLSymbol, *SpawnBufferDIStrings::NumInstancesSuffix);

    OutHLSL += FString::Format(
        TEXT(
            "void {FunctionName}(int Index, out float3 OutPosition, out float OutScale, out bool OutValid)\n"
            "{{\n"
            "    if (Index >= 0 && Index < {CountVar})\n"
            "    {{\n"
            "        float4 Entry = {BufferVar}[Index];\n"
            "        OutValid    = (Entry.w > 0.0);\n"
            "        OutPosition = Entry.xyz;\n"
            "        OutScale    = Entry.w;\n"
            "    }}\n"
            "    else\n"
            "    {{\n"
            "        OutPosition = float3(0,0,0);\n"
            "        OutScale    = 0.0;\n"
            "        OutValid    = false;\n"
            "    }}\n"
            "}}\n"
        ),
        {
            { TEXT("FunctionName"), FunctionInfo.InstanceName },
            { TEXT("BufferVar"),    BufferVar },
            { TEXT("CountVar"),     CountVar  },
        }
    );

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Shader parameter binding
// ─────────────────────────────────────────────────────────────────────────────

void UNiagaraDataInterfaceSpawnBuffer::BuildShaderParameters(
    FNiagaraShaderParametersBuilder& ShaderParametersBuilder) const
{
    ShaderParametersBuilder.AddNestedStruct<FSpawnBufferShaderParameters>();
}

void UNiagaraDataInterfaceSpawnBuffer::SetShaderParameters(
    const FNiagaraDataInterfaceSetShaderParametersContext& Context) const
{
    FSpawnBufferShaderParameters* Params =
        Context.GetParameterNestedStruct<FSpawnBufferShaderParameters>();
    if (!Params)
        return;

    FSpawnBufferProxy& DIProxy = Context.GetProxy<FSpawnBufferProxy>();

    // Prefer the real SRV; fall back to the dummy (always a valid structured buffer).
    FShaderResourceViewRHIRef SafeSRV = DIProxy.SpawnPositionsSRV.IsValid()
                                            ? DIProxy.SpawnPositionsSRV
                                            : DIProxy.DummySRV;

    if (!SafeSRV.IsValid())
    {
        Params->NumInstances   = 0;
        Params->SpawnPositions = nullptr;
        return;
    }

    Params->NumInstances   = DIProxy.SpawnPositionsSRV.IsValid() ? DIProxy.NumInstances : 0;
    Params->SpawnPositions = SafeSRV;
}

// ─────────────────────────────────────────────────────────────────────────────
// Per-instance lifecycle
// ─────────────────────────────────────────────────────────────────────────────

bool UNiagaraDataInterfaceSpawnBuffer::InitPerInstanceData(
    void* PerInstanceData, FNiagaraSystemInstance* SystemInstance)
{
    FSpawnBufferInstanceData* InstData = new (PerInstanceData) FSpawnBufferInstanceData();

    // SpawnerActor may be null here if the User Parameter override hasn't been
    // applied yet (common when Niagara initialises before BeginPlay completes).
    // We resolve the component lazily in PerInstanceTick instead.
    if (SpawnerActor)
    {
        InstData->Spawner = SpawnerActor->FindComponentByClass<UComputeShaderMeshSpawner>();
    }

    // Ensure dummy buffer is ready on the render thread before frame 1.
    FSpawnBufferProxy* P = GetProxyAs<FSpawnBufferProxy>();
    ENQUEUE_RENDER_COMMAND(EnsureSpawnBufferDummy)(
        [P](FRHICommandListImmediate& RHICmdList)
        {
            P->InitDummyBuffer_RenderThread(RHICmdList);
        });

    return true;
}

void UNiagaraDataInterfaceSpawnBuffer::DestroyPerInstanceData(
    void* PerInstanceData, FNiagaraSystemInstance* SystemInstance)
{
    FSpawnBufferInstanceData* InstData = static_cast<FSpawnBufferInstanceData*>(PerInstanceData);
    InstData->~FSpawnBufferInstanceData();

    FSpawnBufferProxy* P = GetProxyAs<FSpawnBufferProxy>();
    ENQUEUE_RENDER_COMMAND(ClearSpawnBufferProxy)(
        [P](FRHICommandListImmediate&)
        {
            P->SpawnPositionsSRV = nullptr;
            P->NumInstances      = 0;
        });
}

bool UNiagaraDataInterfaceSpawnBuffer::PerInstanceTick(
    void* PerInstanceData, FNiagaraSystemInstance* SystemInstance, float DeltaSeconds)
{
    FSpawnBufferInstanceData* InstData = static_cast<FSpawnBufferInstanceData*>(PerInstanceData);

    // Lazy resolution: if we didn't find the spawner at init time (because the
    // User Parameter override wasn't applied yet), try again every tick until
    // we find it. Once found, this branch never runs again.
    if (!InstData->Spawner.IsValid() && SpawnerActor)
    {
        InstData->Spawner = SpawnerActor->FindComponentByClass<UComputeShaderMeshSpawner>();
    }

    return false;
}

bool UNiagaraDataInterfaceSpawnBuffer::PerInstanceTickPostSimulate(
    void* PerInstanceData, FNiagaraSystemInstance* SystemInstance, float DeltaSeconds)
{
    FSpawnBufferInstanceData* InstData = static_cast<FSpawnBufferInstanceData*>(PerInstanceData);

    if (!InstData->Spawner.IsValid())
        return false;

    FShaderResourceViewRHIRef SRV   = InstData->Spawner->GetPositionBufferSRV();
    int32                     Count = InstData->Spawner->GetNumInstances();

    if (!SRV.IsValid())
        return false;

    FSpawnBufferProxy* P = GetProxyAs<FSpawnBufferProxy>();
    ENQUEUE_RENDER_COMMAND(UpdateSpawnBufferProxy)(
        [P, SRV, Count](FRHICommandListImmediate&)
        {
            P->SpawnPositionsSRV = SRV;
            P->NumInstances      = Count;
        });

    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Equality / copy
// ─────────────────────────────────────────────────────────────────────────────

bool UNiagaraDataInterfaceSpawnBuffer::Equals(const UNiagaraDataInterface* Other) const
{
    if (!Super::Equals(Other))
        return false;
    return CastChecked<const UNiagaraDataInterfaceSpawnBuffer>(Other)->SpawnerActor == SpawnerActor;
}

bool UNiagaraDataInterfaceSpawnBuffer::CopyToInternal(UNiagaraDataInterface* Destination) const
{
    if (!Super::CopyToInternal(Destination))
        return false;
    auto* Dest = CastChecked<UNiagaraDataInterfaceSpawnBuffer>(Destination);
    Dest->SpawnerActor = SpawnerActor;
    // Each copy gets its own proxy via CopyToInternal calling PostInitProperties
    // on the destination — the base class Super::CopyToInternal handles that.
    return true;
}
