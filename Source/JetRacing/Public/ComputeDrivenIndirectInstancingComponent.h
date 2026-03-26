#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/StaticMesh.h"
#include "RHI.h"
#include "RHIResources.h"
#include "ComputeDrivenIndirectInstancingComponent.generated.h"

class UMaterialInterface;
class UStaticMesh;

/**
 * UComputeDrivenIndirectInstancingComponent
 *
 * A PrimitiveComponent that renders a static mesh with GPU-indirect instancing.
 * Instance transforms are written directly into GpuInstanceBuffer by
 * UComputeShaderMeshSpawner — no CPU readback, no ISMC, no plugin dependency.
 *
 * Setup:
 *   1. Add this component to your actor and assign Mesh + Material.
 *   2. Add UComputeShaderMeshSpawner to the same actor and point its
 *      IndirectInstancingComponent property here.
 *   3. Set MaxInstances to match UComputeShaderMeshSpawner::NumInstances.
 */
UCLASS(Blueprintable, ClassGroup = Rendering,
	hideCategories = (Activation, Collision, Cooking, HLOD, Navigation, Object, Physics, VirtualTexture))
class JETRACING_API UComputeDrivenIndirectInstancingComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	UComputeDrivenIndirectInstancingComponent(const FObjectInitializer& ObjectInitializer);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Rendering)
	UStaticMesh* Mesh = nullptr;

	UPROPERTY(EditAnywhere, Category = Rendering)
	UMaterialInterface* Material = nullptr;

	/** Must match UComputeShaderMeshSpawner::NumInstances. Sizes the GPU buffer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compute Spawning")
	int32 MaxInstances = 10000;

	UMaterialInterface* GetMaterial() const { return Material; }

	// -------------------------------------------------------------------------
	// GPU buffers — allocated in CreateRenderThreadResources, written each frame
	// by UComputeShaderMeshSpawner::RunComputeShader, read by the scene proxy.
	// Not UPROPERTYs — RHI resources are not UObject-managed.
	// -------------------------------------------------------------------------

	/** MeshRenderInstance structured buffer (3 x float4 per slot). */
	FBufferRHIRef              GpuInstanceBuffer;
	FUnorderedAccessViewRHIRef GpuInstanceBufferUAV;
	FShaderResourceViewRHIRef  GpuInstanceBufferSRV;

	/** DrawIndexedIndirect args: [IndexCount, InstanceCount, 0, 0, 0]. */
	FBufferRHIRef              GpuIndirectArgsBuffer;
	FUnorderedAccessViewRHIRef GpuIndirectArgsBufferUAV;

	/** Index count of mesh LOD0 — forwarded to the compute shader. */
	int32 GpuMeshNumIndices = 0;

protected:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void ApplyWorldOffset(const FVector& InOffset, bool bWorldShift) override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual bool SupportsStaticLighting() const override { return false; }
	virtual void SetMaterial(int32 ElementIndex, UMaterialInterface* InMaterial) override;
	virtual UMaterialInterface* GetMaterial(int32 Index) const override { return Material; }
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false) const override;
};
