// Copyright Epic Games, Inc. All Rights Reserved.
// Adapted from the VirtualHeightfieldMesh plugin

#include "IndirectInstancing/Public/ExampleIndirectInstancing/ExampleIndirectInstancingComponent.h"

#include "Engine/World.h"
#include "ExampleIndirectInstancingSceneProxy.h"
#include "Materials/MaterialInterface.h"
#include "Math/RandomStream.h"

UExampleIndirectInstancingComponent::UExampleIndirectInstancingComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CastShadow = true;
	bCastContactShadow = true;
	bUseAsOccluder = true;
	bAffectDynamicIndirectLighting = true;
	bAffectDistanceFieldLighting = true;
	bNeverDistanceCull = true;
#if WITH_EDITORONLY_DATA
	bEnableAutoLODGeneration = false;
#endif
	Mobility = EComponentMobility::Static;
}

void UExampleIndirectInstancingComponent::PopulateRandomInstances()
{
	InstanceTransforms.Reset(RandomInstanceCount);
	FRandomStream Stream(RandomSeed);

	for (int32 i = 0; i < RandomInstanceCount; ++i)
	{
		// Local-space XY offset — the component's LocalToWorld in the vertex
		// shader will map these into the correct world positions automatically.
		const float X = Stream.FRandRange(-SpawnRadius, SpawnRadius);
		const float Y = Stream.FRandRange(-SpawnRadius, SpawnRadius);
		const FRotator Rotation(0.f, Stream.FRandRange(0.f, 360.f), 0.f);
		const float Scale = Stream.FRandRange(ScaleMin, ScaleMax);

		FTransform T;
		T.SetLocation(FVector(X, Y, 0.f));
		T.SetRotation(Rotation.Quaternion());
		T.SetScale3D(FVector(Scale));
		InstanceTransforms.Add(T);
	}
}

void UExampleIndirectInstancingComponent::RegenerateRandomInstances()
{
	PopulateRandomInstances();
	MarkRenderStateDirty();
}

void UExampleIndirectInstancingComponent::OnRegister()
{
	Super::OnRegister();
	SetMobility(EComponentMobility::Movable);

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
	// Instance transforms are local-space — world origin rebasing doesn't affect them.
	Super::ApplyWorldOffset(InOffset, bWorldShift);
	MarkRenderStateDirty();
}

bool UExampleIndirectInstancingComponent::IsVisible() const
{
	return Super::IsVisible();
}

FBoxSphereBounds UExampleIndirectInstancingComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	if (InstanceTransforms.Num() > 0)
	{
		FBox LocalBounds(ForceInit);
		for (const FTransform& T : InstanceTransforms)
		{
			LocalBounds += T.GetTranslation();
		}
		LocalBounds = LocalBounds.ExpandBy(FVector(ScaleMax * 200.f));
		return FBoxSphereBounds(LocalBounds).TransformBy(LocalToWorld);
	}
	return FBoxSphereBounds(FBox(FVector(-SpawnRadius), FVector(SpawnRadius))).TransformBy(LocalToWorld);
}

FPrimitiveSceneProxy* UExampleIndirectInstancingComponent::CreateSceneProxy()
{
	if (Mesh == nullptr || Mesh->GetRenderData() == nullptr || Mesh->GetRenderData()->LODResources.Num() == 0)
	{
		return nullptr;
	}

	// Generate instances in local space right before the proxy reads them.
	// Local space means we never need a valid world transform here.
	if (bAutoSpawnInstances && InstanceTransforms.Num() == 0)
	{
		PopulateRandomInstances();
	}

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
