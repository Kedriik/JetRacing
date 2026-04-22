#include "ComputeDrivenIndirectInstancingComponent.h"
#include "ComputeDrivenInstancingVertexFactory.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "Materials/MaterialRenderProxy.h"
#include "MaterialDomain.h"
#include "MeshBatch.h"
#include "PrimitiveSceneProxy.h"
#include "SceneManagement.h"
#include "StaticMeshResources.h"
#include "RHICommandList.h"
#include "RHIResources.h"

// ============================================================================
// Per-section data captured at proxy construction time (game thread)
// ============================================================================
struct FComputeDrivenSection
{
	FMaterialRenderProxy* MaterialProxy   = nullptr;
	FMaterialRelevance    MaterialRelevance;
	uint32                FirstIndex      = 0;
	uint32                NumIndices      = 0;
	int32                 MinVertexIndex  = 0;
	int32                 MaxVertexIndex  = 0;
};

// ============================================================================
// Scene proxy
// ============================================================================
class FComputeDrivenIndirectInstancingSceneProxy final : public FPrimitiveSceneProxy
{
public:
	FComputeDrivenIndirectInstancingSceneProxy(UComputeDrivenIndirectInstancingComponent* InComponent)
		: FPrimitiveSceneProxy(InComponent, FName("ComputeDrivenIndirectInstancing"))
		, VertexFactory(nullptr)
		, MeshIndexBuffer(nullptr)
		, MeshVertexBuffers(nullptr)
		, MeshNumTexCoords(1)
		, NumInstancesSnapshot(InComponent->MaxInstances)
		, SharedBuffers(InComponent->GpuBuffers)  // hold the same TSharedPtr
	{
		UStaticMesh* StaticMesh = InComponent->Mesh;
		if (!StaticMesh || !StaticMesh->GetRenderData() ||
			StaticMesh->GetRenderData()->LODResources.Num() == 0)
			return;

		FStaticMeshLODResources& LOD = StaticMesh->GetRenderData()->LODResources[0];
		MeshVertexBuffers = &LOD.VertexBuffers;
		MeshIndexBuffer   = &LOD.IndexBuffer;
		MeshNumTexCoords  = LOD.VertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords();

		for (int32 i = 0; i < LOD.Sections.Num(); i++)
		{
			const FStaticMeshSection& MS = LOD.Sections[i];
			UMaterialInterface* Mat = StaticMesh->GetMaterial(MS.MaterialIndex);
			if (!Mat) Mat = UMaterial::GetDefaultMaterial(MD_Surface);

			FComputeDrivenSection& S = Sections.AddDefaulted_GetRef();
			S.MaterialProxy     = Mat->GetRenderProxy();
			S.MaterialRelevance = Mat->GetRelevance_Concurrent(GetScene().GetFeatureLevel());
			S.FirstIndex        = MS.FirstIndex;
			S.NumIndices        = MS.NumTriangles * 3;
			S.MinVertexIndex    = MS.MinVertexIndex;
			S.MaxVertexIndex    = MS.MaxVertexIndex;
			CombinedMaterialRelevance |= S.MaterialRelevance;
		}
	}

	virtual SIZE_T GetTypeHash() const override
	{
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}

	virtual uint32 GetMemoryFootprint() const override
	{
		return sizeof(*this) + FPrimitiveSceneProxy::GetAllocatedSize();
	}

	virtual void CreateRenderThreadResources(FRHICommandListBase& RHICmdList) override
	{
		FPrimitiveSceneProxy::CreateRenderThreadResources(RHICmdList);

		// Vertex factory
		FComputeDrivenInstancingParameters UniformParams;
		VertexFactory = new FComputeDrivenInstancingVertexFactory(
			GetScene().GetFeatureLevel(), UniformParams);
		if (MeshVertexBuffers && MeshIndexBuffer)
			VertexFactory->SetMeshBuffers(MeshVertexBuffers, MeshIndexBuffer);
		VertexFactory->InitResource(RHICmdList);

		// All GPU buffers are created here on the render thread and stored in the
		// shared block.  We never touch the UObject component pointer from here —
		// the component may be GC-moved at any time.  The TSharedPtr keeps the
		// block alive even if the component or proxy is destroyed first.
		check(SharedBuffers.IsValid());

		const uint32 InstanceStride     = 3 * sizeof(FVector4f);
		const uint32 InstanceBufferSize = (uint32)NumInstancesSnapshot * InstanceStride;

		// Instance buffer
		{
			FRHIResourceCreateInfo CI(TEXT("ComputeDriven.InstanceBuffer"));
			SharedBuffers->InstanceBuffer = RHICmdList.CreateStructuredBuffer(
				InstanceStride, InstanceBufferSize,
				BUF_UnorderedAccess | BUF_ShaderResource,
				ERHIAccess::SRVMask, CI);
			SharedBuffers->InstanceBufferUAV = RHICmdList.CreateUnorderedAccessView(
				SharedBuffers->InstanceBuffer, false, false);
			SharedBuffers->InstanceBufferSRV = RHICmdList.CreateShaderResourceView(
				SharedBuffers->InstanceBuffer);
		}

		// IndirectArgs + reset buffers
		const int32 NumSections = Sections.Num();
		SharedBuffers->IndirectArgsBuffers.SetNum(NumSections);
		SharedBuffers->IndirectArgsBufferUAVs.SetNum(NumSections);
		SharedBuffers->IndirectArgsResetBuffers.SetNum(NumSections);

		int32 TotalIndices = 0;
		for (int32 i = 0; i < NumSections; i++)
		{
			const FComputeDrivenSection& S = Sections[i];
			TotalIndices += (int32)S.NumIndices;

			// IndirectArgs: [IndexCount, InstanceCount=0, StartIndex, 0, 0]
			TResourceArray<uint32, VERTEXBUFFER_ALIGNMENT> Args;
			Args.Add(S.NumIndices); Args.Add(0); Args.Add(S.FirstIndex); Args.Add(0); Args.Add(0);
			FRHIResourceCreateInfo CI(TEXT("ComputeDriven.IndirectArgs"), &Args);
			SharedBuffers->IndirectArgsBuffers[i] = RHICmdList.CreateVertexBuffer(
				5 * sizeof(uint32), BUF_DrawIndirect | BUF_UnorderedAccess,
				ERHIAccess::IndirectArgs, CI);
			SharedBuffers->IndirectArgsBufferUAVs[i] = RHICmdList.CreateUnorderedAccessView(
				SharedBuffers->IndirectArgsBuffers[i], PF_R32_UINT);

			// Reset buffer (BUF_CopySrc, never changes)
			TResourceArray<uint32, VERTEXBUFFER_ALIGNMENT> Reset;
			Reset.Add(S.NumIndices); Reset.Add(0u); Reset.Add(S.FirstIndex); Reset.Add(0u); Reset.Add(0u);
			FRHIResourceCreateInfo ResetCI(TEXT("ComputeDriven.IndirectArgsReset"), &Reset);
			SharedBuffers->IndirectArgsResetBuffers[i] = RHICmdList.CreateVertexBuffer(
				5 * sizeof(uint32), BUF_Static, ERHIAccess::CopySrc, ResetCI);
		}

		SharedBuffers->MeshNumIndices = TotalIndices;

		// Signal readiness AFTER all buffers are fully constructed.
		// The game thread polls bReady before dispatching the compute shader.
		FPlatformMisc::MemoryBarrier();
		SharedBuffers->bReady = true;
	}

	virtual void DestroyRenderThreadResources() override
	{
		if (VertexFactory)
		{
			VertexFactory->ReleaseResource();
			delete VertexFactory;
			VertexFactory = nullptr;
		}
		// SharedBuffers is a TSharedPtr — releasing our ref here is safe.
		// The component still holds its own ref and will keep the buffers alive
		// as long as RunComputeShader needs them.
		SharedBuffers.Reset();
	}

	virtual void OnTransformChanged(FRHICommandListBase& RHICmdList) override {}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance        = true;
		Result.bShadowRelevance      = true;
		Result.bDynamicRelevance     = true;
		Result.bStaticRelevance      = false;
		Result.bRenderInMainPass     = true;
		Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
		Result.bRenderCustomDepth    = ShouldRenderCustomDepth();
		CombinedMaterialRelevance.SetPrimitiveViewRelevance(Result);
		return Result;
	}

	virtual void GetDynamicMeshElements(
		const TArray<const FSceneView*>& Views,
		const FSceneViewFamily&          ViewFamily,
		uint32                           /*VisibilityMap*/,
		FMeshElementCollector&           Collector) const override
	{
		check(IsInRenderingThread() || IsInParallelRenderingThread());

		if (!SharedBuffers.IsValid() || !SharedBuffers->bReady ||
			!MeshIndexBuffer || Sections.Num() == 0 ||
			SharedBuffers->IndirectArgsBuffers.Num() != Sections.Num())
			return;

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			for (int32 Si = 0; Si < Sections.Num(); Si++)
			{
				const FComputeDrivenSection& S = Sections[Si];
				if (!SharedBuffers->IndirectArgsBuffers[Si].IsValid()) continue;

				FMeshBatch& Mesh = Collector.AllocateMesh();
				Mesh.bWireframe                     = AllowDebugViewmodes() && ViewFamily.EngineShowFlags.Wireframe;
				Mesh.bUseWireframeSelectionColoring  = IsSelected();
				Mesh.VertexFactory                  = VertexFactory;
				Mesh.MaterialRenderProxy            = S.MaterialProxy;
				Mesh.ReverseCulling                 = IsLocalToWorldDeterminantNegative();
				Mesh.Type                           = PT_TriangleList;
				Mesh.DepthPriorityGroup             = SDPG_World;
				Mesh.bCanApplyViewModeOverrides      = true;
				Mesh.bUseForMaterial                = true;
				Mesh.CastShadow                     = true;
				Mesh.bUseForDepthPass               = true;

				FMeshBatchElement& E = Mesh.Elements[0];
				E.IndexBuffer            = MeshIndexBuffer;
				E.IndirectArgsBuffer     = SharedBuffers->IndirectArgsBuffers[Si];
				E.IndirectArgsOffset     = 0;
				E.FirstIndex             = 0;
				E.NumPrimitives          = 0;
				E.MinVertexIndex         = S.MinVertexIndex;
				E.MaxVertexIndex         = S.MaxVertexIndex;
				E.PrimitiveIdMode        = PrimID_ForceZero;
				E.PrimitiveUniformBuffer = GetUniformBuffer();

				FComputeDrivenInstancingUserData* UserData =
					&Collector.AllocateOneFrameResource<FComputeDrivenInstancingUserData>();
				E.UserData = UserData;

				UserData->InstanceBufferSRV = SharedBuffers->InstanceBufferSRV.GetReference();
				UserData->PositionBufferSRV = VertexFactory->PositionBufferSRV.GetReference();
				UserData->TangentBufferSRV  = VertexFactory->TangentBufferSRV.GetReference();
				UserData->UV0BufferSRV      = VertexFactory->UV0BufferSRV.GetReference();
				UserData->NumTexCoords      = MeshNumTexCoords;
				UserData->LodViewOrigin     =
					(FVector3f)ViewFamily.Views[0]->ViewMatrices.GetViewOrigin();

				Collector.AddMesh(ViewIndex, Mesh);
			}
		}
	}

private:
	FComputeDrivenInstancingVertexFactory* VertexFactory;
	FMaterialRelevance                     CombinedMaterialRelevance;

	const FRawStaticIndexBuffer*    MeshIndexBuffer   = nullptr;
	const FStaticMeshVertexBuffers* MeshVertexBuffers = nullptr;
	uint32                          MeshNumTexCoords  = 1;

	TArray<FComputeDrivenSection> Sections;

	int32 NumInstancesSnapshot = 0;

	// Shared with the component — outlives both proxy and component destruction.
	TSharedPtr<FComputeDrivenGpuBuffers> SharedBuffers;
};

// ============================================================================
// UComputeDrivenIndirectInstancingComponent
// ============================================================================
UComputeDrivenIndirectInstancingComponent::UComputeDrivenIndirectInstancingComponent(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CastShadow                     = true;
	bCastContactShadow             = true;
	bUseAsOccluder                 = true;
	bAffectDynamicIndirectLighting = true;
	bAffectDistanceFieldLighting   = true;
	bNeverDistanceCull             = true;
	Mobility                       = EComponentMobility::Static;
}

void UComputeDrivenIndirectInstancingComponent::OnRegister()
{
	Super::OnRegister();
	SetMobility(EComponentMobility::Movable);

	// Allocate the shared block here so it exists before CreateSceneProxy.
	if (!GpuBuffers.IsValid())
		GpuBuffers = MakeShared<FComputeDrivenGpuBuffers>();
}

void UComputeDrivenIndirectInstancingComponent::OnUnregister()
{
	Super::OnUnregister();
}

void UComputeDrivenIndirectInstancingComponent::ApplyWorldOffset(
	const FVector& InOffset, bool bWorldShift)
{
	Super::ApplyWorldOffset(InOffset, bWorldShift);
	MarkRenderStateDirty();
}

FBoxSphereBounds UComputeDrivenIndirectInstancingComponent::CalcBounds(
	const FTransform& LocalToWorld) const
{
	const float WorldHalf = HALF_WORLD_MAX;
	return FBoxSphereBounds(FVector::ZeroVector, FVector(WorldHalf), WorldHalf);
}

FPrimitiveSceneProxy* UComputeDrivenIndirectInstancingComponent::CreateSceneProxy()
{
	if (!Mesh || !Mesh->GetRenderData() ||
		Mesh->GetRenderData()->LODResources.Num() == 0 ||
		Mesh->GetRenderData()->LODResources[0].Sections.Num() == 0)
		return nullptr;

	// Reset bReady so the new proxy's CreateRenderThreadResources sets it fresh.
	if (GpuBuffers.IsValid())
		GpuBuffers->bReady = false;

	return new FComputeDrivenIndirectInstancingSceneProxy(this);
}

UMaterialInterface* UComputeDrivenIndirectInstancingComponent::GetMaterial(int32 Index) const
{
	return Mesh ? Mesh->GetMaterial(Index) : nullptr;
}

void UComputeDrivenIndirectInstancingComponent::SetMaterial(int32, UMaterialInterface*) {}

void UComputeDrivenIndirectInstancingComponent::GetUsedMaterials(
	TArray<UMaterialInterface*>& OutMaterials, bool) const
{
	if (!Mesh || !Mesh->GetRenderData() ||
		Mesh->GetRenderData()->LODResources.Num() == 0) return;
	for (const FStaticMeshSection& S : Mesh->GetRenderData()->LODResources[0].Sections)
		if (UMaterialInterface* M = Mesh->GetMaterial(S.MaterialIndex))
			OutMaterials.AddUnique(M);
}

int32 UComputeDrivenIndirectInstancingComponent::GetNumMaterials() const
{
	return Mesh ? Mesh->GetStaticMaterials().Num() : 0;
}
