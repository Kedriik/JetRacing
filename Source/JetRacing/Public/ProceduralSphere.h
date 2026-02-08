// Fill out your copyright notice in the Description page of Project Settings.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "ProceduralSphere.generated.h"


UCLASS()
class JETRACING_API AProceduralSphere : public AActor
{
public:
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Materials")
		UMaterialInterface* material;

	UPROPERTY(EditAnywhere, Category = "OceanProperties")
		float Radius = 790000;

	UPROPERTY(EditAnywhere, Category = "GeometryProperties")
		float sectorCount = 100;

	UPROPERTY(EditAnywhere, Category = "GeometryProperties")
		float stackCount = 100;

public:	
	// Sets default values for this actor's properties
	AProceduralSphere();
	UFUNCTION(CallInEditor, category = "generate")
		void generateSphere();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void PostActorCreated() override;
	virtual void PostLoad() override;
	UProceduralMeshComponent* mesh;


public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};