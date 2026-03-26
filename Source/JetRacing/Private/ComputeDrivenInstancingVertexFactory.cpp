#include "ComputeDrivenInstancingVertexFactory.h"

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

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FComputeDrivenInstancingParameters, "ComputeDrivenInstancingParams");

// ---------------------------------------------------------------------------
// Index buffer — three indices for the fallback case (no mesh assigned).
// ---------------------------------------------------------------------------
namespace FComputeDrivenInstancingUtil
{
	template <typename T>
	FBufferRHIRef CreateIndexBuffer(FRHICommandListBase& RHICmdList)
	{
		TResourceArray<T, INDEXBUFFER_ALIGNMENT> Indices;
		Indices.Reserve(3);
		Indices.Add(1);
		Indices.Add(0);
		Indices.Add(2);

		const uint32 Size   = Indices.GetResourceDataSize();
		const uint32 Stride = sizeof(T);

		FRHIResourceCreateInfo CreateInfo(TEXT("ComputeDrivenInstancingIndexBuffer"), &Indices);
		return RHICmdList.CreateIndexBuffer(Stride, Size, BUF_Static, CreateInfo);
	}
}

void FComputeDrivenInstancingIndexBuffer::InitRHI(FRHICommandListBase& RHICmdList)
{
	IndexBufferRHI = FComputeDrivenInstancingUtil::CreateIndexBuffer<uint16>(RHICmdList);
}

// ---------------------------------------------------------------------------
// Shader parameters — binds the per-draw UserData to shader registers.
// ---------------------------------------------------------------------------
class FComputeDrivenInstancingShaderParameters : public FVertexFactoryShaderParameters
{
	DECLARE_TYPE_LAYOUT(FComputeDrivenInstancingShaderParameters, NonVirtual);

public:
	void Bind(const FShaderParameterMap& ParameterMap)
	{
		InstanceBufferParameter.Bind(ParameterMap,     TEXT("InstanceBuffer"));
		MeshPositionBufferParameter.Bind(ParameterMap, TEXT("MeshPositionBuffer"));
		MeshTangentBufferParameter.Bind(ParameterMap,  TEXT("MeshTangentBuffer"));
		MeshUVBufferParameter.Bind(ParameterMap,       TEXT("MeshUVBuffer"));
		MeshNumTexCoordsParameter.Bind(ParameterMap,   TEXT("MeshNumTexCoords"));
		LodViewOriginParameter.Bind(ParameterMap,      TEXT("LodViewOrigin"));
	}

	void GetElementShaderBindings(
		const class FSceneInterface*        Scene,
		const class FSceneView*             View,
		const class FMeshMaterialShader*    Shader,
		const EVertexInputStreamType        InputStreamType,
		ERHIFeatureLevel::Type              FeatureLevel,
		const class FVertexFactory*         InVertexFactory,
		const struct FMeshBatchElement&     BatchElement,
		class FMeshDrawSingleShaderBindings& ShaderBindings,
		FVertexInputStreamArray&            VertexStreams) const
	{
		FComputeDrivenInstancingVertexFactory* VF =
			(FComputeDrivenInstancingVertexFactory*)InVertexFactory;

		ShaderBindings.Add(
			Shader->GetUniformBufferParameter<FComputeDrivenInstancingParameters>(),
			VF->UniformBuffer);

		FComputeDrivenInstancingUserData* UserData =
			(FComputeDrivenInstancingUserData*)BatchElement.UserData;

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

IMPLEMENT_TYPE_LAYOUT(FComputeDrivenInstancingShaderParameters);

IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FComputeDrivenInstancingVertexFactory, SF_Vertex,
                                        FComputeDrivenInstancingShaderParameters);
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FComputeDrivenInstancingVertexFactory, SF_Pixel,
                                        FComputeDrivenInstancingShaderParameters);

// ---------------------------------------------------------------------------
// Vertex factory
// ---------------------------------------------------------------------------
FComputeDrivenInstancingVertexFactory::FComputeDrivenInstancingVertexFactory(
	ERHIFeatureLevel::Type                    InFeatureLevel,
	const FComputeDrivenInstancingParameters& InParams)
	: FVertexFactory(InFeatureLevel)
	, Params(InParams)
{
	IndexBuffer = new FComputeDrivenInstancingIndexBuffer();
}

FComputeDrivenInstancingVertexFactory::~FComputeDrivenInstancingVertexFactory()
{
	delete IndexBuffer;
}

void FComputeDrivenInstancingVertexFactory::SetMeshBuffers(
	const FStaticMeshVertexBuffers* InVertexBuffers,
	const FIndexBuffer*             InIndexBuffer)
{
	MeshVertexBuffers = InVertexBuffers;
	MeshIndexBuffer   = InIndexBuffer;
}

void FComputeDrivenInstancingVertexFactory::InitRHI(FRHICommandListBase& RHICmdList)
{
	UniformBuffer = FComputeDrivenInstancingBufferRef::CreateUniformBufferImmediate(
		Params, UniformBuffer_MultiFrame);

	IndexBuffer->InitResource(RHICmdList);

	FVertexStream NullStream;
	NullStream.VertexBuffer      = nullptr;
	NullStream.Stride            = 0;
	NullStream.Offset            = 0;
	NullStream.VertexStreamUsage = EVertexStreamUsage::ManualFetch;

	check(Streams.Num() == 0);
	Streams.Add(NullStream);

	FVertexDeclarationElementList Elements;
	InitDeclaration(Elements);

	// Build manual-fetch SRVs from the static mesh vertex buffers.
	if (MeshVertexBuffers)
	{
		PositionBufferSRV = RHICmdList.CreateShaderResourceView(
			MeshVertexBuffers->PositionVertexBuffer.VertexBufferRHI,
			sizeof(float), PF_R32_FLOAT);

		TangentBufferSRV = RHICmdList.CreateShaderResourceView(
			MeshVertexBuffers->StaticMeshVertexBuffer.TangentsVertexBuffer.VertexBufferRHI,
			sizeof(FPackedNormal), PF_R8G8B8A8_SNORM);

		const bool bFullPrecision =
			MeshVertexBuffers->StaticMeshVertexBuffer.GetUseFullPrecisionUVs();

		UV0BufferSRV = bFullPrecision
			? RHICmdList.CreateShaderResourceView(
				MeshVertexBuffers->StaticMeshVertexBuffer.TexCoordVertexBuffer.VertexBufferRHI,
				sizeof(FVector2f), PF_G32R32F)
			: RHICmdList.CreateShaderResourceView(
				MeshVertexBuffers->StaticMeshVertexBuffer.TexCoordVertexBuffer.VertexBufferRHI,
				sizeof(FVector2DHalf), PF_G16R16F);
	}
}

void FComputeDrivenInstancingVertexFactory::ReleaseRHI()
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

bool FComputeDrivenInstancingVertexFactory::ShouldCompilePermutation(
	const FVertexFactoryShaderPermutationParameters& Parameters)
{
	if (!IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5))
		return false;

	return Parameters.MaterialParameters.MaterialDomain == MD_Surface
		&& Parameters.MaterialParameters.bIsUsedWithVirtualHeightfieldMesh
		|| Parameters.MaterialParameters.bIsSpecialEngineMaterial;
}

void FComputeDrivenInstancingVertexFactory::ModifyCompilationEnvironment(
	const FVertexFactoryShaderPermutationParameters& Parameters,
	FShaderCompilerEnvironment& OutEnvironment)
{
}

void FComputeDrivenInstancingVertexFactory::ValidateCompiledResult(
	const FVertexFactoryType* Type, EShaderPlatform Platform,
	const FShaderParameterMap& ParameterMap, TArray<FString>& OutErrors)
{
}

// Shader path points to our own Shaders directory — same virtual mount as the compute shader.
IMPLEMENT_VERTEX_FACTORY_TYPE(
	FComputeDrivenInstancingVertexFactory,
	"/Project/JetRacing/Private/ComputeDrivenInstancingVertexFactory.ush",
	EVertexFactoryFlags::UsedWithMaterials
	| EVertexFactoryFlags::SupportsDynamicLighting
	| EVertexFactoryFlags::SupportsPrimitiveIdStream);
