#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/StaticMesh.h"
#include "RHI.h"
#include "RHIResources.h"
#include "ComputeDrivenIndirectInstancingComponent.generated.h"

class UStaticMesh;

// ============================================================================
// Shared GPU buffer block
//
// Allocated on the heap and held by both the component (game thread) and the
// scene proxy (render thread) via TSharedPtr.  RHI ref-counted types are
// thread-safe by design.  bReady is written once by the proxy on the render
// thread and polled by the spawner on the game thread — acceptable because
// it is a single bool written once after all buffer writes complete.
// ============================================================================
struct FComputeDrivenGpuBuffers
{
	FBufferRHIRef              InstanceBuffer;
	FUnorderedAccessViewRHIRef InstanceBufferUAV;
	FShaderResourceViewRHIRef  InstanceBufferSRV;

	TArray<FBufferRHIRef>              IndirectArgsBuffers;
	TArray<FUnorderedAccessViewRHIRef> IndirectArgsBufferUAVs;
	TArray<FBufferRHIRef>              IndirectArgsResetBuffers;

	int32 MeshNumIndices = 0;

	// Set to true by the proxy after all buffers are created.
	// Polled on game thread; safe because it transitions false→true exactly once.
	volatile bool bReady = false;
};

UCLASS(Blueprintable, ClassGroup = Rendering,
	hideCategories = (Activation, Collision, Cooking, HLOD, Navigation, Object, Physics, VirtualTexture))
class JETRACING_API UComputeDrivenIndirectInstancingComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	UComputeDrivenIndirectInstancingComponent(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Rendering)
	UStaticMesh* Mesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compute Spawning")
	int32 MaxInstances = 10000;

	// Shared GPU buffer block — created by proxy, read by spawner.
	// TSharedPtr keeps the block alive regardless of which side is destroyed first.
	TSharedPtr<FComputeDrivenGpuBuffers> GpuBuffers;

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
