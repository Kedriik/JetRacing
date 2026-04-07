#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "RHI.h"
#include "RHIResources.h"
#include "Components/PrimitiveComponent.h"
#include "ComputeShaderMeshSpawner.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class JETRACING_API UComputeShaderMeshSpawner : public UActorComponent
{
    GENERATED_BODY()

public:
    UComputeShaderMeshSpawner();

    // ── Designer properties (unchanged) ──────────────────────────────────────

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture")
    TObjectPtr<UTextureRenderTarget2D> DepthRenderTarget;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning")
    int32 NumInstances = 10000;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning")
    float GridCellSize = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float SpawnDensity = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning")
    float VerticalOffset = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture")
    FVector CameraLocation = FVector(0, 0, 2000000);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture")
    FRotator CameraRotation = FRotator(-90, 0, 0);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture")
    float OrthoWidth = 10000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture")
    FName VoxelMeshComponentTag = FName("VoxelMesh");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning")
    bool bUpdateEveryFrame = false;

    // ── Blueprint API (unchanged) ─────────────────────────────────────────────

    UFUNCTION(BlueprintCallable, Category = "Spawning")
    void RegisterVoxelMeshComponent(UPrimitiveComponent* Component);

    UFUNCTION(BlueprintCallable, Category = "Spawning")
    void UnregisterVoxelMeshComponent(UPrimitiveComponent* Component);

    UFUNCTION(BlueprintCallable, Category = "Spawning")
    void ExecuteComputeShader();

    UFUNCTION(BlueprintCallable, Category = "Spawning")
    void CaptureDepth();

    // ── Accessors for the Niagara Data Interface ──────────────────────────────
    // Safe to call from the game thread; the SRV is created once in CreateBuffers
    // and lives until ReleaseBuffers, so the DI can cache the pointer each frame.

    FORCEINLINE FShaderResourceViewRHIRef GetPositionBufferSRV() const { return PositionBufferSRV; }
    FORCEINLINE int32                     GetNumInstances()      const { return NumInstances; }

    // ── Component lifecycle ───────────────────────────────────────────────────

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    UPROPERTY()
    TObjectPtr<USceneCaptureComponent2D> SceneCaptureComponent;

    // GPU buffer – written by compute shader, read by Niagara via SRV
    FBufferRHIRef                  PositionBuffer;
    FUnorderedAccessViewRHIRef     PositionBufferUAV;  // compute shader writes via this
    FShaderResourceViewRHIRef      PositionBufferSRV;  // Niagara reads via this

    bool bCaptureInProgress = false;

    void CreateBuffers();
    void ReleaseBuffers();
    void RunComputeShader();
    void SetupDepthCapture();
    void UpdateVoxelComponentList();
};
