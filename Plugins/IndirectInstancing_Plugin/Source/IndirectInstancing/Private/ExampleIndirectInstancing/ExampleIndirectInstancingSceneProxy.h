// Copyright Epic Games, Inc. All Rights Reserved.
// Adapted from the VirtualHeightfieldMesh plugin

#pragma once

#include "CoreMinimal.h"
#include "PrimitiveSceneProxy.h"
#include "Materials/MaterialRenderProxy.h"
#include "StaticMeshResources.h"

namespace ExampleIndirectInstancingMesh
{
	/** Buffers filled by GPU culling. */
	struct FDrawInstanceBuffers
	{
		/* Culled instance buffer. */
		FBufferRHIRef InstanceBuffer;
		FUnorderedAccessViewRHIRef InstanceBufferUAV;
		FShaderResourceViewRHIRef InstanceBufferSRV;

		/* IndirectArgs buffer for final DrawInstancedIndirect. */
		FBufferRHIRef IndirectArgsBuffer;
		FUnorderedAccessViewRHIRef IndirectArgsBufferUAV;
	};
}

class FExampleIndirectInstancingSceneProxy final : public FPrimitiveSceneProxy
{
public:
	FExampleIndirectInstancingSceneProxy(class UExampleIndirectInstancingComponent * InComponent);

protected:
	//~ Begin FPrimitiveSceneProxy Interface
	virtual SIZE_T GetTypeHash() const override;
	virtual uint32 GetMemoryFootprint() const override;
	virtual void CreateRenderThreadResources(FRHICommandListBase& RHICmdList) override;
	virtual void DestroyRenderThreadResources() override;
	virtual void OnTransformChanged(FRHICommandListBase& RHICmdList) override;
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView *View) const override;
	virtual void GetDynamicMeshElements(const TArray<const FSceneView *> &Views, const FSceneViewFamily &ViewFamily, uint32 VisibilityMap, FMeshElementCollector &Collector) const override;
	//~ End FPrimitiveSceneProxy Interface

private:
	void BuildOcclusionVolumes(TArrayView<FVector2D> const &InMinMaxData, FIntPoint const &InMinMaxSize, TArrayView<int32> const &InMinMaxMips, int32 InNumLods);

public:
	bool bHiddenInEditor;

	class FMaterialRenderProxy *Material;
	FMaterialRelevance MaterialRelevance;

	bool bCallbackRegistered;

	class FExampleIndirectInstancingVertexFactory *VertexFactory;

	// -----------------------------------------------------------------------
	// Static mesh data — captured from LOD 0 at proxy construction time.
	// -----------------------------------------------------------------------
	const FStaticMeshVertexBuffers* MeshVertexBuffers = nullptr; // not owned
	const FRawStaticIndexBuffer*    MeshIndexBuffer   = nullptr; // not owned
	int32                           MeshNumIndices    = 0;
	uint32                          MeshNumTexCoords  = 1;

	// -----------------------------------------------------------------------
	// Per-instance transform data.
	// CPU side: built from UExampleIndirectInstancingComponent::InstanceTransforms.
	// GPU side: uploaded once to SourceInstanceBuffer during CreateRenderThreadResources.
	// -----------------------------------------------------------------------
	struct FInstanceTransform
	{
		FVector4f Row0; // (m00,m01,m02, tx)
		FVector4f Row1; // (m10,m11,m12, ty)
		FVector4f Row2; // (m20,m21,m22, tz)
	};
	TArray<FInstanceTransform> CpuInstanceData;

	FBufferRHIRef             SourceInstanceBuffer;
	FShaderResourceViewRHIRef SourceInstanceBufferSRV;

	// Pre-filled indirect draw args: [IndexCount, InstanceCount, 0, 0, 0]
	// Built once in CreateRenderThreadResources — no compute passes needed.
	FBufferRHIRef             IndirectArgsBuffer;
};