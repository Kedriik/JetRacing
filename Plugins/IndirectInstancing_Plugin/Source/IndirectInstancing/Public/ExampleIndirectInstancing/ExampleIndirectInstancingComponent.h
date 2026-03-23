// Copyright Epic Games, Inc. All Rights Reserved.
// Adapted from the VirtualHeightfieldMesh plugin

#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/StaticMesh.h"
#include "ExampleIndirectInstancingComponent.generated.h"

class UMaterialInterface;
class UStaticMesh;

UCLASS(Blueprintable, ClassGroup = Rendering, hideCategories = (Activation, Collision, Cooking, HLOD, Navigation, Object, Physics, VirtualTexture))
class INDIRECTINSTANCING_API UExampleIndirectInstancingComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()

protected:
	UPROPERTY(EditAnywhere, Category = Rendering)
	UMaterialInterface* Material = nullptr;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Rendering)
	UStaticMesh* Mesh = nullptr;

	/**
	 * Per-instance transforms in component LOCAL SPACE.
	 * The vertex shader applies the component's LocalToWorld on top, so (0,0,0)
	 * means "at the component's own location in the world".
	 * Leave empty with bAutoSpawnInstances=true to auto-generate random instances.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Instancing)
	TArray<FTransform> InstanceTransforms;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instancing|AutoSpawn")
	bool bAutoSpawnInstances = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instancing|AutoSpawn", meta = (ClampMin = "1", ClampMax = "100000"))
	int32 RandomInstanceCount = 1000;

	/** Half-extent (cm) in local XY around the component origin. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instancing|AutoSpawn", meta = (ClampMin = "1.0"))
	float SpawnRadius = 5000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instancing|AutoSpawn", meta = (ClampMin = "0.01"))
	float ScaleMin = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instancing|AutoSpawn", meta = (ClampMin = "0.01"))
	float ScaleMax = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instancing|AutoSpawn")
	int32 RandomSeed = 42;

	UMaterialInterface* GetMaterial() const { return Material; }

	/** Refill InstanceTransforms with new random instances and recreate the render state. */
	UFUNCTION(BlueprintCallable, Category = "Instancing|AutoSpawn")
	void RegenerateRandomInstances();

protected:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void ApplyWorldOffset(const FVector& InOffset, bool bWorldShift) override;
	virtual bool IsVisible() const override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual bool SupportsStaticLighting() const override { return true; }
	virtual void SetMaterial(int32 ElementIndex, class UMaterialInterface* Material) override;
	virtual UMaterialInterface* GetMaterial(int32 Index) const override { return Material; }
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false) const override;

private:
	void PopulateRandomInstances();
};
