// Copyright Epic Games, Inc. All Rights Reserved.
// Adapted from the VirtualHeightfieldMesh plugin

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
 * Uniform buffer to hold parameters specific to this vertex factory. Only set up once.
 */
BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FExampleIndirectInstancingParameters, )
// SHADER_PARAMETER_TEXTURE(Texture2D<uint4>, PageTableTexture)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

typedef TUniformBufferRef<FExampleIndirectInstancingParameters> FExampleIndirectInstancingBufferRef;

/**
 * Per frame UserData to pass to the vertex shader.
 */
struct FExampleIndirectInstancingUserData : public FOneFrameResource
{
	FRHIShaderResourceView* InstanceBufferSRV;   // culled MeshRenderInstance buffer
	FRHIShaderResourceView* PositionBufferSRV;   // float x3 per vertex
	FRHIShaderResourceView* TangentBufferSRV;    // float4 x2 per vertex (TangentX, TangentZ)
	FRHIShaderResourceView* UV0BufferSRV;        // float2 per UV slot
	uint32 NumTexCoords;                         // UV channel count (interleaved buffer stride)
	FVector3f LodViewOrigin;
};

/*
 * Index buffer to provide incides for the mesh we're rending.
 */
class FExampleIndirectInstancingIndexBuffer : public FIndexBuffer
{
public:
	FExampleIndirectInstancingIndexBuffer()
	{
	}

	virtual void InitRHI(FRHICommandListBase & RHICmdList) override;

	int32 GetIndexCount() const { return NumIndices; }

private:
	int32 NumIndices = 0;
};

class FExampleIndirectInstancingVertexFactory : public FVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FExampleIndirectInstancing);

public:
	FExampleIndirectInstancingVertexFactory(ERHIFeatureLevel::Type InFeatureLevel, const FExampleIndirectInstancingParameters &InParams);

	~FExampleIndirectInstancingVertexFactory();

	virtual void InitRHI(FRHICommandListBase & RHICmdList) override;
	virtual void ReleaseRHI() override;

	static bool ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters &Parameters);
	static void ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters &Parameters, FShaderCompilerEnvironment &OutEnvironment);
	static void ValidateCompiledResult(const FVertexFactoryType *Type, EShaderPlatform Platform, const FShaderParameterMap &ParameterMap, TArray<FString> &OutErrors);

	FIndexBuffer const *GetIndexBuffer() const { return MeshIndexBuffer ? MeshIndexBuffer : IndexBuffer; }

	/**
	 * Supply the LOD0 vertex buffers before InitResource().
	 * Builds SRVs for the three manual-fetch streams used by the shader.
	 */
	void SetMeshBuffers(const FStaticMeshVertexBuffers* InVertexBuffers, const FIndexBuffer* InIndexBuffer);

	// Cached SRVs for the three vertex streams — read by GetDynamicMeshElements via UserData.
	FShaderResourceViewRHIRef PositionBufferSRV;
	FShaderResourceViewRHIRef TangentBufferSRV;
	FShaderResourceViewRHIRef UV0BufferSRV;

private:
	FExampleIndirectInstancingParameters Params;
	FExampleIndirectInstancingBufferRef UniformBuffer;
	FExampleIndirectInstancingIndexBuffer *IndexBuffer = nullptr;

	// Mesh LOD index buffer — set via SetMeshBuffers().  Not owned.
	const FIndexBuffer* MeshIndexBuffer = nullptr;
	// Mesh LOD vertex buffers — set via SetMeshBuffers().  Not owned.
	const FStaticMeshVertexBuffers* MeshVertexBuffers = nullptr;

	// Shader parameters is the data passed to our vertex shader
	friend class FExampleIndirectInstancingShaderParameters;
};
