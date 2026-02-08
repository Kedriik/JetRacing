// Fill out your copyright notice in the Description page of Project Settings.

#include "Anomaly.h"
#include "Misc/OutputDeviceNull.h"
#include "Kismet/KismetSystemLibrary.h"
// Sets default values
AAnomaly::AAnomaly()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	MoveNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
	

}

// Called when the game starts or when spawned
void AAnomaly::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AAnomaly::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	const FString command = FString::Printf(TEXT("UpdatePaintVolume %f"), DeltaTime);
	FOutputDeviceNull OutputDeviceNull;
	CallFunctionByNameWithArguments(*command, OutputDeviceNull, this, true);
	//CallFunctionByNameWithArguments(TEXT("Test Function"), OutputDeviceNull, this, true);
	//CallFunctionByNameWithArguments(TEXT("TestFunction"), OutputDeviceNull, this, true);
	
	
	lifeTime += DeltaTime;

	if (maxLifeTime !=0 && lifeTime >= maxLifeTime) {
		Destroy();
	}

	moveActorRandomly();
	if(alignToGravity){
		orientActor();
		
			
	}
	if(adjustReferencePlane)
		if (FMath::FRandRange(0.f, 10.f)<=1.0f)
			adjustToTerrain();
	else{
		touchGround();
	}
}

void AAnomaly::orientActor() {
	FVector location = this->GetActorLocation();
	location.Normalize();
	FRotator zRotator = FRotationMatrix::MakeFromZ(1.0f * location).Rotator();
	this->SetActorRotation(zRotator);
}

void AAnomaly::touchGround() {
	TArray < AActor* > actorsToIgnore;
	TArray < TEnumAsByte < EObjectTypeQuery >> traceObjectTypes;
	traceObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_WorldStatic));
	FHitResult hitResult;
	FVector currentToCenter = this->GetActorLocation();
	currentToCenter.Normalize();

	if(UKismetSystemLibrary::LineTraceSingleForObjects(GetWorld(), this->GetActorLocation()+1000.0f*currentToCenter, FVector(0, 0, 0),
		traceObjectTypes, true, actorsToIgnore, EDrawDebugTrace::None, hitResult, true)){ 
		FVector toCenter = hitResult.Location;
		toCenter.Normalize();

		if (hitResult.GetActor()->ActorHasTag(actorName)) {
			this->SetActorLocation(hitResult.Location + zOffset * toCenter);
		}
		else {
			//TODO: What happens if not touching designated actor.
		}
	}
}

void AAnomaly::moveActorRandomly() {
	FVector forward = this->GetActorForwardVector();
	FVector right = this->GetActorRightVector();
	FVector currentLocation = this->GetActorLocation();
	FVector newLocation = this->GetActorLocation();
	newLocation += forward * (movingNoiseAmplitude + movingNoiseAmplitude * MoveNoise.GetNoise(currentLocation.X * movingNoiseFrequency, currentLocation.Y * movingNoiseFrequency, currentLocation.Z * movingNoiseFrequency));
	if(!moveOnlyForward){
		newLocation += right   * movingNoiseAmplitude * MoveNoise.GetNoise(currentLocation.Z * movingNoiseFrequency, currentLocation.Y * movingNoiseFrequency, currentLocation.X * movingNoiseFrequency);
	}
	this->SetActorLocation(newLocation);
}

void AAnomaly::adjustToTerrain(){
	FTransform PlaneTransform = GetActorTransform();
	TArray<FVector> Points;

	for (int i = 0; i < referencePointsNumber; ++i)
	{
		// Generate random polar coordinates
		float Theta = FMath::FRandRange(0.f, 2.f * PI);
		float r = radius * FMath::Sqrt(FMath::FRand()); // sqrt ensures uniform distribution over circle

		// Convert polar to Cartesian (in local XY plane)
		FVector LocalPoint(
			r * FMath::Cos(Theta),
			r * FMath::Sin(Theta),
			0.0f
		);

		// Transform to world space using the plane's transform
		FVector WorldPoint = GetActorTransform().TransformPosition(LocalPoint);
		Points.Add(WorldPoint);
	}


	TArray < AActor* > actorsToIgnore;
	TArray < TEnumAsByte < EObjectTypeQuery >> traceObjectTypes;
	traceObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_WorldStatic));
	FHitResult hitResult;
	std::vector<std::pair<FVector, float>> distances;
	for (auto Point : Points) {
		FVector currentToCenter = Point;
		currentToCenter.Normalize();

		if (UKismetSystemLibrary::LineTraceSingleForObjects(GetWorld(), Point + 1000.0f * currentToCenter, FVector(0, 0, 0),
			traceObjectTypes, true, actorsToIgnore, EDrawDebugTrace::None, hitResult, true)) {
			distances.push_back(std::pair{ Point , FVector::Dist(Point,hitResult.Location) });
		}
	}

	auto it = std::max_element(distances.begin(), distances.end(),
		[](auto const& a, auto const& b) {
			return a.second < b.second;
		});

	if (it != distances.end()) {
		float biggest = it->second;
		FVector currentToCenter = this->GetActorLocation();
		currentToCenter.Normalize();
		this->SetActorLocation(this->GetActorLocation() + zOffset * currentToCenter);
		
	}

	/*for (const FVector& P : Points)
	{
		DrawDebugPoint(
			GetWorld(),
			P,
			1.0f,              // point size (bigger = more visible)
			FColor::Green,
			false,              // persistent lines
			0.1f                // lifetime in seconds
		);
	}*/
}

