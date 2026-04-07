// NiagaraDataInterfaceSpawnBuffer_RenderTypes.h
#pragma once

#include "NiagaraDataInterface.h"
#include "NiagaraShaderParametersBuilder.h"
#include "RHI.h"
#include "RHIResources.h"
#include "ComputeShaderMeshSpawner.h"
#include "ShaderParameterStruct.h"

// ─────────────────────────────────────────────────────────────────────────────
// Shader parameter struct
// Must live in a non-UHT-processed header. BEGIN_SHADER_PARAMETER_STRUCT and
// UCLASS in the same header causes C2143 because UHT inserts class declarations
// that collide with the macro-expanded anonymous structs.
// ─────────────────────────────────────────────────────────────────────────────
BEGIN_SHADER_PARAMETER_STRUCT(FSpawnBufferShaderParameters, )
    SHADER_PARAMETER(int32, NumInstances)
    SHADER_PARAMETER_SRV(StructuredBuffer<float4>, SpawnPositions)
END_SHADER_PARAMETER_STRUCT()

// ─────────────────────────────────────────────────────────────────────────────
// Render-thread proxy
// ─────────────────────────────────────────────────────────────────────────────
struct FSpawnBufferProxy : public FNiagaraDataInterfaceProxy
{
    FShaderResourceViewRHIRef SpawnPositionsSRV;
    int32                     NumInstances = 0;

    FBufferRHIRef             DummyBuffer;
    FShaderResourceViewRHIRef DummySRV;

    void InitDummyBuffer_RenderThread(FRHICommandListImmediate& RHICmdList)
    {
        if (DummySRV.IsValid())
            return;

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