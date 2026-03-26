#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "RHI.h"
#include "RHIResources.h"
#include "Components/PrimitiveComponent.h"
#include "ComputeShaderMeshSpawner.generated.h"

class UComputeDrivenIndirectInstancingComponent;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class JETRACING_API UComputeShaderMeshSpawner : public UActorComponent
{
    GENERATED_BODY()

public:
    UComputeShaderMeshSpawner();

    // -----------------------------------------------------------------------
    // Assign the mesh — its material slots are used automatically.
    // -----------------------------------------------------------------------
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning")
    TObjectPtr<UStaticMesh> FoliageMesh;

    // -----------------------------------------------------------------------
    // Depth capture
    // -----------------------------------------------------------------------
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture")
    TObjectPtr<UTextureRenderTarget2D> DepthRenderTarget;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture")
    FVector CameraLocation = FVector(0, 0, 2000000);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture")
    FRotator CameraRotation = FRotator(-90, 0, 0);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture")
    float OrthoWidth = 10000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture")
    FName VoxelMeshComponentTag = FName("VoxelMesh");

    // -----------------------------------------------------------------------
    // Spawn parameters
    // -----------------------------------------------------------------------
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning")
    int32 NumInstances = 10000;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning")
    float GridCellSize = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float SpawnDensity = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning")
    float VerticalOffset = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning", meta = (ClampMin = "0.01"))
    float ScaleMin = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning", meta = (ClampMin = "0.01"))
    float ScaleMax = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning")
    bool bUpdateEveryFrame = false;

    // -----------------------------------------------------------------------
    // Blueprint API
    // -----------------------------------------------------------------------
    UFUNCTION(BlueprintCallable, Category = "Spawning")
    void ExecuteComputeShader();

    UFUNCTION(BlueprintCallable, Category = "Spawning")
    void CaptureDepth();

    UFUNCTION(BlueprintCallable, Category = "Spawning")
    void UpdateVoxelComponentList();

    UFUNCTION(BlueprintCallable, Category = "Spawning")
    void RegisterVoxelMeshComponent(UPrimitiveComponent* Component);

    UFUNCTION(BlueprintCallable, Category = "Spawning")
    void UnregisterVoxelMeshComponent(UPrimitiveComponent* Component);

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    UPROPERTY()
    TObjectPtr<UComputeDrivenIndirectInstancingComponent> IndirectInstancingComponent;

    UPROPERTY()
    TObjectPtr<USceneCaptureComponent2D> SceneCaptureComponent;

    bool bCaptureInProgress = false;

    void SetupDepthCapture();
    void RunComputeShader();
};
