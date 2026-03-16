// Copyright Epic Games, Inc. All Rights Reserved.
// Adapted from the VirtualHeightfieldMesh plugin

#include "ExampleIndirectInstancingVertexFactory.h"

#include "Engine/Engine.h"
#include "EngineGlobals.h"
#include "Materials/Material.h"
#include "MeshMaterialShader.h"
#include "RHIStaticStates.h"
#include "ShaderParameters.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "MaterialDomain.h"
#include "MeshDrawShaderBindings.h"
#include "ShaderParameterUtils.h"
#include "StaticMeshResources.h"

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FExampleIndirectInstancingParameters, "ExampleIndirectInstancingParams");

namespace FExampleIndirectInstancingUtil
{
	template <typename T>
	FBufferRHIRef CreateIndexBuffer(FRHICommandListBase & RHICmdList)
	{
		TResourceArray<T, INDEXBUFFER_ALIGNMENT> Indices;

		// Allocate room for indices
		Indices.Reserve(3);

		// Top left
		// CCW triangle winding order
		Indices.Add(1);
		Indices.Add(0);
		Indices.Add(2);

		const uint32 Size = Indices.GetResourceDataSize();
		const uint32 Stride = sizeof(T);

		// Create index buffer. Fill buffer with initial data upon creation
		FRHIResourceCreateInfo CreateInfo(TEXT("ExampleIndirectInstancingIndexBuffer"), &Indices);
		return RHICmdList.CreateIndexBuffer(Stride, Size, BUF_Static, CreateInfo);
	}
}

void FExampleIndirectInstancingIndexBuffer::InitRHI(FRHICommandListBase &RHICmdList)
{
	IndexBufferRHI = FExampleIndirectInstancingUtil::CreateIndexBuffer<uint16>(RHICmdList);
}

/**
 * Shader parameters for vertex factory.
 */
class FExampleIndirectInstancingShaderParameters : public FVertexFactoryShaderParameters
{
	DECLARE_TYPE_LAYOUT(FExampleIndirectInstancingShaderParameters, NonVirtual);

public:
	void Bind(const FShaderParameterMap &ParameterMap)
	{
		InstanceBufferParameter.Bind(ParameterMap,     TEXT("InstanceBuffer"));
		MeshPositionBufferParameter.Bind(ParameterMap, TEXT("MeshPositionBuffer"));
		MeshTangentBufferParameter.Bind(ParameterMap,  TEXT("MeshTangentBuffer"));
		MeshUVBufferParameter.Bind(ParameterMap,       TEXT("MeshUVBuffer"));
		MeshNumTexCoordsParameter.Bind(ParameterMap,   TEXT("MeshNumTexCoords"));
		LodViewOriginParameter.Bind(ParameterMap,      TEXT("LodViewOrigin"));
	}

	void GetElementShaderBindings(
			const class FSceneInterface *Scene,
			const class FSceneView *View,
			const class FMeshMaterialShader *Shader,
			const EVertexInputStreamType InputStreamType,
			ERHIFeatureLevel::Type FeatureLevel,
			const class FVertexFactory *InVertexFactory,
			const struct FMeshBatchElement &BatchElement,
			class FMeshDrawSingleShaderBindings &ShaderBindings,
			FVertexInputStreamArray &VertexStreams) const
	{
		FExampleIndirectInstancingVertexFactory *VertexFactory = (FExampleIndirectInstancingVertexFactory *)InVertexFactory;
		ShaderBindings.Add(Shader->GetUniformBufferParameter<FExampleIndirectInstancingParameters>(), VertexFactory->UniformBuffer);

		FExampleIndirectInstancingUserData *UserData = (FExampleIndirectInstancingUserData *)BatchElement.UserData;
		ShaderBindings.Add(InstanceBufferParameter,     UserData->InstanceBufferSRV);
		ShaderBindings.Add(MeshPositionBufferParameter, UserData->PositionBufferSRV);
		ShaderBindings.Add(MeshTangentBufferParameter,  UserData->TangentBufferSRV);
		ShaderBindings.Add(MeshUVBufferParameter,       UserData->UV0BufferSRV);
		ShaderBindings.Add(MeshNumTexCoordsParameter,   UserData->NumTexCoords);
		ShaderBindings.Add(LodViewOriginParameter,      UserData->LodViewOrigin);
	}

protected:
	LAYOUT_FIELD(FShaderResourceParameter, InstanceBufferParameter);
	LAYOUT_FIELD(FShaderResourceParameter, MeshPositionBufferParameter);
	LAYOUT_FIELD(FShaderResourceParameter, MeshTangentBufferParameter);
	LAYOUT_FIELD(FShaderResourceParameter, MeshUVBufferParameter);
	LAYOUT_FIELD(FShaderParameter,         MeshNumTexCoordsParameter);
	LAYOUT_FIELD(FShaderParameter,         LodViewOriginParameter);
};

IMPLEMENT_TYPE_LAYOUT(FExampleIndirectInstancingShaderParameters);

IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FExampleIndirectInstancingVertexFactory, SF_Vertex, FExampleIndirectInstancingShaderParameters);
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FExampleIndirectInstancingVertexFactory, SF_Pixel, FExampleIndirectInstancingShaderParameters);

FExampleIndirectInstancingVertexFactory::FExampleIndirectInstancingVertexFactory(ERHIFeatureLevel::Type InFeatureLevel, const FExampleIndirectInstancingParameters &InParams)
		: FVertexFactory(InFeatureLevel), Params(InParams)
{
	IndexBuffer = new FExampleIndirectInstancingIndexBuffer();
}

FExampleIndirectInstancingVertexFactory::~FExampleIndirectInstancingVertexFactory()
{
	delete IndexBuffer;
}

void FExampleIndirectInstancingVertexFactory::SetMeshBuffers(const FStaticMeshVertexBuffers* InVertexBuffers, const FIndexBuffer* InIndexBuffer)
{
	MeshVertexBuffers = InVertexBuffers;
	MeshIndexBuffer   = InIndexBuffer;
}

void FExampleIndirectInstancingVertexFactory::InitRHI(FRHICommandListBase &RHICmdList)
{
	UniformBuffer = FExampleIndirectInstancingBufferRef::CreateUniformBufferImmediate(Params, UniformBuffer_MultiFrame);

	// Keep the fallback index buffer for the case where no mesh is set.
	IndexBuffer->InitResource(RHICmdList);

	FVertexStream NullVertexStream;
	NullVertexStream.VertexBuffer        = nullptr;
	NullVertexStream.Stride              = 0;
	NullVertexStream.Offset              = 0;
	NullVertexStream.VertexStreamUsage   = EVertexStreamUsage::ManualFetch;

	check(Streams.Num() == 0);
	Streams.Add(NullVertexStream);

	FVertexDeclarationElementList Elements;
	InitDeclaration(Elements);

	// Build SRVs for the three manual-fetch vertex streams.
	if (MeshVertexBuffers)
	{
		// Position: one float per element, shader fetches 3 consecutive → PF_R32_FLOAT
		PositionBufferSRV = RHICmdList.CreateShaderResourceView(
			MeshVertexBuffers->PositionVertexBuffer.VertexBufferRHI,
			sizeof(float), PF_R32_FLOAT);

		// Tangents: FPackedNormal (SNORM8x4) x2 per vertex (TangentX then TangentZ)
		TangentBufferSRV = RHICmdList.CreateShaderResourceView(
			MeshVertexBuffers->StaticMeshVertexBuffer.TangentsVertexBuffer.VertexBufferRHI,
			sizeof(FPackedNormal), PF_R8G8B8A8_SNORM);

		// UVs: may be half or full precision depending on the mesh import settings.
		const bool bUseFullPrecisionUVs = MeshVertexBuffers->StaticMeshVertexBuffer.GetUseFullPrecisionUVs();
		if (bUseFullPrecisionUVs)
		{
			UV0BufferSRV = RHICmdList.CreateShaderResourceView(
				MeshVertexBuffers->StaticMeshVertexBuffer.TexCoordVertexBuffer.VertexBufferRHI,
				sizeof(FVector2f), PF_G32R32F);
		}
		else
		{
			UV0BufferSRV = RHICmdList.CreateShaderResourceView(
				MeshVertexBuffers->StaticMeshVertexBuffer.TexCoordVertexBuffer.VertexBufferRHI,
				sizeof(FVector2DHalf), PF_G16R16F);
		}
	}
}

void FExampleIndirectInstancingVertexFactory::ReleaseRHI()
{
	UniformBuffer.SafeRelease();
	PositionBufferSRV.SafeRelease();
	TangentBufferSRV.SafeRelease();
	UV0BufferSRV.SafeRelease();

	if (IndexBuffer)
	{
		IndexBuffer->ReleaseResource();
	}

	FVertexFactory::ReleaseRHI();
}

bool FExampleIndirectInstancingVertexFactory::ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters &Parameters)
{
	// todo[vhm]: Fallback path for mobile.
	if (!IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5))
	{
		return false;
	}
	// TODO
	return (Parameters.MaterialParameters.MaterialDomain == MD_Surface && Parameters.MaterialParameters.bIsUsedWithVirtualHeightfieldMesh) || Parameters.MaterialParameters.bIsSpecialEngineMaterial;
}

void FExampleIndirectInstancingVertexFactory::ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters &Parameters, FShaderCompilerEnvironment &OutEnvironment)
{
	// TODO
	// OutEnvironment.SetDefine(TEXT("VF_VIRTUAL_HEIGHFIELD_MESH"), 1);
}

void FExampleIndirectInstancingVertexFactory::ValidateCompiledResult(const FVertexFactoryType *Type, EShaderPlatform Platform, const FShaderParameterMap &ParameterMap, TArray<FString> &OutErrors)
{
}

// TODO
IMPLEMENT_VERTEX_FACTORY_TYPE(FExampleIndirectInstancingVertexFactory, "/IndirectInstancingShaders/ExampleIndirectInstancing/ExampleIndirectInstancingVertexFactory.ush",
															EVertexFactoryFlags::UsedWithMaterials | EVertexFactoryFlags::SupportsDynamicLighting | EVertexFactoryFlags::SupportsPrimitiveIdStream);
