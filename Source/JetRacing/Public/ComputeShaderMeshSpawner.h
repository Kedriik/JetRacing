#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "RHI.h"
#include "RHIResources.h"
#include "ComputeShaderMeshSpawner.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class JETRACING_API UComputeShaderMeshSpawner : public UActorComponent
{
    GENERATED_BODY()

public:
    UComputeShaderMeshSpawner();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning")
    UStaticMesh* MeshToSpawn;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning")
    int32 NumInstances = 100;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning")
    float SpawnRadius = 1000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning")
    bool bUpdateEveryFrame = true;

    UFUNCTION(BlueprintCallable, Category = "Spawning")
    void ExecuteComputeShader();

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    UPROPERTY()
    TObjectPtr<UInstancedStaticMeshComponent> InstancedMeshComponent;

    FBufferRHIRef PositionBuffer;
    FUnorderedAccessViewRHIRef PositionBufferUAV;

    void CreateBuffers();
    void ReleaseBuffers();
    void RunComputeShader(float Time);
    void UpdateMeshInstances();
};