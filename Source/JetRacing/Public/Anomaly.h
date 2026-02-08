// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FastNoiseLite.h"
#include <string>
#include "Anomaly.generated.h"

UCLASS()
class JETRACING_API AAnomaly : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AAnomaly();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	float lifeTime = 0;
	FastNoiseLite MoveNoise;

	

	virtual bool ShouldTickIfViewportsOnly()const override {
		return viewportTick;
	}
	


public:	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnomalyProperties")
		float maxLifeTime = 60;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnomalyProperties")
		float movingNoiseAmplitude = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnomalyProperties")
		float movingNoiseFrequency = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnomalyProperties")
		float zOffset = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnomalyProperties")
		bool moveOnlyForward = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnomalyProperties")
		bool alignToGravity = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnomalyProperties")
		FName actorName = FName(TEXT("VoxelWorld"));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnomalyProperties")
		bool viewportTick = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnomalyProperties")
		float radius = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnomalyProperties")
		int referencePointsNumber = 10;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnomalyProperties")
		bool adjustReferencePlane = false;

	
	UFUNCTION(BlueprintCallable)
	void orientActor();
	UFUNCTION(BlueprintCallable)
	void touchGround();
	UFUNCTION(BlueprintCallable)
	void moveActorRandomly();
	UFUNCTION(BlueprintCallable)
	void adjustToTerrain();
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
