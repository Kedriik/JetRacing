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
#include "RenderGraphBuilder.h"
#include "CommonRenderResources.h"

// ============================================================================
// Scene proxy
//
// Self-contained — does NOT use FExampleIndirectInstancingRendererExtension or
// the CullInstancesCS path.  The compute shader writes MeshRenderInstance data
// directly into GpuInstanceBuffer, so we just bind it straight to the draw.
// ============================================================================
class FComputeDrivenIndirectInstancingSceneProxy final : public FPrimitiveSceneProxy
{
public:
	FComputeDrivenIndirectInstancingSceneProxy(UComputeDrivenIndirectInstancingComponent* InComponent)
		: FPrimitiveSceneProxy(InComponent, FName("ComputeDrivenIndirectInstancing"))
		, VertexFactory(nullptr)
		, Material(nullptr)
		, MeshIndexBuffer(nullptr)
		, MeshVertexBuffers(nullptr)
		, MeshNumIndices(0)
		, MeshNumTexCoords(1)
		// Snapshot the component's GPU buffer refs.
		// They are null until CreateRenderThreadResources fires — GetDynamicMeshElements
		// guards against this.
		, ComponentInstanceBufferSRV(InComponent->GpuInstanceBufferSRV)
		, ComponentIndirectArgsBuffer(InComponent->GpuIndirectArgsBuffer)
		// Keep a raw (non-owning) pointer so CreateRenderThreadResources can write
		// back the newly allocated buffer refs to the component.
		, OwnerComponent(InComponent)
	{
		UMaterialInterface* ComponentMaterial = InComponent->GetMaterial();
		const bool bValidMaterial = ComponentMaterial != nullptr &&
			ComponentMaterial->CheckMaterialUsage_Concurrent(MATUSAGE_VirtualHeightfieldMesh);
		Material = bValidMaterial
			? ComponentMaterial->GetRenderProxy()
			: UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy();
		MaterialRelevance = Material->GetMaterialInterface()->GetRelevance_Concurrent(
			GetScene().GetFeatureLevel());

		if (InComponent->Mesh &&
			InComponent->Mesh->GetRenderData() &&
			InComponent->Mesh->GetRenderData()->LODResources.Num() > 0)
		{
			FStaticMeshLODResources& LOD = InComponent->Mesh->GetRenderData()->LODResources[0];
			MeshVertexBuffers = &LOD.VertexBuffers;
			MeshIndexBuffer   = &LOD.IndexBuffer;
			MeshNumIndices    = LOD.IndexBuffer.GetNumIndices();
			MeshNumTexCoords  = LOD.VertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords();
		}
	}

	// -----------------------------------------------------------------------
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

		// Build vertex factory
		FComputeDrivenInstancingParameters UniformParams;
		VertexFactory = new FComputeDrivenInstancingVertexFactory(
			GetScene().GetFeatureLevel(), UniformParams);

		if (MeshVertexBuffers && MeshIndexBuffer)
		{
			VertexFactory->SetMeshBuffers(MeshVertexBuffers, MeshIndexBuffer);
		}
		VertexFactory->InitResource(RHICmdList);

		// Allocate GPU buffers on the component so the compute shader can write to them.
		// OwnerComponent pointer is safe here because CreateRenderThreadResources is called
		// while the game thread is blocked (proxy creation fence).
		if (OwnerComponent)
		{
			// MeshRenderInstance = 3 x float4 = 48 bytes per slot
			const uint32 InstanceStride     = 3 * sizeof(FVector4f);
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

			{
				FRHIResourceCreateInfo CI(TEXT("ComputeDriven.GpuIndirectArgsBuffer"));
				OwnerComponent->GpuIndirectArgsBuffer = RHICmdList.CreateVertexBuffer(
					5 * sizeof(uint32),
					BUF_DrawIndirect | BUF_UnorderedAccess,
					ERHIAccess::IndirectArgs, CI);
				OwnerComponent->GpuIndirectArgsBufferUAV = RHICmdList.CreateUnorderedAccessView(
					OwnerComponent->GpuIndirectArgsBuffer, PF_R32_UINT);
			}

			OwnerComponent->GpuMeshNumIndices = MeshNumIndices;

			// Update our local cached SRV/buffer pointers from the freshly allocated refs.
			ComponentInstanceBufferSRV  = OwnerComponent->GpuInstanceBufferSRV;
			ComponentIndirectArgsBuffer = OwnerComponent->GpuIndirectArgsBuffer;
		}
	}

	virtual void DestroyRenderThreadResources() override
	{
		if (VertexFactory)
		{
			VertexFactory->ReleaseResource();
			delete VertexFactory;
			VertexFactory = nullptr;
		}

		// GPU buffers belong to the component — do NOT release them here.
		// Just null out our cached pointers.
		ComponentInstanceBufferSRV.SafeRelease();
		ComponentIndirectArgsBuffer.SafeRelease();
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
		MaterialRelevance.SetPrimitiveViewRelevance(Result);
		return Result;
	}

	virtual void GetDynamicMeshElements(
		const TArray<const FSceneView*>& Views,
		const FSceneViewFamily&          ViewFamily,
		uint32                           VisibilityMap,
		FMeshElementCollector&           Collector) const override
	{
		check(IsInRenderingThread() || IsInParallelRenderingThread());

		// Skip until the compute shader has run at least once.
		if (!MeshIndexBuffer || MeshNumIndices == 0 ||
			!ComponentInstanceBufferSRV || !ComponentIndirectArgsBuffer)
		{
			return;
		}

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			if (!(VisibilityMap & (1 << ViewIndex)))
				continue;

			FMeshBatch& Mesh = Collector.AllocateMesh();
			Mesh.bWireframe                    = AllowDebugViewmodes() && ViewFamily.EngineShowFlags.Wireframe;
			Mesh.bUseWireframeSelectionColoring = IsSelected();
			Mesh.VertexFactory                 = VertexFactory;
			Mesh.MaterialRenderProxy           = Material;
			Mesh.ReverseCulling                = IsLocalToWorldDeterminantNegative();
			Mesh.Type                          = PT_TriangleList;
			Mesh.DepthPriorityGroup            = SDPG_World;
			Mesh.bCanApplyViewModeOverrides    = true;
			Mesh.bUseForMaterial               = true;
			Mesh.CastShadow                    = true;
			Mesh.bUseForDepthPass              = true;

			FMeshBatchElement& BatchElement     = Mesh.Elements[0];
			BatchElement.IndexBuffer            = MeshIndexBuffer;
			BatchElement.IndirectArgsBuffer     = ComponentIndirectArgsBuffer; // GPU-filled
			BatchElement.IndirectArgsOffset     = 0;
			BatchElement.FirstIndex             = 0;
			BatchElement.NumPrimitives          = 0; // ignored with IndirectArgsBuffer
			BatchElement.MinVertexIndex         = 0;
			BatchElement.MaxVertexIndex         = 0;
			BatchElement.PrimitiveIdMode        = PrimID_ForceZero;
			BatchElement.PrimitiveUniformBuffer = GetUniformBuffer();

			FComputeDrivenInstancingUserData* UserData =
				&Collector.AllocateOneFrameResource<FComputeDrivenInstancingUserData>();
			BatchElement.UserData = UserData;

			UserData->InstanceBufferSRV = ComponentInstanceBufferSRV.GetReference(); // GPU-filled
			UserData->PositionBufferSRV = VertexFactory->PositionBufferSRV.GetReference();
			UserData->TangentBufferSRV  = VertexFactory->TangentBufferSRV.GetReference();
			UserData->UV0BufferSRV      = VertexFactory->UV0BufferSRV.GetReference();
			UserData->NumTexCoords      = MeshNumTexCoords;
			UserData->LodViewOrigin     =
				(FVector3f)ViewFamily.Views[0]->ViewMatrices.GetViewOrigin();

			Collector.AddMesh(ViewIndex, Mesh);
		}
	}

private:
	FComputeDrivenInstancingVertexFactory* VertexFactory;
	FMaterialRenderProxy*                  Material;
	FMaterialRelevance                     MaterialRelevance;

	const FRawStaticIndexBuffer*    MeshIndexBuffer   = nullptr; // not owned
	const FStaticMeshVertexBuffers* MeshVertexBuffers = nullptr; // not owned
	int32                           MeshNumIndices    = 0;
	uint32                          MeshNumTexCoords  = 1;

	// Cached from component — valid after CreateRenderThreadResources.
	FShaderResourceViewRHIRef ComponentInstanceBufferSRV;
	FBufferRHIRef             ComponentIndirectArgsBuffer;

	// Raw back-pointer used only in CreateRenderThreadResources to write
	// newly allocated buffer refs back to the component.  Never used after that.
	UComputeDrivenIndirectInstancingComponent* OwnerComponent = nullptr;
};

// ============================================================================
// UComputeDrivenIndirectInstancingComponent
// ============================================================================
UComputeDrivenIndirectInstancingComponent::UComputeDrivenIndirectInstancingComponent(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CastShadow                    = true;
	bCastContactShadow            = true;
	bUseAsOccluder                = true;
	bAffectDynamicIndirectLighting = true;
	bAffectDistanceFieldLighting  = true;
	bNeverDistanceCull            = true;
	Mobility                      = EComponentMobility::Static;
}

void UComputeDrivenIndirectInstancingComponent::OnRegister()
{
	Super::OnRegister();
	SetMobility(EComponentMobility::Movable);

	if (Material != nullptr)
	{
		Material->CheckMaterialUsage(MATUSAGE_VirtualHeightfieldMesh);
	}
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
	// Conservative world-space AABB based on camera ortho footprint.
	// Adjust if your foliage placement area differs significantly.
	const float HalfSize = 50000.f;
	return FBoxSphereBounds(
		FBox(FVector(-HalfSize), FVector(HalfSize))).TransformBy(LocalToWorld);
}

FPrimitiveSceneProxy* UComputeDrivenIndirectInstancingComponent::CreateSceneProxy()
{
	if (!Mesh || !Mesh->GetRenderData() ||
		Mesh->GetRenderData()->LODResources.Num() == 0)
	{
		return nullptr;
	}

	return new FComputeDrivenIndirectInstancingSceneProxy(this);
}

void UComputeDrivenIndirectInstancingComponent::SetMaterial(
	int32 ElementIndex, UMaterialInterface* InMaterial)
{
	if (ElementIndex == 0 && Material != InMaterial)
	{
		Material = InMaterial;
		if (Material != nullptr)
		{
			Material->CheckMaterialUsage(MATUSAGE_VirtualHeightfieldMesh);
		}
		MarkRenderStateDirty();
	}
}

void UComputeDrivenIndirectInstancingComponent::GetUsedMaterials(
	TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const
{
	if (Material != nullptr)
	{
		OutMaterials.Add(Material);
	}
}
