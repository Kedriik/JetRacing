#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "RHI.h"
#include "RHIResources.h"
#include "ComputeShaderMeshSpawner.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class JETRACING_API UComputeShaderMeshSpawner : public UActorComponent
{
    GENERATED_BODY()

public:
    UComputeShaderMeshSpawner();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning")
    TObjectPtr<UStaticMesh> FoliageMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture")
    TObjectPtr<UTextureRenderTarget2D> DepthRenderTarget;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture")
    TObjectPtr<UTextureRenderTarget2D> StencilRenderTarget;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning")
    int32 NumInstances = 10000;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning")
    int32 TargetStencilValue= 42;

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
    UMaterialInterface* StencilVisualizationMaterial;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Raymarching")
    float MaxRayDistance = 2000000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Raymarching")
    float RaymarchStepSize = 1000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Raymarching")
    int32 MaxRaymarchSteps = 2000;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning")
    bool bUpdateEveryFrame = false;

    UFUNCTION(BlueprintCallable, Category = "Spawning")
    void ExecuteComputeShader();

    UFUNCTION(BlueprintCallable, Category = "Spawning")
    void CaptureDepth();

    UFUNCTION(BlueprintCallable, Category = "Spawning")
    void CaptureStencil();

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    UPROPERTY()
    TObjectPtr<UInstancedStaticMeshComponent> InstancedMeshComponent;

    UPROPERTY()
    TObjectPtr<USceneCaptureComponent2D> SceneCaptureComponent;

    UPROPERTY()
    TObjectPtr<USceneCaptureComponent2D> StencilCaptureComponent;

    FBufferRHIRef PositionBuffer;
    FUnorderedAccessViewRHIRef PositionBufferUAV;
    
    void CreateBuffers();
    void ReleaseBuffers();
    void RunComputeShader();
    void UpdateMeshInstances();
    void SetupDepthCapture();
};