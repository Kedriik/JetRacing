// Copyright 2020 Phyronnaz

#pragma once

#include "CoreMinimal.h"
#include "FastNoise/VoxelFastNoise.h"
#include "VoxelGenerators/VoxelGeneratorHelpers.h"
#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <random>
#include "Windows/WindowsWindow.h"
#include <windows.h>
#include "FastNoiseLite.h"
#include "MyVoxelPlanetGenerator.generated.h"

UCLASS(Blueprintable)
class UMyVoxelPlanetGenerator : public UVoxelGenerator
{
	GENERATED_BODY()
protected:


public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator")
        float NoiseHeight = 10.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator")
        int32 Seed = 1337;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator")
        float Radius = 100;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator")
        float HeightMultiplier = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generator")
        float Epsilon = 1;


	UTexture2D* PlanetTopography = nullptr;
	int* FormatedTopographyImageData = nullptr;
	int* FormatedBiomesImageData = nullptr;
	static int* StaticFormatedTopographyImageData;
	static int* StaticFormatedBiomesImageData;
    int gX, gY;
	int gbX, gbY;
	static int gsX, gsY;
	static int gbsX, gbsY;
	static bool dataPopulated;;
	TArray<float> Epsilons;
	float SmoothingMethod = 1;
	int SmoothSize = 3;
	bool isServer = false;
	//~ Begin UVoxelGenerator Interface
	virtual TVoxelSharedRef<FVoxelGeneratorInstance> GetInstance() override;
    void SetPlanetRadius(float _Radius) override {
        Radius = _Radius;
    };
    void SetHeightMultiplier(float multiplier) override {
        HeightMultiplier = multiplier;
    };
    virtual void SetEpsilon(float epsilon) override {
        Epsilon = epsilon;
    };

	virtual void SetEpsilons(TArray<float> epsilons) override {
		Epsilons = epsilons;
	};
	virtual void SetSmoothingMethod(float method) override {
		SmoothingMethod = method;
	};
	virtual void SetSmoothSize(int size) override {
		SmoothSize = size;
	}

	static void LoadFileInResourceStatic(int name, int type, DWORD& size, const char*& data)
	{
		HMODULE handle = NULL;
		handle = ::GetModuleHandle(L"UnrealEditor-VoxelExamples.dll");
		if (handle == NULL) {
			UE_LOG(LogTemp, Log, TEXT("SpyrraSpells.exe found"));
			handle = ::GetModuleHandle(L"SpyrraSpells.exe");
		}
		if (handle == NULL) {
			UE_LOG(LogTemp, Log, TEXT("SpyrraSpellsServer.exe found"));
			handle = ::GetModuleHandle(L"SpyrraSpellsServer.exe");
		}
		if (handle == NULL) {
			throw "Handle for the application has not been found.";
		}
		HRSRC rc = ::FindResource(handle, MAKEINTRESOURCE(name),
			MAKEINTRESOURCE(type));
		HGLOBAL rcData = ::LoadResource(handle, rc);
		size = ::SizeofResource(handle, rc);
		data = static_cast<const char*>(::LockResource(rcData));
	};

	void LoadFileInResource(int name, int type, DWORD& size, const char*& data)
	{
		HMODULE handle = NULL;
		handle = ::GetModuleHandle(L"UnrealEditor-VoxelExamples.dll");
		if (handle == NULL) {
			UE_LOG(LogTemp, Log, TEXT("SpyrraSpells.exe found"));
			handle = ::GetModuleHandle(L"SpyrraSpells.exe");
		}
		if (handle == NULL) {
			UE_LOG(LogTemp, Log, TEXT("SpyrraSpellsServer.exe found"));
			isServer = true;
			handle = ::GetModuleHandle(L"SpyrraSpellsServer.exe");
		}
		if (handle == NULL) {
			throw "Handle for the application has not been found.";
		}
		HRSRC rc = ::FindResource(handle, MAKEINTRESOURCE(name),
			MAKEINTRESOURCE(type));
		HGLOBAL rcData = ::LoadResource(handle, rc);
		size = ::SizeofResource(handle, rc);
		data = static_cast<const char*>(::LockResource(rcData));
	};
	void SetPlanetTopography()override;

	UFUNCTION(BlueprintCallable, CallInEditor)
	void ReloadPlanetTopography();
	//~ End UVoxelGenerator Interface
};

class FMyVoxelPlanetGeneratorInstance : public TVoxelGeneratorInstanceHelper<FMyVoxelPlanetGeneratorInstance, UMyVoxelPlanetGenerator>
{
public:
    using Super = TVoxelGeneratorInstanceHelper<FMyVoxelPlanetGeneratorInstance, UMyVoxelPlanetGenerator>;

	explicit FMyVoxelPlanetGeneratorInstance( UMyVoxelPlanetGenerator& MyGenerator);

	//~ Begin FVoxelGeneratorInstance Interface
	virtual void Init(const FVoxelGeneratorInit& InitStruct) override;

	v_flt GetValueImpl(v_flt X, v_flt Y, v_flt Z, int32 LOD, const FVoxelItemStack& Items) const;
	FVoxelMaterial GetMaterialImpl(v_flt X, v_flt Y, v_flt Z, int32 LOD, const FVoxelItemStack& Items) const;

	TVoxelRange<v_flt> GetValueRangeImpl(const FVoxelIntBox& Bounds, int32 LOD, const FVoxelItemStack& Items) const;

	virtual FVector GetUpVector(v_flt X, v_flt Y, v_flt Z) const override final;
	//~ End FVoxelGeneratorInstance Interface
	FVector2D generateUV(FVector pos)const;
	FVector FindTangentAtPoint(FVector position, float thetaOffset, float phiOffset) const;
	float getTopographyAt(FVector2D uv) const;
	float getBiomeAt(FVector2D uv) const;

private:
	std::uniform_real_distribution<float> urd;
	int* FormatedTopographyImageData = nullptr;
	int* FormatedBiomesImageData = nullptr;
    int gX, gY;
	int gbX, gbY;
    bool firstLookup = true;
	const float NoiseHeight;
	const int32 Seed;
    float Epsilon=0.001;
    const float Radius;
	FVoxelFastNoise Noise;
    UTexture2D* PlanetTopography = nullptr;
    float HeightMultiplier;
	TArray<float> Epsilons;
	float SmoothingMethod = 1;
	int SmoothSize = 3;
	bool isServer;
	FastNoiseLite noise;
	std::vector<float> Weigths;
};