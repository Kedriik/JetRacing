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
// Per-section data captured at proxy construction time
// ============================================================================
struct FComputeDrivenSection
{
	FMaterialRenderProxy* MaterialProxy   = nullptr;
	FMaterialRelevance    MaterialRelevance;
	uint32                FirstIndex      = 0;   // byte offset into the index buffer
	uint32                NumIndices      = 0;   // indices in this section
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
		, ComponentInstanceBufferSRV(InComponent->GpuInstanceBufferSRV)
		, OwnerComponent(InComponent)
	{
		UStaticMesh* StaticMesh = InComponent->Mesh;
		if (!StaticMesh || !StaticMesh->GetRenderData() ||
			StaticMesh->GetRenderData()->LODResources.Num() == 0)
		{
			return;
		}

		FStaticMeshLODResources& LOD = StaticMesh->GetRenderData()->LODResources[0];
		MeshVertexBuffers = &LOD.VertexBuffers;
		MeshIndexBuffer   = &LOD.IndexBuffer;
		MeshNumTexCoords  = LOD.VertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords();

		// Build one section entry per LOD section, reading the material from
		// the mesh's material slots — exactly what the editor assigns.
		for (int32 SectionIdx = 0; SectionIdx < LOD.Sections.Num(); SectionIdx++)
		{
			const FStaticMeshSection& MeshSection = LOD.Sections[SectionIdx];

			UMaterialInterface* Mat = StaticMesh->GetMaterial(MeshSection.MaterialIndex);
			if (!Mat)
				Mat = UMaterial::GetDefaultMaterial(MD_Surface);

			FComputeDrivenSection& S = Sections.AddDefaulted_GetRef();
			S.MaterialProxy     = Mat->GetRenderProxy();
			S.MaterialRelevance = Mat->GetRelevance_Concurrent(GetScene().GetFeatureLevel());
			S.FirstIndex       = MeshSection.FirstIndex;
			S.NumIndices       = MeshSection.NumTriangles * 3;
			S.MinVertexIndex   = MeshSection.MinVertexIndex;
			S.MaxVertexIndex   = MeshSection.MaxVertexIndex;

			// Accumulate material relevance across all sections
			CombinedMaterialRelevance |= S.MaterialRelevance;
		}

		// Snapshot the per-section indirect arg buffers from the component.
		// These are null until CreateRenderThreadResources — GetDynamicMeshElements guards.
		ComponentIndirectArgsBuffers = InComponent->GpuIndirectArgsBuffers;
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

		FComputeDrivenInstancingParameters UniformParams;
		VertexFactory = new FComputeDrivenInstancingVertexFactory(
			GetScene().GetFeatureLevel(), UniformParams);

		if (MeshVertexBuffers && MeshIndexBuffer)
			VertexFactory->SetMeshBuffers(MeshVertexBuffers, MeshIndexBuffer);

		VertexFactory->InitResource(RHICmdList);

		if (!OwnerComponent)
			return;

		// ---- Instance buffer (shared across all sections) ----
		const uint32 InstanceStride     = 3 * sizeof(FVector4f); // MeshRenderInstance
		const uint32 InstanceBufferSize = (uint32)OwnerComponent->MaxInstances * InstanceStride;

		{
			FRHIResourceCreateInfo CI(TEXT("ComputeDriven.GpuInstanceBuffer"));
			OwnerComponent->GpuInstanceBuffer = RHICmdList.CreateStructuredBuffer(
				InstanceStride, InstanceBufferSize,
				BUF_UnorderedAccess | BUF_ShaderResource,
				ERHIAccess::SRVMask, CI);
			OwnerComponent->GpuInstanceBufferUAV = RHICmdList.CreateUnorderedAccessView(
				OwnerComponent->GpuInstanceBuffer, false, false);
			OwnerComponent->GpuInstanceBufferSRV = RHICmdList.CreateShaderResourceView(
				OwnerComponent->GpuInstanceBuffer);
		}

		// ---- One IndirectArgs buffer per section ----
		// All sections draw the same instances, so InstanceCount is the same
		// for every buffer. The compute shader writes to buffer[0] only.
		// We copy InstanceCount from buffer[0] to the others before drawing.
		OwnerComponent->GpuIndirectArgsBuffers.SetNum(Sections.Num());
		OwnerComponent->GpuIndirectArgsBufferUAVs.SetNum(Sections.Num());

		for (int32 SectionIdx = 0; SectionIdx < Sections.Num(); SectionIdx++)
		{
			const FComputeDrivenSection& S = Sections[SectionIdx];

			// Pre-fill: [NumIndices, 0, FirstIndex, 0, 0]
			TResourceArray<uint32, VERTEXBUFFER_ALIGNMENT> Args;
			Args.Add(S.NumIndices);  // IndexCountPerInstance
			Args.Add(0);             // InstanceCount — filled by compute shader
			Args.Add(S.FirstIndex);  // StartIndexLocation
			Args.Add(0);             // BaseVertexLocation
			Args.Add(0);             // StartInstanceLocation

			FRHIResourceCreateInfo CI(TEXT("ComputeDriven.GpuIndirectArgsBuffer"), &Args);
			OwnerComponent->GpuIndirectArgsBuffers[SectionIdx] =
				RHICmdList.CreateVertexBuffer(
					5 * sizeof(uint32),
					BUF_DrawIndirect | BUF_UnorderedAccess,
					ERHIAccess::IndirectArgs, CI);
			OwnerComponent->GpuIndirectArgsBufferUAVs[SectionIdx] =
				RHICmdList.CreateUnorderedAccessView(
					OwnerComponent->GpuIndirectArgsBuffers[SectionIdx], PF_R32_UINT);
		}

		// Total index count (used by RunComputeShader to check readiness)
		int32 TotalIndices = 0;
		for (const FComputeDrivenSection& S : Sections)
			TotalIndices += (int32)S.NumIndices;
		OwnerComponent->GpuMeshNumIndices = TotalIndices;

		// Update local cached refs
		ComponentInstanceBufferSRV   = OwnerComponent->GpuInstanceBufferSRV;
		ComponentIndirectArgsBuffers = OwnerComponent->GpuIndirectArgsBuffers;
	}

	virtual void DestroyRenderThreadResources() override
	{
		if (VertexFactory)
		{
			VertexFactory->ReleaseResource();
			delete VertexFactory;
			VertexFactory = nullptr;
		}

		ComponentInstanceBufferSRV.SafeRelease();
		ComponentIndirectArgsBuffers.Empty();
	}

	virtual void OnTransformChanged(FRHICommandListBase& RHICmdList) override {}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance        = IsShown(View);
		Result.bShadowRelevance      = IsShadowCast(View) && ShouldRenderInMainPass();
		Result.bDynamicRelevance     = true;
		Result.bStaticRelevance      = false;
		Result.bRenderInMainPass     = ShouldRenderInMainPass();
		Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
		Result.bRenderCustomDepth    = ShouldRenderCustomDepth();
		CombinedMaterialRelevance.SetPrimitiveViewRelevance(Result);
		return Result;
	}

	virtual void GetDynamicMeshElements(
		const TArray<const FSceneView*>& Views,
		const FSceneViewFamily&          ViewFamily,
		uint32                           VisibilityMap,
		FMeshElementCollector&           Collector) const override
	{
		check(IsInRenderingThread() || IsInParallelRenderingThread());

		if (!MeshIndexBuffer || Sections.Num() == 0 || !ComponentInstanceBufferSRV ||
			ComponentIndirectArgsBuffers.Num() != Sections.Num())
		{
			return;
		}

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			if (!(VisibilityMap & (1 << ViewIndex)))
				continue;

			// One FMeshBatch per section — each gets its own material and
			// its own IndirectArgs buffer (with the correct FirstIndex baked in).
			for (int32 SectionIdx = 0; SectionIdx < Sections.Num(); SectionIdx++)
			{
				const FComputeDrivenSection& S = Sections[SectionIdx];

				if (!ComponentIndirectArgsBuffers[SectionIdx].IsValid())
					continue;

				FMeshBatch& Mesh = Collector.AllocateMesh();
				Mesh.bWireframe                    = AllowDebugViewmodes() && ViewFamily.EngineShowFlags.Wireframe;
				Mesh.bUseWireframeSelectionColoring = IsSelected();
				Mesh.VertexFactory                 = VertexFactory;
				Mesh.MaterialRenderProxy           = S.MaterialProxy;
				Mesh.ReverseCulling                = IsLocalToWorldDeterminantNegative();
				Mesh.Type                          = PT_TriangleList;
				Mesh.DepthPriorityGroup            = SDPG_World;
				Mesh.bCanApplyViewModeOverrides    = true;
				Mesh.bUseForMaterial               = true;
				Mesh.CastShadow                    = true;
				Mesh.bUseForDepthPass              = true;

				FMeshBatchElement& BatchElement     = Mesh.Elements[0];
				BatchElement.IndexBuffer            = MeshIndexBuffer;
				BatchElement.IndirectArgsBuffer     = ComponentIndirectArgsBuffers[SectionIdx];
				BatchElement.IndirectArgsOffset     = 0;
				BatchElement.FirstIndex             = 0; // StartIndex is baked into IndirectArgs
				BatchElement.NumPrimitives          = 0; // ignored with IndirectArgsBuffer
				BatchElement.MinVertexIndex         = S.MinVertexIndex;
				BatchElement.MaxVertexIndex         = S.MaxVertexIndex;
				BatchElement.PrimitiveIdMode        = PrimID_ForceZero;
				BatchElement.PrimitiveUniformBuffer = GetUniformBuffer();

				FComputeDrivenInstancingUserData* UserData =
					&Collector.AllocateOneFrameResource<FComputeDrivenInstancingUserData>();
				BatchElement.UserData = UserData;

				UserData->InstanceBufferSRV = ComponentInstanceBufferSRV.GetReference();
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

	FShaderResourceViewRHIRef  ComponentInstanceBufferSRV;
	TArray<FBufferRHIRef>      ComponentIndirectArgsBuffers;

	UComputeDrivenIndirectInstancingComponent* OwnerComponent = nullptr;
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
	const float HalfSize = 50000.f;
	return FBoxSphereBounds(FBox(FVector(-HalfSize), FVector(HalfSize))).TransformBy(LocalToWorld);
}

FPrimitiveSceneProxy* UComputeDrivenIndirectInstancingComponent::CreateSceneProxy()
{
	if (!Mesh || !Mesh->GetRenderData() ||
		Mesh->GetRenderData()->LODResources.Num() == 0 ||
		Mesh->GetRenderData()->LODResources[0].Sections.Num() == 0)
	{
		return nullptr;
	}

	return new FComputeDrivenIndirectInstancingSceneProxy(this);
}

UMaterialInterface* UComputeDrivenIndirectInstancingComponent::GetMaterial(int32 Index) const
{
	if (Mesh)
		return Mesh->GetMaterial(Index);
	return nullptr;
}

void UComputeDrivenIndirectInstancingComponent::SetMaterial(
	int32 ElementIndex, UMaterialInterface* InMaterial)
{
	// Materials live on the mesh asset — override not supported.
}

void UComputeDrivenIndirectInstancingComponent::GetUsedMaterials(
	TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const
{
	if (!Mesh || !Mesh->GetRenderData() ||
		Mesh->GetRenderData()->LODResources.Num() == 0)
		return;

	const FStaticMeshLODResources& LOD = Mesh->GetRenderData()->LODResources[0];
	for (const FStaticMeshSection& Section : LOD.Sections)
	{
		UMaterialInterface* Mat = Mesh->GetMaterial(Section.MaterialIndex);
		if (Mat)
			OutMaterials.AddUnique(Mat);
	}
}

int32 UComputeDrivenIndirectInstancingComponent::GetNumMaterials() const
{
	return Mesh ? Mesh->GetStaticMaterials().Num() : 0;
}
