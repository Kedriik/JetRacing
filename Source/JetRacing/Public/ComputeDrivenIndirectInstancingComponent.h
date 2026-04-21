#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/StaticMesh.h"
#include "RHI.h"
#include "RHIResources.h"
#include "ComputeDrivenIndirectInstancingComponent.generated.h"

class UStaticMesh;

UCLASS(Blueprintable, ClassGroup = Rendering,
	hideCategories = (Activation, Collision, Cooking, HLOD, Navigation, Object, Physics, VirtualTexture))
class JETRACING_API UComputeDrivenIndirectInstancingComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	UComputeDrivenIndirectInstancingComponent(const FObjectInitializer& ObjectInitializer);

	/** The mesh to render. Materials are read directly from the mesh's material slots. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Rendering)
	UStaticMesh* Mesh = nullptr;

	/** Must match UComputeShaderMeshSpawner::NumInstances. Sizes the GPU instance buffer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compute Spawning")
	int32 MaxInstances = 10000;

	/**
	 * Must match UComputeShaderMeshSpawner::GridCellSize.
	 * Used by CalcBounds to compute a bounds sphere that covers all possible
	 * instance positions without depending on this component's local transform.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compute Spawning")
	float GridCellSize = 50.0f;

	/**
	 * World-space position of the depth-capture camera at the time of the last
	 * compute dispatch.  Updated by UComputeShaderMeshSpawner::RunComputeShader
	 * so that CalcBounds stays centred on where instances actually live.
	 * Changing this automatically calls UpdateBounds() to refresh culling.
	 */
	FVector CaptureOrigin = FVector::ZeroVector;

	// -------------------------------------------------------------------------
	// GPU buffers — allocated in CreateRenderThreadResources, written each frame
	// by UComputeShaderMeshSpawner::RunComputeShader, read by the scene proxy.
	// -------------------------------------------------------------------------

	/** MeshRenderInstance structured buffer (3 x float4 per slot). */
	FBufferRHIRef              GpuInstanceBuffer;
	FUnorderedAccessViewRHIRef GpuInstanceBufferUAV;
	FShaderResourceViewRHIRef  GpuInstanceBufferSRV;

	/**
	 * One IndirectArgs buffer per LOD0 section:
	 * [IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation]
	 * All sections share the same InstanceCount (same set of instances drawn for each).
	 */
	TArray<FBufferRHIRef>              GpuIndirectArgsBuffers;
	TArray<FUnorderedAccessViewRHIRef> GpuIndirectArgsBufferUAVs;

	/**
	 * Total index count across all sections — used by the compute shader to size
	 * the instance buffer. For the atomic counter we only need one IndirectArgs
	 * buffer (all sections share the same InstanceCount), so the compute shader
	 * writes to GpuIndirectArgsBuffers[0].
	 */
	int32 GpuMeshNumIndices = 0;

	/**
	 * Immutable per-section args baked at allocation time:
	 *   [0] = IndexCountPerInstance
	 *   [2] = StartIndexLocation
	 * Used by RunComputeShader to reset InstanceCount to 0 each frame without
	 * locking the GPU buffer for CPU readback (which is not allowed on modern RHIs
	 * when the buffer is in UAVCompute state).
	 */
	struct FStaticIndirectArgs { uint32 IndexCount; uint32 StartIndex; };
	TArray<FStaticIndirectArgs> GpuIndirectArgsStatic;

	/**
	 * One read-only (BUF_CopySrc) 5×uint32 buffer per section, pre-filled at
	 * creation with [IndexCount, 0, StartIndex, 0, 0].  Each frame we GPU-copy
	 * from here into GpuIndirectArgsBuffers[i] to reset InstanceCount to 0
	 * with a pure CopyBufferRegion — no CPU lock, no staging upload.
	 */
	TArray<FBufferRHIRef> GpuIndirectArgsResetBuffers;

protected:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void ApplyWorldOffset(const FVector& InOffset, bool bWorldShift) override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual bool SupportsStaticLighting() const override { return false; }
	virtual UMaterialInterface* GetMaterial(int32 Index) const override;
	virtual void SetMaterial(int32 ElementIndex, UMaterialInterface* InMaterial) override;
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false) const override;
	virtual int32 GetNumMaterials() const override;
};
