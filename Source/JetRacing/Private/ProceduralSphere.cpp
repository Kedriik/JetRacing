// Fill out your copyright notice in the Description page of Project Settings.


#include "ProceduralSphere.h"
#include "PhysicsEngine/BodySetup.h"



// Sets default values
AProceduralSphere::AProceduralSphere()
{
    mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("mesh"));
    static_cast<UProceduralMeshComponent*>(mesh)->bUseAsyncCooking = false;
    static_cast<UProceduralMeshComponent*>(mesh)->SetMeshSectionVisible(1, false);
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AProceduralSphere::BeginPlay()
{
	Super::BeginPlay();
	
}

void AProceduralSphere::PostActorCreated()
{
    generateSphere();
    Super::PostActorCreated();
}
void AProceduralSphere::PostLoad() {
    generateSphere();
    Super::PostLoad();
}
// Called every frame
void AProceduralSphere::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AProceduralSphere::generateSphere()
{
    static_cast<UProceduralMeshComponent*>(mesh)->ClearAllMeshSections();
    float sphereScalingFactor = 1.0;
    TArray<FVector> vertices;
    TArray<FVector> normals;
    TArray<FVector2D> texCoords;

    float x, y, z, xy;                              // vertex position
    float nx, ny, nz, lengthInv = 1.0f / (Radius * sphereScalingFactor);    // vertex normal
    float s, t;                                     // vertex texCoord

    float sectorStep = 2 * PI / sectorCount;
    float stackStep = PI / stackCount;
    float sectorAngle, stackAngle;

    for (int i = 0; i <= stackCount; ++i)
    {
        stackAngle = PI / 2 - i * stackStep;        // starting from pi/2 to -pi/2
        xy = (Radius * sphereScalingFactor) * cosf(stackAngle);             // r * cos(u)
        z = (Radius * sphereScalingFactor) * sinf(stackAngle);              // r * sin(u)

        // add (sectorCount+1) vertices per stack
        // the first and last vertices have same position and normal, but different tex coords
        for (int j = 0; j <= sectorCount; ++j)
        {
            sectorAngle = j * sectorStep;           // starting from 0 to 2pi

            // vertex position (x, y, z)
            x = xy * cosf(sectorAngle);             // r * cos(u) * cos(v)
            y = xy * sinf(sectorAngle);             // r * cos(u) * sin(v)
            vertices.Add(FVector(x, y, z));

            // normalized vertex normal (nx, ny, nz)
            nx = x * lengthInv;
            ny = y * lengthInv;
            nz = z * lengthInv;
            normals.Add(FVector(nx, ny, nz));

            // vertex tex coord (s, t) range between [0, 1]
            s = (float)j / sectorCount;
            t = (float)i / stackCount;
            texCoords.Add(FVector2D(s,t));
        }
    }

    TArray<int32> indices;
    TArray<int> lineIndices;
    int k1, k2;
    for (int i = 0; i < stackCount; ++i)
    {
        k1 = i * (sectorCount + 1);     // beginning of current stack
        k2 = k1 + sectorCount + 1;      // beginning of next stack

        for (int j = 0; j < sectorCount; ++j, ++k1, ++k2)
        {
            // 2 triangles per sector excluding first and last stacks
            // k1 => k2 => k1+1
            if (i != 0)
            {

                indices.Add(k1 + 1);
                indices.Add(k2);
                indices.Add(k1);
            }

            // k1+1 => k2 => k2+1
            if (i != (stackCount - 1))
            {
                indices.Add(k2 + 1);
                indices.Add(k2);
                indices.Add(k1 + 1);
            }

            // store indices for lines
            // vertical lines for all stacks, k1 => k2
            lineIndices.Add(k1);
            lineIndices.Add(k2);
            if (i != 0)  // horizontal lines except 1st stack, k1 => k+1
            {
                lineIndices.Add(k1);
                lineIndices.Add(k1 + 1);
            }
        }
    }

    TArray<FLinearColor> vertexColors;
  

    TArray<FProcMeshTangent> tangents;
    static_cast<UProceduralMeshComponent*>(mesh)->CreateMeshSection_LinearColor(0, vertices, indices, normals, texCoords, vertexColors, tangents, true);
    static_cast<UProceduralMeshComponent*>(mesh)->GetBodySetup()->bDoubleSidedGeometry = true;
    if (material)
        static_cast<UProceduralMeshComponent*>(mesh)->SetMaterial(0, material);
}

