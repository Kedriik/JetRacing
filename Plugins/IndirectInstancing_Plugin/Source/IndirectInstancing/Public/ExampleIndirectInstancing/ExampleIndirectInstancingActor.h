// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ExampleIndirectInstancingActor.generated.h"

UCLASS(hidecategories = (Cooking, Input, LOD), MinimalAPI)
class AExampleIndirectInstancing : public AActor
{
	GENERATED_UCLASS_BODY()

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	class UExampleIndirectInstancingComponent* ExampleIndirectInstancingComponent;

protected:
};
