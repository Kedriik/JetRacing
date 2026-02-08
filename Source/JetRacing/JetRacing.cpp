// Copyright Epic Games, Inc. All Rights Reserved.

#include "JetRacing.h"
#include "Modules/ModuleManager.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"

void FJetRacingModule::StartupModule()
{
    FString ShaderDirectory = FPaths::Combine(FPaths::ProjectDir(), TEXT("Shaders"));
    // DEBUG: Print the exact path
    UE_LOG(LogTemp, Error, TEXT("========================================"));
    UE_LOG(LogTemp, Error, TEXT("Project Dir: %s"), *FPaths::ProjectDir());
    UE_LOG(LogTemp, Error, TEXT("Shader Dir: %s"), *ShaderDirectory);
    UE_LOG(LogTemp, Error, TEXT("Does Shader Dir Exist? %s"), FPaths::DirectoryExists(ShaderDirectory) ? TEXT("YES") : TEXT("NO"));

    FString ShaderFilePath = FPaths::Combine(ShaderDirectory, TEXT("Private/InstancesComputeShader.usf"));
    UE_LOG(LogTemp, Error, TEXT("Full Shader Path: %s"), *ShaderFilePath);
    UE_LOG(LogTemp, Error, TEXT("Does Shader File Exist? %s"), FPaths::FileExists(ShaderFilePath) ? TEXT("YES") : TEXT("NO"));
    UE_LOG(LogTemp, Error, TEXT("========================================"));

    AddShaderSourceDirectoryMapping(TEXT("/Project/JetRacing"), ShaderDirectory);
}

void FJetRacingModule::ShutdownModule()
{
}




//IMPLEMENT_PRIMARY_GAME_MODULE(FJetRacingModule, JetRacing, "JetRacing");

IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, JetRacing, "JetRacing");