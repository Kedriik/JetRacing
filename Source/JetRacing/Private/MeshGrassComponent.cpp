// MeshGrassComponent.cpp
// Implementation of custom mesh grass system

#include "MeshGrassComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "DrawDebugHelpers.h"
#include "Kismet/KismetMathLibrary.h"

UMeshGrassComponent::UMeshGrassComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    bAutoActivate = true;
}

void UMeshGrassComponent::BeginPlay()
{
    Super::BeginPlay();

    if (bAutoGenerate)
    {
        GenerateGrass();
    }
}

void UMeshGrassComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
    ClearGrass();
    Super::OnComponentDestroyed(bDestroyingHierarchy);
}

#if WITH_EDITOR
void UMeshGrassComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    // Auto-regenerate in editor when properties change
    if (PropertyChangedEvent.Property != nullptr)
    {
        const FName PropertyName = PropertyChangedEvent.Property->GetFName();
        if (PropertyName == GET_MEMBER_NAME_CHECKED(UMeshGrassComponent, GrassVarieties) ||
            PropertyName == GET_MEMBER_NAME_CHECKED(UMeshGrassComponent, SamplingGridSize) ||
            PropertyName == GET_MEMBER_NAME_CHECKED(UMeshGrassComponent, TargetMeshComponent) ||
            PropertyName == GET_MEMBER_NAME_CHECKED(UMeshGrassComponent, RandomSeed))
        {
            GenerateGrass();
        }
    }
}
#endif

void UMeshGrassComponent::AutoFindTargetMesh()
{
    if (TargetMeshComponent != nullptr)
    {
        return;
    }

    // Try to find static mesh component on same actor
    AActor* Owner = GetOwner();
    if (Owner)
    {
        TargetMeshComponent = Owner->FindComponentByClass<UStaticMeshComponent>();
    }

    // Try parent component if attached
    if (TargetMeshComponent == nullptr)
    {
        USceneComponent* Parent = GetAttachParent();
        TargetMeshComponent = Cast<UStaticMeshComponent>(Parent);
    }
}

void UMeshGrassComponent::GenerateGrass()
{
    // Clear existing grass
    ClearGrass();

    // Find target mesh if not set
    AutoFindTargetMesh();

    if (TargetMeshComponent == nullptr)
    {
        UE_LOG(LogTemp, Warning, TEXT("MeshGrassComponent: No target mesh component found!"));
        return;
    }

    if (TargetMeshComponent->GetStaticMesh() == nullptr)
    {
        UE_LOG(LogTemp, Warning, TEXT("MeshGrassComponent: Target mesh component has no static mesh!"));
        return;
    }

    // Generate each grass variety
    for (int32 i = 0; i < GrassVarieties.Num(); ++i)
    {
        if (GrassVarieties[i].GrassMesh != nullptr)
        {
            GenerateGrassVariety(i);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("MeshGrassComponent: Generated %d total instances"), GetTotalInstanceCount());
}

void UMeshGrassComponent::GenerateGrassVariety(int32 VarietyIndex)
{
    if (!GrassVarieties.IsValidIndex(VarietyIndex))
    {
        return;
    }

    const FMeshGrassVariety& Variety = GrassVarieties[VarietyIndex];

    if (Variety.GrassMesh == nullptr)
    {
        return;
    }

    // Get or create HISM component
    UHierarchicalInstancedStaticMeshComponent* HISMComponent = GetOrCreateHISMComponent(VarietyIndex);
    if (HISMComponent == nullptr)
    {
        return;
    }

    // Sample mesh surface
    TArray<FTransform> SpawnTransforms = SampleMeshSurface(Variety);

    // Add instances
    for (const FTransform& Transform : SpawnTransforms)
    {
        HISMComponent->AddInstance(Transform);
    }

    // Build tree for efficient rendering
    HISMComponent->BuildTreeIfOutdated(true, true);

    UE_LOG(LogTemp, Log, TEXT("MeshGrassComponent: Generated %d instances for variety %d"),
        SpawnTransforms.Num(), VarietyIndex);
}

TArray<FTransform> UMeshGrassComponent::SampleMeshSurface(const FMeshGrassVariety& Variety)
{
    TArray<FTransform> OutTransforms;

    if (TargetMeshComponent == nullptr)
    {
        return OutTransforms;
    }

    // Initialize random stream with seed
    FRandomStream RandomStream(RandomSeed);

    // Get mesh bounds
    FBox BoundingBox = TargetMeshComponent->Bounds.GetBox();

    // Calculate number of samples based on density
    float Area = (BoundingBox.Max.X - BoundingBox.Min.X) * (BoundingBox.Max.Y - BoundingBox.Min.Y);
    float DensityMultiplier = Variety.GrassDensity / 10000.0f; // Normalize to per-unit-squared
    int32 NumSamples = FMath::RoundToInt(Area * DensityMultiplier);

    // Grid-based sampling
    int32 GridCountX = FMath::CeilToInt((BoundingBox.Max.X - BoundingBox.Min.X) / SamplingGridSize);
    int32 GridCountY = FMath::CeilToInt((BoundingBox.Max.Y - BoundingBox.Min.Y) / SamplingGridSize);

    // Setup trace parameters
    FCollisionQueryParams TraceParams;
    TraceParams.AddIgnoredActor(GetOwner());
    TraceParams.bTraceComplex = true;

    // Sample points
    for (int32 GridX = 0; GridX < GridCountX; ++GridX)
    {
        for (int32 GridY = 0; GridY < GridCountY; ++GridY)
        {
            // Calculate samples per grid cell
            float CellArea = SamplingGridSize * SamplingGridSize;
            int32 SamplesPerCell = FMath::Max(1, FMath::RoundToInt(CellArea * DensityMultiplier));

            for (int32 Sample = 0; Sample < SamplesPerCell; ++Sample)
            {
                // Random point within grid cell
                float X = BoundingBox.Min.X + (GridX + RandomStream.FRand()) * SamplingGridSize;
                float Y = BoundingBox.Min.Y + (GridY + RandomStream.FRand()) * SamplingGridSize;
                float Z = BoundingBox.Max.Z + TraceHeight;

                FVector TraceStart(X, Y, Z);
                FVector TraceEnd(X, Y, BoundingBox.Min.Z - TraceHeight);

                FHitResult HitResult;
                bool bHit = GetWorld()->LineTraceSingleByChannel(
                    HitResult,
                    TraceStart,
                    TraceEnd,
                    ECC_Visibility,
                    TraceParams
                );

                if (bHit && HitResult.GetComponent() == TargetMeshComponent)
                {
                    // Calculate transform
                    FVector Location = HitResult.Location;
                    FRotator Rotation = FRotator::ZeroRotator;

                    // Align to surface normal
                    if (Variety.bAlignToSurface)
                    {
                        FVector UpVector = HitResult.Normal;
                        FVector ForwardVector = FVector::ForwardVector;

                        // Make sure forward is perpendicular to up
                        ForwardVector = FVector::CrossProduct(FVector::RightVector, UpVector);
                        ForwardVector.Normalize();

                        Rotation = UKismetMathLibrary::MakeRotFromXZ(ForwardVector, UpVector);

                        // Add random pitch offset
                        float PitchOffset = RandomStream.FRandRange(
                            Variety.RandomPitchOffset.Min,
                            Variety.RandomPitchOffset.Max
                        );
                        Rotation.Pitch += PitchOffset;
                    }

                    // Random yaw rotation
                    if (Variety.bRandomRotation)
                    {
                        Rotation.Yaw = RandomStream.FRandRange(0.0f, 360.0f);
                    }

                    // Random scale
                    float Scale = RandomStream.FRandRange(Variety.ScaleRange.Min, Variety.ScaleRange.Max);
                    FVector ScaleVector(Scale, Scale, Scale);

                    // Apply surface offset
                    Location += HitResult.Normal * Variety.SurfaceOffset;

                    // Create transform
                    FTransform Transform(Rotation, Location, ScaleVector);
                    OutTransforms.Add(Transform);
                }
            }
        }
    }

    return OutTransforms;
}

UHierarchicalInstancedStaticMeshComponent* UMeshGrassComponent::GetOrCreateHISMComponent(int32 VarietyIndex)
{
    // Ensure array is large enough
    while (GrassComponents.Num() <= VarietyIndex)
    {
        GrassComponents.Add(nullptr);
    }

    // Return existing component if valid
    if (IsValid(GrassComponents[VarietyIndex]))
    {
        GrassComponents[VarietyIndex]->ClearInstances();
        return GrassComponents[VarietyIndex];
    }

    // Create new HISM component
    const FMeshGrassVariety& Variety = GrassVarieties[VarietyIndex];

    UHierarchicalInstancedStaticMeshComponent* HISMComponent = NewObject<UHierarchicalInstancedStaticMeshComponent>(
        GetOwner(),
        UHierarchicalInstancedStaticMeshComponent::StaticClass(),
        *FString::Printf(TEXT("GrassHISM_%d"), VarietyIndex)
    );

    if (HISMComponent)
    {
        HISMComponent->SetStaticMesh(Variety.GrassMesh);
        HISMComponent->SetCullDistances(Variety.CullDistance.Min, Variety.CullDistance.Max);
        HISMComponent->SetCollisionEnabled(Variety.bEnableCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
        HISMComponent->SetCastShadow(Variety.bCastShadow);
        HISMComponent->SetCanEverAffectNavigation(false);
        HISMComponent->SetMobility(EComponentMobility::Static);

        // Attach to this component
        HISMComponent->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform);
        HISMComponent->RegisterComponent();

        GrassComponents[VarietyIndex] = HISMComponent;
    }

    return HISMComponent;
}

void UMeshGrassComponent::ClearGrass()
{
    for (UHierarchicalInstancedStaticMeshComponent* Component : GrassComponents)
    {
        if (IsValid(Component))
        {
            Component->ClearInstances();
            Component->DestroyComponent();
        }
    }
    GrassComponents.Empty();
}

int32 UMeshGrassComponent::GetTotalInstanceCount() const
{
    int32 TotalCount = 0;
    for (const UHierarchicalInstancedStaticMeshComponent* Component : GrassComponents)
    {
        if (IsValid(Component))
        {
            TotalCount += Component->GetInstanceCount();
        }
    }
    return TotalCount;
}