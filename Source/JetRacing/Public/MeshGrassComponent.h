// MeshGrassComponent.h
// Custom grass spawning component for any static mesh in UE5
// Replicates UE4 landscape grass functionality

#pragma once

#include "CoreMinimal.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "MeshGrassComponent.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;

/**
 * Grass type definition - defines one type of grass to spawn
 */
USTRUCT(BlueprintType)
struct FMeshGrassVariety
{
    GENERATED_BODY()

    // The static mesh to use for this grass type
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass")
    UStaticMesh* GrassMesh = nullptr;

    // Density of grass instances (instances per 100x100 units)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass", meta = (ClampMin = "0.0"))
    float GrassDensity = 100.0f;

    // Min/Max scale for grass instances
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass")
    FFloatInterval ScaleRange = FFloatInterval(0.8f, 1.2f);

    // Random rotation range (in degrees)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass")
    bool bRandomRotation = true;

    // Align to surface normal
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass")
    bool bAlignToSurface = true;

    // Random pitch offset range (degrees)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass")
    FFloatInterval RandomPitchOffset = FFloatInterval(-5.0f, 5.0f);

    // Offset from surface
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass")
    float SurfaceOffset = 0.0f;

    // Cull distance
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass")
    FInt32Interval CullDistance = FInt32Interval(0, 10000);

    // Enable collision on grass instances
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass")
    bool bEnableCollision = false;

    // Cast shadows
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass")
    bool bCastShadow = false;
};

/**
 * Component that spawns grass on any static mesh surface
 * Attach this to an actor with a static mesh component
 */
UCLASS(ClassGroup = (Rendering), meta = (BlueprintSpawnableComponent), hidecategories = (Object, LOD, Physics, Collision))
class UMeshGrassComponent : public USceneComponent
{
    GENERATED_BODY()

public:
    UMeshGrassComponent();

    // Grass varieties to spawn
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass")
    TArray<FMeshGrassVariety> GrassVarieties;

    // Target static mesh component to spawn grass on
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass")
    UStaticMeshComponent* TargetMeshComponent = nullptr;

    // Grid size for sampling (smaller = more accurate but slower)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass", meta = (ClampMin = "10.0", ClampMax = "500.0"))
    float SamplingGridSize = 50.0f;

    // Height offset for ray traces
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass")
    float TraceHeight = 1000.0f;

    // Auto-generate grass on BeginPlay
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass")
    bool bAutoGenerate = true;

    // Random seed for reproducible results
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass")
    int32 RandomSeed = 12345;

    // Regenerate grass (call this to update)
    UFUNCTION(BlueprintCallable, Category = "Grass")
    void GenerateGrass();

    // Clear all grass instances
    UFUNCTION(BlueprintCallable, Category = "Grass")
    void ClearGrass();

    // Get total instance count
    UFUNCTION(BlueprintCallable, Category = "Grass")
    int32 GetTotalInstanceCount() const;

protected:
    virtual void BeginPlay() override;
    virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
    // HISM components for each grass variety
    UPROPERTY(Transient)
    TArray<UHierarchicalInstancedStaticMeshComponent*> GrassComponents;

    // Generate grass for a specific variety
    void GenerateGrassVariety(int32 VarietyIndex);

    // Sample points on mesh surface
    TArray<FTransform> SampleMeshSurface(const FMeshGrassVariety& Variety);

    // Get or create HISM component for a variety
    UHierarchicalInstancedStaticMeshComponent* GetOrCreateHISMComponent(int32 VarietyIndex);

    // Auto-find target mesh component
    void AutoFindTargetMesh();
};