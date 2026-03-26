#include "ComputeShaderDeclaration.h"
#include "ShaderCompilerCore.h"

// Static initialization — runs when the module DLL loads.
// Maps the virtual shader path used in IMPLEMENT_GLOBAL_SHADER to the
// on-disk directory that contains InstancesComputeShader.usf.
static struct FJetRacingShaderInit
{
    FJetRacingShaderInit()
    {
        FString ShaderDirectory = FPaths::Combine(FPaths::ProjectDir(), TEXT("Shaders"));

        if (GLog)
        {
            GLog->Log(TEXT("LogTemp"), ELogVerbosity::Log, TEXT("==========================================="));
            GLog->Log(TEXT("LogTemp"), ELogVerbosity::Log, *FString::Printf(TEXT("SHADER INIT: Project Dir = %s"), *FPaths::ProjectDir()));
            GLog->Log(TEXT("LogTemp"), ELogVerbosity::Log, *FString::Printf(TEXT("SHADER INIT: Shader Dir  = %s"), *ShaderDirectory));
            GLog->Log(TEXT("LogTemp"), ELogVerbosity::Log, *FString::Printf(TEXT("SHADER INIT: Dir Exists  = %s"), FPaths::DirectoryExists(ShaderDirectory) ? TEXT("YES") : TEXT("NO")));

            FString ShaderFile = FPaths::Combine(ShaderDirectory, TEXT("Private/InstancesComputeShader.usf"));
            GLog->Log(TEXT("LogTemp"), ELogVerbosity::Log, *FString::Printf(TEXT("SHADER INIT: File Path   = %s"), *ShaderFile));
            GLog->Log(TEXT("LogTemp"), ELogVerbosity::Log, *FString::Printf(TEXT("SHADER INIT: File Exists = %s"), FPaths::FileExists(ShaderFile) ? TEXT("YES") : TEXT("NO")));
            GLog->Log(TEXT("LogTemp"), ELogVerbosity::Log, TEXT("==========================================="));
        }

        AddShaderSourceDirectoryMapping(TEXT("/Project/JetRacing"), ShaderDirectory);
    }
} GJetRacingShaderInit;

IMPLEMENT_GLOBAL_SHADER(FInstancesComputeShader,
    "/Project/JetRacing/Private/InstancesComputeShader.usf",
    "MainCS",
    SF_Compute);
