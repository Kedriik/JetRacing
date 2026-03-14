// Copyright Epic Games, Inc. All Rights Reserved.
// Adapted from the VirtualHeightfieldMesh plugin

#include "IndirectInstancing/Public/ExampleIndirectInstancing/ExampleIndirectInstancingComponent.h"

#include "Engine/World.h"
#include "ExampleIndirectInstancingSceneProxy.h"
#include "Materials/MaterialInterface.h"

UExampleIndirectInstancingComponent::UExampleIndirectInstancingComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CastShadow = true;
	bCastContactShadow = false;
	bUseAsOccluder = true;
	bAffectDynamicIndirectLighting = false;
	bAffectDistanceFieldLighting = false;
	bNeverDistanceCull = true;
#if WITH_EDITORONLY_DATA
	bEnableAutoLODGeneration = false;
#endif
	Mobility = EComponentMobility::Static;
}

void UExampleIndirectInstancingComponent::OnRegister()
{
	Super::OnRegister();
	SetMobility( EComponentMobility::Movable );	

	// Ensure required shader permutations are compiled on the game thread.
	if (Material != nullptr)
	{
		Material->CheckMaterialUsage(MATUSAGE_VirtualHeightfieldMesh);
	}
}

void UExampleIndirectInstancingComponent::OnUnregister()
{
	Super::OnUnregister();
}

void UExampleIndirectInstancingComponent::ApplyWorldOffset(const FVector& InOffset, bool bWorldShift)
{
	Super::ApplyWorldOffset(InOffset, bWorldShift);
	MarkRenderStateDirty();
}

bool UExampleIndirectInstancingComponent::IsVisible() const
{
	return Super::IsVisible();
}

FBoxSphereBounds UExampleIndirectInstancingComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	return FBoxSphereBounds(FBox(FVector(0.f, 0.f, 0.f), FVector(10000.f))).TransformBy(LocalToWorld);
}

FPrimitiveSceneProxy* UExampleIndirectInstancingComponent::CreateSceneProxy()
{
	return new FExampleIndirectInstancingSceneProxy(this);
}

void UExampleIndirectInstancingComponent::SetMaterial(int32 InElementIndex, UMaterialInterface* InMaterial)
{
	if (InElementIndex == 0 && Material != InMaterial)
	{
		Material = InMaterial;
		if (Material != nullptr)
		{
			Material->CheckMaterialUsage(MATUSAGE_VirtualHeightfieldMesh);
		}
		MarkRenderStateDirty();
	}
}

void UExampleIndirectInstancingComponent::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const
{
	if (Material != nullptr)
	{
		OutMaterials.Add(Material);
	}
}
