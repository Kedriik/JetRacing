// Copyright Epic Games, Inc. All Rights Reserved.
// Adapted from the VirtualHeightfieldMesh plugin

#include "ExampleIndirectInstancingSceneProxy.h"

#include "CommonRenderResources.h"
#include "EngineModule.h"
#include "Engine/Engine.h"
#include "GlobalShader.h"
#include "HAL/IConsoleManager.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderUtils.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "Misc/ScopeLock.h"
#include "IndirectInstancing/Public/ExampleIndirectInstancing/ExampleIndirectInstancingComponent.h"
#include "ExampleIndirectInstancingVertexFactory.h"
#include "RHIResources.h"
#include "RHICommandList.h"

namespace ExampleIndirectInstancingMesh
{
	// UE6.6
	/** Initialize the FDrawInstanceBuffers objects. */
	//void InitializeInstanceBuffers(FRHICommandListImmediate & InRHICmdList, FDrawInstanceBuffers & InBuffers);

	// UE5.6
	/** Initialize the FDrawInstanceBuffers objects. */
	void InitializeInstanceBuffers(FRHICommandListBase& RHICmdList, FDrawInstanceBuffers& InBuffers);


	/** Release the FDrawInstanceBuffers objects. */
	void ReleaseInstanceBuffers(FDrawInstanceBuffers & InBuffers)
	{
		InBuffers.InstanceBuffer.SafeRelease();
		InBuffers.InstanceBufferUAV.SafeRelease();
		InBuffers.InstanceBufferSRV.SafeRelease();
		InBuffers.IndirectArgsBuffer.SafeRelease();
		InBuffers.IndirectArgsBufferUAV.SafeRelease();
	}
}

/** Renderer extension to manage the buffer pool and add hooks for GPU culling passes. */
class FExampleIndirectInstancingRendererExtension : public FRenderResource
{
public:
	FExampleIndirectInstancingRendererExtension()
			: bInFrame(false), DiscardId(0)
	{
	}

	virtual ~FExampleIndirectInstancingRendererExtension()
	{
	}

	bool IsInFrame()
	{
		FScopeLock ScopeLock(&WorkLock);
		return bInFrame;
	}

	/** Call once to register this extension. */
	void RegisterExtension();

	/** Call once per frame for each mesh/view that has relevance. This allocates the buffers to use for the frame and adds the work to fill the buffers to the queue. */
	ExampleIndirectInstancingMesh::FDrawInstanceBuffers AddWork(FExampleIndirectInstancingSceneProxy const *InProxy, FSceneView const *InMainView, FSceneView const *InCullView);
	/** Submit all the work added by AddWork(). The work fills all of the buffers ready for use by the referencing mesh batches. */
	void SubmitWork(FRDGBuilder & GraphBuilder);

protected:
	//~ Begin FRenderResource Interface
	virtual void ReleaseRHI() override;
	//~ End FRenderResource Interface

private:
	/** Protects cross-thread access during parallel GetDynamicMeshElements work collection. */
	mutable FCriticalSection WorkLock;

	/** Called by renderer at start of render frame. */
	void BeginFrame(FRDGBuilder & GraphBuilder);
	/** Called by renderer at end of render frame. */
	void EndFrame(FRDGBuilder & GraphBuilder);
	void EndFrame();

	/** Flag for frame validation. */
	bool bInFrame;

	/** Buffers to fill. Resources can persist between frames to reduce allocation cost, but contents don't persist. */
	TArray<ExampleIndirectInstancingMesh::FDrawInstanceBuffers> Buffers;
	/** Per buffer frame time stamp of last usage. */
	TArray<uint32> DiscardIds;
	/** Current frame time stamp. */
	uint32 DiscardId;

	/** Arrary of unique scene proxies to render this frame. */
	TArray<FExampleIndirectInstancingSceneProxy const *> SceneProxies;
	/** Arrary of unique main views to render this frame. */
	TArray<FSceneView const *> MainViews;
	/** Arrary of unique culling views to render this frame. */
	TArray<FSceneView const *> CullViews;

	/** Key for each buffer we need to generate. */
	struct FWorkDesc
	{
		int32 ProxyIndex;
		int32 MainViewIndex;
		int32 CullViewIndex;
		int32 BufferIndex;
	};

	/** Keys specifying what to render. */
	TArray<FWorkDesc> WorkDescs;

	/** Sort predicate for FWorkDesc. When rendering we want to batch work by proxy, then by main view. */
	struct FWorkDescSort
	{
		uint32 SortKey(FWorkDesc const &WorkDesc) const
		{
			return (WorkDesc.ProxyIndex << 24) | (WorkDesc.MainViewIndex << 16) | (WorkDesc.CullViewIndex << 8) | WorkDesc.BufferIndex;
		}

		bool operator()(FWorkDesc const &A, FWorkDesc const &B) const
		{
			return SortKey(A) < SortKey(B);
		}
	};
};

/** Single global instance of the ISM renderer extension. */
TGlobalResource<FExampleIndirectInstancingRendererExtension> ExampleIndirectInstancingRendererExtension;

void FExampleIndirectInstancingRendererExtension::RegisterExtension()
{
	static bool bInit = false;
	if (!bInit)
	{
		GEngine->GetPreRenderDelegateEx().AddRaw(this, &FExampleIndirectInstancingRendererExtension::BeginFrame);
		GEngine->GetPostRenderDelegateEx().AddRaw(this, &FExampleIndirectInstancingRendererExtension::EndFrame);
		bInit = true;
	}
}

void FExampleIndirectInstancingRendererExtension::ReleaseRHI()
{
	FScopeLock ScopeLock(&WorkLock);
	Buffers.Empty();
	DiscardIds.Empty();
	SceneProxies.Empty();
	MainViews.Empty();
	CullViews.Empty();
	WorkDescs.Empty();
	bInFrame = false;
}

ExampleIndirectInstancingMesh::FDrawInstanceBuffers FExampleIndirectInstancingRendererExtension::AddWork(FExampleIndirectInstancingSceneProxy const *InProxy, FSceneView const *InMainView, FSceneView const *InCullView)
{
	FScopeLock ScopeLock(&WorkLock);

	// If we hit this then BeginFrame()/EndFrame() logic needs fixing in the Scene Renderer.
	if (!ensure(!bInFrame))
	{
		return ExampleIndirectInstancingMesh::FDrawInstanceBuffers();
	}

	// Create workload
	FWorkDesc WorkDesc;
	WorkDesc.ProxyIndex = SceneProxies.AddUnique(InProxy);
	WorkDesc.MainViewIndex = MainViews.AddUnique(InMainView);
	WorkDesc.CullViewIndex = CullViews.AddUnique(InCullView);
	WorkDesc.BufferIndex = -1;

	// Check for an existing duplicate
	for (FWorkDesc &It : WorkDescs)
	{
		if (It.ProxyIndex == WorkDesc.ProxyIndex && It.MainViewIndex == WorkDesc.MainViewIndex && It.CullViewIndex == WorkDesc.CullViewIndex && It.BufferIndex != -1)
		{
			WorkDesc.BufferIndex = It.BufferIndex;
			break;
		}
	}

	// Try to recycle a buffer
	if (WorkDesc.BufferIndex == -1)
	{
		for (int32 BufferIndex = 0; BufferIndex < Buffers.Num(); BufferIndex++)
		{
			if (DiscardIds[BufferIndex] < DiscardId)
			{
				DiscardIds[BufferIndex] = DiscardId;
				WorkDesc.BufferIndex = BufferIndex;
				WorkDescs.Add(WorkDesc);
				break;
			}
		}
	}

	// Allocate new buffer if necessary
	if (WorkDesc.BufferIndex == -1)
	{
		DiscardIds.Add(DiscardId);
		WorkDesc.BufferIndex = Buffers.AddDefaulted();
		WorkDescs.Add(WorkDesc);
		ExampleIndirectInstancingMesh::InitializeInstanceBuffers(GetImmediateCommandList_ForRenderCommand(), Buffers[WorkDesc.BufferIndex]);
	}

	// Return a copy of RHI refs so callers don't rely on TArray element stability.
	return Buffers[WorkDesc.BufferIndex];
}

void FExampleIndirectInstancingRendererExtension::BeginFrame(FRDGBuilder &GraphBuilder)
{
	bool bHasWork = false;
	{
		FScopeLock ScopeLock(&WorkLock);

		// If we hit this then BeginFrame()/EndFrame() logic needs fixing in the Scene Renderer.
		if (!ensure(!bInFrame))
		{
			// Recover conservatively without re-entering the same lock.
			bInFrame = false;
			SceneProxies.Reset();
			MainViews.Reset();
			CullViews.Reset();
			WorkDescs.Reset();
		}
		bInFrame = true;
		bHasWork = WorkDescs.Num() > 0;
	}

	if (bHasWork)
	{
		SubmitWork(GraphBuilder);
	}
}

void FExampleIndirectInstancingRendererExtension::EndFrame()
{
	FScopeLock ScopeLock(&WorkLock);

	ensure(bInFrame);
	bInFrame = false;

	SceneProxies.Reset();
	MainViews.Reset();
	CullViews.Reset();
	WorkDescs.Reset();

	// Clean the buffer pool
	DiscardId++;

	for (int32 Index = 0; Index < DiscardIds.Num();)
	{
		if (DiscardId - DiscardIds[Index] > 4u)
		{
			ExampleIndirectInstancingMesh::ReleaseInstanceBuffers(Buffers[Index]);
			Buffers.RemoveAtSwap(Index);
			DiscardIds.RemoveAtSwap(Index);
		}
		else
		{
			++Index;
		}
	}
}

void FExampleIndirectInstancingRendererExtension::EndFrame(FRDGBuilder &GraphBuilder)
{
	EndFrame();
}

const static FName NAME_ExampleIndirectInstancing(TEXT("ExampleIndirectInstancing"));

FExampleIndirectInstancingSceneProxy::FExampleIndirectInstancingSceneProxy(UExampleIndirectInstancingComponent *InComponent)
		: FPrimitiveSceneProxy(InComponent, NAME_ExampleIndirectInstancing), VertexFactory(nullptr)
{
	ExampleIndirectInstancingRendererExtension.RegisterExtension();

	bHasDeformableMesh = false;

	UMaterialInterface *ComponentMaterial = InComponent->GetMaterial();
	const bool bValidMaterial = ComponentMaterial != nullptr && ComponentMaterial->CheckMaterialUsage_Concurrent(MATUSAGE_VirtualHeightfieldMesh);
	Material = bValidMaterial ? ComponentMaterial->GetRenderProxy() : UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy();
	MaterialRelevance = Material->GetMaterialInterface()->GetRelevance_Concurrent(GetScene().GetFeatureLevel());

	// Capture LOD 0 render data from the static mesh.
	UStaticMesh* StaticMesh = InComponent->Mesh;
	if (StaticMesh && StaticMesh->GetRenderData() && StaticMesh->GetRenderData()->LODResources.Num() > 0)
	{
		FStaticMeshLODResources& LOD = StaticMesh->GetRenderData()->LODResources[0];
		MeshVertexBuffers = &LOD.VertexBuffers;
		MeshIndexBuffer   = &LOD.IndexBuffer;
		MeshNumIndices    = LOD.IndexBuffer.GetNumIndices();
		MeshNumTexCoords  = LOD.VertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords();
	}

	// -----------------------------------------------------------------------
	// Build CPU instance list.
	//
	// The shader does: mul(float4(LocalPos, 1), InstToWorld)
	// where InstToWorld = float4x4(Row0, Row1, Row2, float4(0,0,0,1)).
	//
	// For mul(row-vector, matrix) the translation must be in the LAST ROW
	// (Row3), but Row3 is hardcoded to (0,0,0,1) in the shader, so we cannot
	// store translation there.
	//
	// The correct approach for a 3×4 "row-major" layout where .w holds
	// translation is to store the TRANSPOSE of the UE FMatrix:
	//
	//   UE FMatrix (row-major, translation at M[3][0..2]):
	//     [ R00  R01  R02  0  ]   row 0
	//     [ R10  R11  R12  0  ]   row 1
	//     [ R20  R21  R22  0  ]   row 2
	//     [ Tx   Ty   Tz   1  ]   row 3  (translation row)
	//
	//   Transposed (translation now in column 3):
	//     [ R00  R10  R20  Tx ]   row 0  → InstanceToWorld0
	//     [ R01  R11  R21  Ty ]   row 1  → InstanceToWorld1
	//     [ R02  R12  R22  Tz ]   row 2  → InstanceToWorld2
	//
	//   mul(float4(px,py,pz,1), Transposed):
	//     result.x = px*R00 + py*R01 + pz*R02 + 1*0   (hardcoded row3)  → wrong col
	//
	// Hmm — still wrong. Let's think again from first principles.
	//
	// mul(v, M) where v is a row vector expands as:
	//   result[j] = sum_i( v[i] * M[i][j] )
	//
	// For result.x = R00*px + R10*py + R20*pz + Tx*1  we need:
	//   M[0][0]=R00, M[1][0]=R10, M[2][0]=R20, M[3][0]=Tx  ← column 0 = X basis + Tx
	//   M[0][1]=R01, M[1][1]=R11, M[2][1]=R21, M[3][1]=Ty  ← column 1 = Y basis + Ty
	//   M[0][2]=R02, M[1][2]=R12, M[2][2]=R22, M[3][2]=Tz  ← column 2 = Z basis + Tz
	//   M[0][3]=0,   M[1][3]=0,   M[2][3]=0,   M[3][3]=1
	//
	// Row3 must be (Tx, Ty, Tz, 1) — but the shader hardcodes Row3=(0,0,0,1).
	// → We MUST change the shader to read Row3 from the instance data, OR
	//   change to mul(M, v) with a column-vector convention.
	//
	// Simplest fix with NO shader changes:
	// Store the matrix in COLUMN-MAJOR order and change the shader to
	// mul(InstToWorld, float4(pos,1)) — but that requires a shader edit too.
	//
	// ACTUAL FIX chosen: store the matrix so Row3 carries the translation
	// by repurposing the existing three float4 rows as the COLUMNS of the
	// standard TRS matrix.  Then change GetInstanceToWorldMatrix in the USH
	// to build the matrix with these as columns (not rows), which makes
	// mul(v, M) work correctly — but that is a shader change.
	//
	// ALTERNATIVE (no shader change): Just store a standard row-major matrix
	// where Row0..2 are the rotation/scale rows and the translation is baked
	// into Row3 — which requires adding a 4th float4 to MeshRenderInstance.
	// That is a struct layout change touching the USH, the compute shader, and
	// this proxy.
	//
	// SIMPLEST CORRECT FIX: Keep the existing 3×float4 struct, but interpret
	// them as COLUMNS.  In GetInstanceToWorldMatrix, construct the float4x4 by
	// passing them as columns, and use mul(InstToWorld, float4(pos,1)) instead.
	// This requires only a one-line change in the USH.
	//
	// We implement that here: MakeRow stores column 0, 1, 2 of the TRS matrix,
	// with the translation baked into the .w component as before — but now
	// they are columns, so .w = translation component for that column (Tx, Ty, Tz).
	// The USH change: float4x4(col0,col1,col2,col3) and mul(M,v).
	// -----------------------------------------------------------------------

	// Capture the component's world transform once.
	// We bake it into every instance matrix so the GPU matrices are true
	// local-to-world transforms.  The vertex shader then outputs world-space
	// positions/normals directly, bypassing the primitive's LocalToWorld.
	const FMatrix44f ComponentToWorld(InComponent->GetComponentTransform().ToMatrixWithScale());

	auto MakeRow = [&ComponentToWorld](const FTransform& T) -> FInstanceTransform
	{
		// Compose: InstanceLocal → ComponentWorld
		// UE FMatrix is row-major with translation at M[3][0..2].
		// We multiply: InstLocal * ComponentToWorld to get InstToWorld.
		const FMatrix44f InstLocal(T.ToMatrixWithScale());
		const FMatrix44f InstToWorld = InstLocal * ComponentToWorld;

		// Store columns of InstToWorld so the shader can do mul(M, col_vec).
		// Col k = (InstToWorld[0][k], InstToWorld[1][k], InstToWorld[2][k], InstToWorld[3][k])
		FInstanceTransform Out;
		Out.Row0 = FVector4f(InstToWorld.M[0][0], InstToWorld.M[1][0], InstToWorld.M[2][0], InstToWorld.M[3][0]); // col 0
		Out.Row1 = FVector4f(InstToWorld.M[0][1], InstToWorld.M[1][1], InstToWorld.M[2][1], InstToWorld.M[3][1]); // col 1
		Out.Row2 = FVector4f(InstToWorld.M[0][2], InstToWorld.M[1][2], InstToWorld.M[2][2], InstToWorld.M[3][2]); // col 2
		return Out;
	};

	if (InComponent->InstanceTransforms.Num() > 0)
	{
		CpuInstanceData.Reserve(InComponent->InstanceTransforms.Num());
		for (const FTransform& T : InComponent->InstanceTransforms)
		{
			CpuInstanceData.Add(MakeRow(T));
		}
	}
	else
	{
		// Single instance at the component's own transform.
		CpuInstanceData.Add(MakeRow(InComponent->GetComponentTransform()));
	}
}

SIZE_T FExampleIndirectInstancingSceneProxy::GetTypeHash() const
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}

uint32 FExampleIndirectInstancingSceneProxy::GetMemoryFootprint() const
{
	return (sizeof(*this) + FPrimitiveSceneProxy::GetAllocatedSize());
}

void FExampleIndirectInstancingSceneProxy::OnTransformChanged(FRHICommandListBase& RHICmdList)
{
}

void FExampleIndirectInstancingSceneProxy::CreateRenderThreadResources(FRHICommandListBase& RHICmdList)
{
	FPrimitiveSceneProxy::CreateRenderThreadResources(RHICmdList);

	FExampleIndirectInstancingParameters UniformParams;

	VertexFactory = new FExampleIndirectInstancingVertexFactory(GetScene().GetFeatureLevel(), UniformParams);

	// Supply mesh LOD buffers before init so InitRHI can build the SRVs.
	if (MeshVertexBuffers && MeshIndexBuffer)
	{
		VertexFactory->SetMeshBuffers(MeshVertexBuffers, MeshIndexBuffer);
	}

	VertexFactory->InitResource(RHICmdList);

	// Upload the CPU instance list to a GPU structured buffer.
	if (CpuInstanceData.Num() > 0)
	{
		const uint32 Stride     = sizeof(FInstanceTransform);
		const uint32 BufferSize = CpuInstanceData.Num() * Stride;

		TResourceArray<FInstanceTransform, VERTEXBUFFER_ALIGNMENT> ResourceArray;
		ResourceArray.Append(CpuInstanceData);

		FRHIResourceCreateInfo CreateInfo(TEXT("ExampleIndirectInstancing.SourceInstanceBuffer"), &ResourceArray);
		SourceInstanceBuffer = RHICmdList.CreateStructuredBuffer(
			Stride, BufferSize,
			BUF_ShaderResource, ERHIAccess::SRVMask, CreateInfo);
		SourceInstanceBufferSRV = RHICmdList.CreateShaderResourceView(SourceInstanceBuffer);
	}

	// Pre-fill the indirect draw args buffer on the CPU.
	// Layout: [IndexCountPerInstance, InstanceCount, StartIndex, BaseVertex, StartInstance]
	// This never changes — no culling, all instances always drawn.
	{
		TResourceArray<uint32, VERTEXBUFFER_ALIGNMENT> Args;
		Args.Add((uint32)MeshNumIndices);              // IndexCountPerInstance
		Args.Add((uint32)CpuInstanceData.Num());       // InstanceCount
		Args.Add(0u);                                  // StartIndexLocation
		Args.Add(0u);                                  // BaseVertexLocation
		Args.Add(0u);                                  // StartInstanceLocation

		FRHIResourceCreateInfo CreateInfo(TEXT("ExampleIndirectInstancing.IndirectArgsBuffer"), &Args);
		IndirectArgsBuffer = RHICmdList.CreateVertexBuffer(
			5 * sizeof(uint32),
			BUF_DrawIndirect | BUF_Static | BUF_UnorderedAccess,
			ERHIAccess::IndirectArgs,
			CreateInfo);
	}
}

void FExampleIndirectInstancingSceneProxy::DestroyRenderThreadResources()
{
	if (VertexFactory != nullptr)
	{
		VertexFactory->ReleaseResource();
		delete VertexFactory;
		VertexFactory = nullptr;
	}

	SourceInstanceBufferSRV.SafeRelease();
	SourceInstanceBuffer.SafeRelease();
	IndirectArgsBuffer.SafeRelease();
}

FPrimitiveViewRelevance FExampleIndirectInstancingSceneProxy::GetViewRelevance(const FSceneView *View) const
{
	const bool bValid = true;
	const bool bIsHiddenInEditor = bHiddenInEditor && View->Family->EngineShowFlags.Editor;

	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = bValid && IsShown(View) && !bIsHiddenInEditor;
	Result.bShadowRelevance = bValid && IsShadowCast(View) && ShouldRenderInMainPass() && !bIsHiddenInEditor;
	Result.bDynamicRelevance = true;
	Result.bStaticRelevance = false;
	Result.bRenderInMainPass = ShouldRenderInMainPass();
	Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
	Result.bRenderCustomDepth = ShouldRenderCustomDepth();
	Result.bTranslucentSelfShadow = false;
	Result.bVelocityRelevance = false;
	MaterialRelevance.SetPrimitiveViewRelevance(Result);
	return Result;
}

void FExampleIndirectInstancingSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView *> &Views, const FSceneViewFamily &ViewFamily, uint32 VisibilityMap, FMeshElementCollector &Collector) const
{
	check(IsInRenderingThread() || IsInParallelRenderingThread());

	if (!MeshIndexBuffer || MeshNumIndices == 0 || !SourceInstanceBufferSRV || !IndirectArgsBuffer)
	{
		return;
	}

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (!(VisibilityMap & (1 << ViewIndex)))
		{
			continue;
		}

		FMeshBatch& Mesh = Collector.AllocateMesh();
		Mesh.bWireframe             = AllowDebugViewmodes() && ViewFamily.EngineShowFlags.Wireframe;
		Mesh.bUseWireframeSelectionColoring = IsSelected();
		Mesh.VertexFactory          = VertexFactory;
		Mesh.MaterialRenderProxy    = Material;
		Mesh.ReverseCulling         = IsLocalToWorldDeterminantNegative();
		Mesh.Type                   = PT_TriangleList;
		Mesh.DepthPriorityGroup     = SDPG_World;
		Mesh.bCanApplyViewModeOverrides = true;
		Mesh.bUseForMaterial        = true;
		Mesh.CastShadow             = true;
		Mesh.bUseForDepthPass       = true;

		FMeshBatchElement& BatchElement = Mesh.Elements[0];
		BatchElement.IndexBuffer        = MeshIndexBuffer;
		BatchElement.IndirectArgsBuffer = IndirectArgsBuffer;
		BatchElement.IndirectArgsOffset = 0;
		BatchElement.FirstIndex         = 0;
		BatchElement.NumPrimitives      = 0; // ignored when IndirectArgsBuffer is set
		BatchElement.MinVertexIndex     = 0;
		BatchElement.MaxVertexIndex     = 0;
		BatchElement.PrimitiveIdMode        = PrimID_ForceZero;
		BatchElement.PrimitiveUniformBuffer = GetUniformBuffer();

		FExampleIndirectInstancingUserData* UserData = &Collector.AllocateOneFrameResource<FExampleIndirectInstancingUserData>();
		BatchElement.UserData = UserData;

		UserData->InstanceBufferSRV = SourceInstanceBufferSRV.GetReference();
		UserData->PositionBufferSRV = VertexFactory->PositionBufferSRV.GetReference();
		UserData->TangentBufferSRV  = VertexFactory->TangentBufferSRV.GetReference();
		UserData->UV0BufferSRV      = VertexFactory->UV0BufferSRV.GetReference();
		UserData->NumTexCoords      = MeshNumTexCoords;
		UserData->LodViewOrigin     = (FVector3f)ViewFamily.Views[0]->ViewMatrices.GetViewOrigin();

		Collector.AddMesh(ViewIndex, Mesh);
	}
}

namespace ExampleIndirectInstancingMesh
{
	/* Keep indirect args offsets in sync with ISM.usf. */
	static const int32 IndirectArgsByteOffset_FinalCull = 0;
	static const int32 IndirectArgsByteSize = 5 * sizeof(uint32);

	struct WorkerQueueInfo
	{
		uint32 Read;
		uint32 Write;
		int32 NumActive;
	};

	struct FExampleIndirectInstancingRenderInstance
	{
		FVector4f Row0;
		FVector4f Row1;
		FVector4f Row2;
	};

	class FInitBuffersVHM_CS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FInitBuffersVHM_CS);
		SHADER_USE_PARAMETER_STRUCT(FInitBuffersVHM_CS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, MaxLevel)
		SHADER_PARAMETER(uint32, NumForceLoadLods)
		SHADER_PARAMETER(uint32, PageTableFeedbackId)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<WorkerQueueInfo>, RWQueueInfo)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWQueueBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint2>, RWQuadBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWIndirectArgsBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWFeedbackBuffer)
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(FGlobalShaderPermutationParameters const &Parameters)
		{
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
		}
	};

	IMPLEMENT_GLOBAL_SHADER(FInitBuffersVHM_CS, "/IndirectInstancingShaders/ExampleIndirectInstancing/ExampleIndirectInstancingCompute.usf", "InitBuffersCS", SF_Compute);

	class FCollectQuadsVHM_CS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FCollectQuadsVHM_CS);
		SHADER_USE_PARAMETER_STRUCT(FCollectQuadsVHM_CS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_TEXTURE(Texture2D, HeightMinMaxTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, MinMaxTextureSampler)
		SHADER_PARAMETER(int32, MinMaxLevelOffset)
		SHADER_PARAMETER_TEXTURE(Texture2D, LodBiasMinMaxTexture)
		SHADER_PARAMETER_TEXTURE(Texture2D<float>, OcclusionTexture)
		SHADER_PARAMETER(int32, OcclusionLevelOffset)
		SHADER_PARAMETER_TEXTURE(Texture2D<uint>, PageTableTexture)
		SHADER_PARAMETER(uint32, MaxLevel)
		SHADER_PARAMETER(FVector4f, PageTableSize)
		SHADER_PARAMETER(uint32, PageTableFeedbackId)
		SHADER_PARAMETER(FVector4f, LodDistances)
		SHADER_PARAMETER(float, LodBiasScale)
		SHADER_PARAMETER(FVector3f, ViewOrigin)
		SHADER_PARAMETER_ARRAY(FVector4f, FrustumPlanes, [5])
		SHADER_PARAMETER(FMatrix44f, UVToWorld)
		SHADER_PARAMETER(FVector3f, UVToWorldScale)
		SHADER_PARAMETER(uint32, QueueBufferSizeMask)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<WorkerQueueInfo>, RWQueueInfo)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWQueueBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint2>, RWQuadBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWIndirectArgsBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWFeedbackBuffer)
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(FGlobalShaderPermutationParameters const &Parameters)
		{
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
		}
	};

	IMPLEMENT_GLOBAL_SHADER(FCollectQuadsVHM_CS, "/IndirectInstancingShaders/ExampleIndirectInstancing/ExampleIndirectInstancingCompute.usf", "CollectQuadsCS", SF_Compute);

	class FInitInstanceBufferVHM_CS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FInitInstanceBufferVHM_CS);
		SHADER_USE_PARAMETER_STRUCT(FInitInstanceBufferVHM_CS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(int32, NumIndices)
		SHADER_PARAMETER(uint32, NumSourceInstances)
		SHADER_PARAMETER_UAV(RWBuffer<uint>, RWIndirectArgsBuffer)
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(FGlobalShaderPermutationParameters const &Parameters)
		{
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
		}
	};

	IMPLEMENT_GLOBAL_SHADER(FInitInstanceBufferVHM_CS, "/IndirectInstancingShaders/ExampleIndirectInstancing/ExampleIndirectInstancingCompute.usf", "InitInstanceBufferCS", SF_Compute);

	class FCullInstancesVHM_CS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FCullInstancesVHM_CS);
		SHADER_USE_PARAMETER_STRUCT(FCullInstancesVHM_CS, FGlobalShader);

		class FReuseCullDim : SHADER_PERMUTATION_BOOL("REUSE_CULL");

		using FPermutationDomain = TShaderPermutationDomain<FReuseCullDim>;

		static bool ShouldCompilePermutation(FGlobalShaderPermutationParameters const &Parameters)
		{
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
		}

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_TEXTURE(Texture2D, HeightMinMaxTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, MinMaxTextureSampler)
		SHADER_PARAMETER(int32, MinMaxLevelOffset)
		SHADER_PARAMETER_TEXTURE(Texture2D, PageTableTexture)
		SHADER_PARAMETER(FVector4f, PageTableSize)
		SHADER_PARAMETER_ARRAY(FVector4f, FrustumPlanes, [5])
		SHADER_PARAMETER(FVector4f, PhysicalPageTransform)
		SHADER_PARAMETER(uint32, NumPhysicalAddressBits)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint2>, QuadBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, IndirectArgsBufferSRV)
		SHADER_PARAMETER_SRV(StructuredBuffer<ExampleIndirectInstancingMesh::FExampleIndirectInstancingRenderInstance>, SourceInstanceBuffer)
		SHADER_PARAMETER(uint32, NumSourceInstances)
		SHADER_PARAMETER_UAV(RWStructuredBuffer<ExampleIndirectInstancingMesh::FExampleIndirectInstancingRenderInstance>, RWInstanceBuffer)
		SHADER_PARAMETER_UAV(RWBuffer<uint>, RWIndirectArgsBuffer)
		RDG_BUFFER_ACCESS(IndirectArgsBuffer, ERHIAccess::IndirectArgs)
		END_SHADER_PARAMETER_STRUCT()
	};

	IMPLEMENT_GLOBAL_SHADER(FCullInstancesVHM_CS, "/IndirectInstancingShaders/ExampleIndirectInstancing/ExampleIndirectInstancingCompute.usf", "CullInstancesCS", SF_Compute);

	class FHeightMinMaxDefaultTexture : public FTexture
	{
	public:
		virtual void InitRHI(FRHICommandListBase &RHICmdList) override
		{
			const FRHITextureCreateDesc Desc =
				FRHITextureCreateDesc::Create2D(TEXT("ISM.MinMaxDefaultTexture"), 1, 1, PF_B8G8R8A8)
				.SetFlags(ETextureCreateFlags::ShaderResource);
			TextureRHI = RHICreateTexture(Desc);

			uint32 DestStride;
			FColor *DestBuffer = (FColor *)RHILockTexture2D(TextureRHI, 0, RLM_WriteOnly, DestStride, false);
			*DestBuffer = FColor(0, 0, 255, 255);
			RHIUnlockTexture2D(TextureRHI, 0, false);

			FSamplerStateInitializerRHI SamplerStateInitializer(SF_Point, AM_Clamp, AM_Clamp, AM_Clamp);
			SamplerStateRHI = GetOrCreateSamplerState(SamplerStateInitializer);
		}

		virtual uint32 GetSizeX() const override { return 1; }
		virtual uint32 GetSizeY() const override { return 1; }
	};

	FTexture *GHeightMinMaxDefaultTexture = new TGlobalResource<FHeightMinMaxDefaultTexture>;

	struct FViewData
	{
		FVector ViewOrigin;
		FMatrix ProjectionMatrix;
		FConvexVolume ViewFrustum;
		bool bViewFrozen;
	};

	void GetViewData(FSceneView const *InSceneView, FViewData &OutViewData)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		const FViewMatrices *FrozenViewMatrices = InSceneView->State != nullptr ? InSceneView->State->GetFrozenViewMatrices() : nullptr;
		if (FrozenViewMatrices != nullptr)
		{
			OutViewData.ViewOrigin = FrozenViewMatrices->GetViewOrigin();
			OutViewData.ProjectionMatrix = FrozenViewMatrices->GetProjectionMatrix();
			GetViewFrustumBounds(OutViewData.ViewFrustum, FrozenViewMatrices->GetViewProjectionMatrix(), true);
			OutViewData.bViewFrozen = true;
		}
		else
#endif
		{
			OutViewData.ViewOrigin = InSceneView->ViewMatrices.GetViewOrigin();
			OutViewData.ProjectionMatrix = InSceneView->ViewMatrices.GetProjectionMatrix();
			OutViewData.ViewFrustum = InSceneView->ViewFrustum;
			OutViewData.bViewFrozen = false;
		}
	}

	struct FProxyDesc
	{
		FRHITexture *PageTableTexture;
		FRHITexture *HeightMinMaxTexture;
		FRHITexture *LodBiasMinMaxTexture;
		int32 MinMaxLevelOffset;

		uint32 MaxLevel;
		uint32 NumForceLoadLods;
		uint32 PageTableFeedbackId;
		uint32 NumPhysicalAddressBits;
		FVector4 PageTableSize;
		FVector4 PhysicalPageTransform;
		FMatrix UVToWorld;
		FVector UVToWorldScale;
		uint32 NumQuadsPerTileSide;

		int32 MaxPersistentQueueItems;
		int32 MaxRenderItems;
		int32 MaxFeedbackItems;
		int32 NumCollectPassWavefronts;
	};

	struct FMainViewDesc
	{
		FSceneView const *ViewDebug;
		FVector ViewOrigin;
		FVector4 LodDistances;
		float LodBiasScale;
		FVector4 Planes[5];
		FTextureRHIRef OcclusionTexture;
		int32 OcclusionLevelOffset;
	};

	struct FChildViewDesc
	{
		FSceneView const *ViewDebug;
		bool bIsMainView;
		FVector4 Planes[5];
	};

	struct FVolatileResources
	{
		FRDGBufferRef QueueInfo;
		FRDGBufferUAVRef QueueInfoUAV;
		FRDGBufferRef QueueBuffer;
		FRDGBufferUAVRef QueueBufferUAV;

		FRDGBufferRef QuadBuffer;
		FRDGBufferUAVRef QuadBufferUAV;
		FRDGBufferSRVRef QuadBufferSRV;

		FRDGBufferRef FeedbackBuffer;
		FRDGBufferUAVRef FeedbackBufferUAV;

		FRDGBufferRef IndirectArgsBuffer;
		FRDGBufferUAVRef IndirectArgsBufferUAV;
		FRDGBufferSRVRef IndirectArgsBufferSRV;
	};

	void InitializeInstanceBuffers(FRHICommandListBase& RHICmdList, FDrawInstanceBuffers& InBuffers)
	{
		{
			FRHIResourceCreateInfo CreateInfo(TEXT("FExampleIndirectInstancing.InstanceBuffer"));
			const uint32 InstanceSize = sizeof(ExampleIndirectInstancingMesh::FExampleIndirectInstancingRenderInstance);
			const uint32 InstanceBufferSize = 1024u * 4u * InstanceSize;
			InBuffers.InstanceBuffer = RHICmdList.CreateStructuredBuffer(InstanceSize, InstanceBufferSize, BUF_UnorderedAccess | BUF_ShaderResource, ERHIAccess::SRVMask, CreateInfo);
			InBuffers.InstanceBufferUAV = RHICmdList.CreateUnorderedAccessView(InBuffers.InstanceBuffer, false, false);
			InBuffers.InstanceBufferSRV = RHICmdList.CreateShaderResourceView(InBuffers.InstanceBuffer);
		}
		{
			FRHIResourceCreateInfo CreateInfo(TEXT("FExampleIndirectInstancing.InstanceIndirectArgsBuffer"));
			InBuffers.IndirectArgsBuffer = RHICmdList.CreateVertexBuffer(5 * sizeof(uint32), BUF_UnorderedAccess | BUF_DrawIndirect, ERHIAccess::IndirectArgs, CreateInfo);
			InBuffers.IndirectArgsBufferUAV = RHICmdList.CreateUnorderedAccessView(InBuffers.IndirectArgsBuffer, PF_R32_UINT);
		}
	}

	void InitializeResources(FRDGBuilder & GraphBuilder, FProxyDesc const &InDesc, FMainViewDesc const &InMainViewDesc, FVolatileResources &OutResources)
	{
		OutResources.QueueInfo = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(WorkerQueueInfo), 1), TEXT("ExampleIndirectInstancingMesh.QueueInfo"));
		OutResources.QueueInfoUAV = GraphBuilder.CreateUAV(OutResources.QueueInfo);
		OutResources.QueueBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), InDesc.MaxPersistentQueueItems), TEXT("ExampleIndirectInstancingMesh.QuadQueue"));
		OutResources.QueueBufferUAV = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(OutResources.QueueBuffer, PF_R32_UINT));

		OutResources.QuadBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32) * 2, InDesc.MaxRenderItems), TEXT("ExampleIndirectInstancingMesh.QuadBuffer"));
		OutResources.QuadBufferUAV = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(OutResources.QuadBuffer, PF_R32G32_UINT));
		OutResources.QuadBufferSRV = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(OutResources.QuadBuffer, PF_R32G32_UINT));

		FRDGBufferDesc FeedbackBufferDesc = FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), InDesc.MaxFeedbackItems + 1);
		FeedbackBufferDesc.Usage = EBufferUsageFlags(FeedbackBufferDesc.Usage | BUF_SourceCopy);
		OutResources.FeedbackBuffer = GraphBuilder.CreateBuffer(FeedbackBufferDesc, TEXT("ExampleIndirectInstancingMesh.FeedbackBuffer"));
		OutResources.FeedbackBufferUAV = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(OutResources.FeedbackBuffer, PF_R32_UINT));

		OutResources.IndirectArgsBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc(IndirectArgsByteSize), TEXT("ExampleIndirectInstancingMesh.IndirectArgsBuffer"));
		OutResources.IndirectArgsBufferUAV = GraphBuilder.CreateUAV(OutResources.IndirectArgsBuffer);
		OutResources.IndirectArgsBufferSRV = GraphBuilder.CreateSRV(OutResources.IndirectArgsBuffer);
	}

	void AddPass_TransitionAllDrawBuffers(FRDGBuilder & GraphBuilder, TArray<ExampleIndirectInstancingMesh::FDrawInstanceBuffers> const &Buffers, TArrayView<int32> const &BufferIndices, bool bToWrite)
	{
		TArray<FRHIUnorderedAccessView *> OverlapUAVs;
		OverlapUAVs.Reserve(BufferIndices.Num());

		TArray<FRHITransitionInfo> TransitionInfos;
		TransitionInfos.Reserve(BufferIndices.Num() * 2);

		for (int32 BufferIndex : BufferIndices)
		{
			FRHIUnorderedAccessView *IndirectArgsBufferUAV = Buffers[BufferIndex].IndirectArgsBufferUAV;
			FRHIUnorderedAccessView *InstanceBufferUAV = Buffers[BufferIndex].InstanceBufferUAV;

			OverlapUAVs.Add(IndirectArgsBufferUAV);

			TransitionInfos.Add(FRHITransitionInfo(IndirectArgsBufferUAV, bToWrite ? ERHIAccess::IndirectArgs : ERHIAccess::UAVMask, bToWrite ? ERHIAccess::UAVMask : ERHIAccess::IndirectArgs));
			TransitionInfos.Add(FRHITransitionInfo(InstanceBufferUAV, bToWrite ? ERHIAccess::SRVMask : ERHIAccess::UAVMask, bToWrite ? ERHIAccess::UAVMask : ERHIAccess::SRVMask));
		}

		AddPass(GraphBuilder, RDG_EVENT_NAME("TransitionAllDrawBuffers"), [bToWrite, OverlapUAVs, TransitionInfos](FRHICommandList &InRHICmdList)
						{
			if (!bToWrite)
			{
				InRHICmdList.EndUAVOverlap(OverlapUAVs);
			}

			InRHICmdList.Transition(TransitionInfos);
			
			if (bToWrite)
			{
				InRHICmdList.BeginUAVOverlap(OverlapUAVs);
			} });
	}

	void AddPass_InitBuffers(FRDGBuilder & GraphBuilder, FGlobalShaderMap * InGlobalShaderMap, FProxyDesc const &InDesc, FVolatileResources &InVolatileResources)
	{
		TShaderMapRef<FInitBuffersVHM_CS> ComputeShader(InGlobalShaderMap);

		FInitBuffersVHM_CS::FParameters *PassParameters = GraphBuilder.AllocParameters<FInitBuffersVHM_CS::FParameters>();
		PassParameters->MaxLevel = InDesc.MaxLevel;
		PassParameters->NumForceLoadLods = InDesc.NumForceLoadLods;
		PassParameters->PageTableFeedbackId = InDesc.PageTableFeedbackId;
		PassParameters->RWQueueInfo = InVolatileResources.QueueInfoUAV;
		PassParameters->RWQueueBuffer = InVolatileResources.QueueBufferUAV;
		PassParameters->RWQuadBuffer = InVolatileResources.QuadBufferUAV;
		PassParameters->RWIndirectArgsBuffer = InVolatileResources.IndirectArgsBufferUAV;
		PassParameters->RWFeedbackBuffer = InVolatileResources.FeedbackBufferUAV;

		GraphBuilder.AddPass(
				RDG_EVENT_NAME("InitBuffers"),
				PassParameters,
				ERDGPassFlags::Compute,
				[PassParameters, ComputeShader](FRHICommandList &RHICmdList)
				{
					RHICmdList.ClearUAVUint(PassParameters->RWFeedbackBuffer->GetRHI(), FUintVector4(0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff));
					FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *PassParameters, FIntVector(1, 1, 1));
				});
	}

	void AddPass_CollectQuads(FRDGBuilder & GraphBuilder, FGlobalShaderMap * InGlobalShaderMap, FProxyDesc const &InDesc, FVolatileResources &InVolatileResources, FMainViewDesc const &InViewDesc)
	{
		TShaderMapRef<FCollectQuadsVHM_CS> ComputeShader(InGlobalShaderMap);

		FCollectQuadsVHM_CS::FParameters *PassParameters = GraphBuilder.AllocParameters<FCollectQuadsVHM_CS::FParameters>();
		PassParameters->HeightMinMaxTexture = InDesc.HeightMinMaxTexture;
		PassParameters->LodBiasMinMaxTexture = InDesc.LodBiasMinMaxTexture;
		PassParameters->MinMaxTextureSampler = TStaticSamplerState<SF_Point>::GetRHI();
		PassParameters->MinMaxLevelOffset = InDesc.MinMaxLevelOffset;
		PassParameters->OcclusionTexture = InViewDesc.OcclusionTexture;
		PassParameters->OcclusionLevelOffset = InViewDesc.OcclusionLevelOffset;
		PassParameters->PageTableTexture = InDesc.PageTableTexture;
		PassParameters->MaxLevel = InDesc.MaxLevel;
		PassParameters->PageTableSize = FVector4f(InDesc.PageTableSize);
		PassParameters->PageTableFeedbackId = InDesc.PageTableFeedbackId;
		PassParameters->UVToWorld = FMatrix44f(InDesc.UVToWorld);
		PassParameters->UVToWorldScale = (FVector3f)InDesc.UVToWorldScale;
		PassParameters->ViewOrigin = (FVector3f)InViewDesc.ViewOrigin;
		PassParameters->LodDistances = FVector4f(InViewDesc.LodDistances);
		PassParameters->LodBiasScale = InViewDesc.LodBiasScale;
		for (int32 PlaneIndex = 0; PlaneIndex < 5; ++PlaneIndex)
		{
			PassParameters->FrustumPlanes[PlaneIndex] = FVector4f(InViewDesc.Planes[PlaneIndex]);
		}
		PassParameters->QueueBufferSizeMask = InDesc.MaxPersistentQueueItems - 1;
		PassParameters->RWQueueInfo = InVolatileResources.QueueInfoUAV;
		PassParameters->RWQueueBuffer = InVolatileResources.QueueBufferUAV;
		PassParameters->RWQuadBuffer = InVolatileResources.QuadBufferUAV;
		PassParameters->RWIndirectArgsBuffer = InVolatileResources.IndirectArgsBufferUAV;
		PassParameters->RWFeedbackBuffer = InVolatileResources.FeedbackBufferUAV;

		FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("CollectQuads"),
				ComputeShader, PassParameters, FIntVector(InDesc.NumCollectPassWavefronts, 1, 1));
	}

	void AddPass_InitInstanceBuffer(FRDGBuilder & GraphBuilder, FGlobalShaderMap * InGlobalShaderMap, FDrawInstanceBuffers & InOutputResources, int32 InNumIndices, uint32 InNumSourceInstances)
	{
		TShaderMapRef<FInitInstanceBufferVHM_CS> ComputeShader(InGlobalShaderMap);

		FInitInstanceBufferVHM_CS::FParameters *PassParameters = GraphBuilder.AllocParameters<FInitInstanceBufferVHM_CS::FParameters>();
		PassParameters->NumIndices          = InNumIndices;
		PassParameters->NumSourceInstances  = InNumSourceInstances;
		PassParameters->RWIndirectArgsBuffer = InOutputResources.IndirectArgsBufferUAV;

		FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("InitInstanceBuffer"),
				ComputeShader, PassParameters, FIntVector(1, 1, 1));
	}

	void AddPass_CullInstances(FRDGBuilder & GraphBuilder, FGlobalShaderMap * InGlobalShaderMap, FProxyDesc const &InDesc, FVolatileResources &InVolatileResources, FDrawInstanceBuffers &InOutputResources, FChildViewDesc const &InViewDesc, FRHIShaderResourceView* InSourceInstanceBufferSRV, uint32 InNumSourceInstances)
	{
		FCullInstancesVHM_CS::FParameters *PassParameters = GraphBuilder.AllocParameters<FCullInstancesVHM_CS::FParameters>();

		PassParameters->QuadBuffer            = InVolatileResources.QuadBufferSRV;
		PassParameters->IndirectArgsBuffer    = InVolatileResources.IndirectArgsBuffer;
		PassParameters->IndirectArgsBufferSRV = InVolatileResources.IndirectArgsBufferSRV;
		PassParameters->SourceInstanceBuffer  = InSourceInstanceBufferSRV;
		PassParameters->NumSourceInstances    = InNumSourceInstances;
		PassParameters->RWInstanceBuffer      = InOutputResources.InstanceBufferUAV;
		PassParameters->RWIndirectArgsBuffer  = InOutputResources.IndirectArgsBufferUAV;

		for (int32 PlaneIndex = 0; PlaneIndex < 5; ++PlaneIndex)
		{
			PassParameters->FrustumPlanes[PlaneIndex] = FVector4f(InViewDesc.Planes[PlaneIndex]);
		}

		PassParameters->HeightMinMaxTexture   = ExampleIndirectInstancingMesh::GHeightMinMaxDefaultTexture->TextureRHI;
		PassParameters->MinMaxTextureSampler  = TStaticSamplerState<SF_Point>::GetRHI();
		PassParameters->MinMaxLevelOffset     = 0;
		PassParameters->PageTableTexture      = ExampleIndirectInstancingMesh::GHeightMinMaxDefaultTexture->TextureRHI;
		PassParameters->PageTableSize         = FVector4f(1.f, 1.f, 1.f, 1.f);
		PassParameters->PhysicalPageTransform = FVector4f(0.f, 0.f, 1.f, 1.f);
		PassParameters->NumPhysicalAddressBits = 0;

		FCullInstancesVHM_CS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FCullInstancesVHM_CS::FReuseCullDim>(InViewDesc.bIsMainView);

		TShaderMapRef<FCullInstancesVHM_CS> ComputeShader(InGlobalShaderMap, PermutationVector);

		const uint32 NumGroups = FMath::DivideAndRoundUp(InNumSourceInstances, 64u);
		FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("CullInstances"),
				ComputeShader, PassParameters,
				FIntVector((int32)NumGroups, 1, 1));
	}
}

void FExampleIndirectInstancingRendererExtension::SubmitWork(FRDGBuilder &GraphBuilder)
{
	WorkDescs.Sort(FWorkDescSort());

	TArray<int32, TInlineAllocator<8>> UsedBufferIndices;
	for (FWorkDesc WorkdDesc : WorkDescs)
	{
		UsedBufferIndices.Add(WorkdDesc.BufferIndex);
	}
	AddPass_TransitionAllDrawBuffers(GraphBuilder, Buffers, UsedBufferIndices, true);

	for (FWorkDesc WorkDesc : WorkDescs)
	{
		FExampleIndirectInstancingSceneProxy const* InitProxy = SceneProxies[WorkDesc.ProxyIndex];
		AddPass_InitInstanceBuffer(GraphBuilder, GetGlobalShaderMap(GMaxRHIFeatureLevel), Buffers[WorkDesc.BufferIndex], InitProxy->MeshNumIndices, (uint32)InitProxy->CpuInstanceData.Num());
	}

	const int32 NumWorkItems = WorkDescs.Num();
	int32 WorkIndex = 0;
	while (WorkIndex < NumWorkItems)
	{
		FExampleIndirectInstancingSceneProxy const *Proxy = SceneProxies[WorkDescs[WorkIndex].ProxyIndex];

		ExampleIndirectInstancingMesh::FProxyDesc ProxyDesc;
		ProxyDesc.PageTableTexture = nullptr;
		ProxyDesc.HeightMinMaxTexture = ExampleIndirectInstancingMesh::GHeightMinMaxDefaultTexture->TextureRHI;
		ProxyDesc.LodBiasMinMaxTexture = ExampleIndirectInstancingMesh::GHeightMinMaxDefaultTexture->TextureRHI;
		ProxyDesc.MinMaxLevelOffset = 0;
		ProxyDesc.MaxLevel = 0;
		ProxyDesc.NumForceLoadLods = 0;
		ProxyDesc.PageTableFeedbackId = 0;
		ProxyDesc.NumPhysicalAddressBits = 0;
		ProxyDesc.PageTableSize = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
		ProxyDesc.PhysicalPageTransform = FVector4(0.0f, 0.0f, 1.0f, 1.0f);
		ProxyDesc.UVToWorld = FMatrix::Identity;
		ProxyDesc.UVToWorldScale = FVector(1.0f, 1.0f, 1.0f);
		ProxyDesc.NumQuadsPerTileSide = 1;
		ProxyDesc.MaxPersistentQueueItems = 1 << FMath::CeilLogTwo(1024 * 4);
		ProxyDesc.MaxRenderItems = 1024 * 4;
		ProxyDesc.MaxFeedbackItems = ProxyDesc.MaxRenderItems;
		ProxyDesc.NumCollectPassWavefronts = 16;

		while (WorkIndex < NumWorkItems && SceneProxies[WorkDescs[WorkIndex].ProxyIndex] == Proxy)
		{
			FSceneView const *MainView = MainViews[WorkDescs[WorkIndex].MainViewIndex];

			ExampleIndirectInstancingMesh::FViewData MainViewData;
			ExampleIndirectInstancingMesh::GetViewData(MainView, MainViewData);

			ExampleIndirectInstancingMesh::FMainViewDesc MainViewDesc;
			MainViewDesc.ViewDebug = MainView;
			MainViewDesc.LodDistances = FVector4(0.0f, 0.0f, 0.0f, 0.0f);
			MainViewDesc.LodBiasScale = 1.0f;
			MainViewDesc.OcclusionTexture = nullptr;
			MainViewDesc.OcclusionLevelOffset = 0;
			MainViewDesc.ViewOrigin = MainViewData.ViewOrigin;
			for (int32 PlaneIndex = 0; PlaneIndex < 5; ++PlaneIndex)
			{
				if (PlaneIndex < MainViewData.ViewFrustum.Planes.Num())
				{
					const FPlane& Plane = MainViewData.ViewFrustum.Planes[PlaneIndex];
					MainViewDesc.Planes[PlaneIndex] = FVector4(Plane.X, Plane.Y, Plane.Z, Plane.W);
				}
				else
				{
					MainViewDesc.Planes[PlaneIndex] = FVector4(0.0f, 0.0f, 0.0f, 1.0f);
				}
			}

			ExampleIndirectInstancingMesh::FVolatileResources VolatileResources;
			ExampleIndirectInstancingMesh::InitializeResources(GraphBuilder, ProxyDesc, MainViewDesc, VolatileResources);

			ExampleIndirectInstancingMesh::AddPass_InitBuffers(GraphBuilder, GetGlobalShaderMap(GMaxRHIFeatureLevel), ProxyDesc, VolatileResources);
			ExampleIndirectInstancingMesh::AddPass_CollectQuads(GraphBuilder, GetGlobalShaderMap(GMaxRHIFeatureLevel), ProxyDesc, VolatileResources, MainViewDesc);
			while (WorkIndex < NumWorkItems && MainViews[WorkDescs[WorkIndex].MainViewIndex] == MainView)
			{
				FSceneView const *CullView = CullViews[WorkDescs[WorkIndex].CullViewIndex];
				FConvexVolume const *ShadowFrustum = CullView->GetDynamicMeshElementsShadowCullFrustum();
				FConvexVolume const &Frustum = ShadowFrustum != nullptr && ShadowFrustum->Planes.Num() > 0 ? *ShadowFrustum : CullView->ViewFrustum;
				const FVector PreShadowTranslation = ShadowFrustum != nullptr ? CullView->GetPreShadowTranslation() : FVector::ZeroVector;

				ExampleIndirectInstancingMesh::FChildViewDesc ChildViewDesc;
				ChildViewDesc.ViewDebug = MainView;
				ChildViewDesc.bIsMainView = CullView == MainView;
				for (int32 PlaneIndex = 0; PlaneIndex < 5; ++PlaneIndex)
				{
					if (PlaneIndex < Frustum.Planes.Num())
					{
						const FPlane& Plane = Frustum.Planes[PlaneIndex];
						ChildViewDesc.Planes[PlaneIndex] = FVector4(Plane.X, Plane.Y, Plane.Z, Plane.W);
					}
					else
					{
						ChildViewDesc.Planes[PlaneIndex] = FVector4(0.0f, 0.0f, 0.0f, 1.0f);
					}
				}

				ExampleIndirectInstancingMesh::AddPass_CullInstances(
					GraphBuilder,
					GetGlobalShaderMap(GMaxRHIFeatureLevel),
					ProxyDesc, VolatileResources,
					Buffers[WorkDescs[WorkIndex].BufferIndex],
					ChildViewDesc,
					Proxy->SourceInstanceBufferSRV.GetReference(),
					(uint32)Proxy->CpuInstanceData.Num());

				WorkIndex++;
			}
		}
	}

	AddPass_TransitionAllDrawBuffers(GraphBuilder, Buffers, UsedBufferIndices, false);
}
