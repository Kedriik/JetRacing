// Copyright Epic Games, Inc. All Rights Reserved.

#include "IndirectInstancing/Public/ExampleIndirectInstancing/ExampleIndirectInstancingActor.h"
#include "IndirectInstancing/Public/ExampleIndirectInstancing/ExampleIndirectInstancingComponent.h"

AExampleIndirectInstancing::AExampleIndirectInstancing(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RootComponent = ExampleIndirectInstancingComponent = CreateDefaultSubobject<UExampleIndirectInstancingComponent>(TEXT("ExampleIndirectInstancingComponent"));
}
