#pragma once

#include "CoreMinimal.h"
#include "Containers/DynamicRHIResourceArray.h"
#include "RenderResource.h"
#include "RHI.h"
#include "SceneManagement.h"
#include "UniformBuffer.h"
#include "VertexFactory.h"
#include "StaticMeshResources.h"

/**
 * Uniform buffer — currently empty, kept so the shader parameter struct
 * binding path compiles without changes.
 */
BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FComputeDrivenInstancingParameters, )
END_GLOBAL_SHADER_PARAMETER_STRUCT()

typedef TUniformBufferRef<FComputeDrivenInstancingParameters> FComputeDrivenInstancingBufferRef;

/**
 * Per-draw UserData passed from the scene proxy to the vertex shader bindings.
 */
struct FComputeDrivenInstancingUserData : public FOneFrameResource
{
	FRHIShaderResourceView* InstanceBufferSRV;  // MeshRenderInstance[] — GPU-written
	FRHIShaderResourceView* PositionBufferSRV;  // float x3 per vertex
	FRHIShaderResourceView* TangentBufferSRV;   // float4 x2 per vertex (TangentX, TangentZ)
	FRHIShaderResourceView* UV0BufferSRV;       // float2 per UV slot
	uint32                  NumTexCoords;
	FVector3f               LodViewOrigin;
};

/** Fallback index buffer — three indices, used only when no mesh is set. */
class FComputeDrivenInstancingIndexBuffer : public FIndexBuffer
{
public:
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;
	int32 GetIndexCount() const { return NumIndices; }
private:
	int32 NumIndices = 0;
};

class FComputeDrivenInstancingVertexFactory : public FVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FComputeDrivenInstancingVertexFactory);

public:
	FComputeDrivenInstancingVertexFactory(ERHIFeatureLevel::Type InFeatureLevel,
	                                      const FComputeDrivenInstancingParameters& InParams);
	~FComputeDrivenInstancingVertexFactory();

	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;
	virtual void ReleaseRHI() override;

	static bool ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters);
	static void ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters,
	                                         FShaderCompilerEnvironment& OutEnvironment);
	static void ValidateCompiledResult(const FVertexFactoryType* Type, EShaderPlatform Platform,
	                                   const FShaderParameterMap& ParameterMap,
	                                   TArray<FString>& OutErrors);

	/** Supply LOD0 buffers before InitResource() so InitRHI can build the SRVs. */
	void SetMeshBuffers(const FStaticMeshVertexBuffers* InVertexBuffers,
	                    const FIndexBuffer*             InIndexBuffer);

	FIndexBuffer const* GetIndexBuffer() const
	{
		return MeshIndexBuffer ? MeshIndexBuffer : IndexBuffer;
	}

	// SRVs read by GetDynamicMeshElements via FComputeDrivenInstancingUserData.
	FShaderResourceViewRHIRef PositionBufferSRV;
	FShaderResourceViewRHIRef TangentBufferSRV;
	FShaderResourceViewRHIRef UV0BufferSRV;

private:
	FComputeDrivenInstancingParameters  Params;
	FComputeDrivenInstancingBufferRef   UniformBuffer;
	FComputeDrivenInstancingIndexBuffer* IndexBuffer = nullptr;

	const FIndexBuffer*             MeshIndexBuffer   = nullptr; // not owned
	const FStaticMeshVertexBuffers* MeshVertexBuffers = nullptr; // not owned

	friend class FComputeDrivenInstancingShaderParameters;
};
