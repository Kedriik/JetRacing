#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphUtils.h"

class FInstancesComputeShader : public FGlobalShader
{
    DECLARE_GLOBAL_SHADER(FInstancesComputeShader);
    SHADER_USE_PARAMETER_STRUCT(FInstancesComputeShader, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
        SHADER_PARAMETER_UAV(RWStructuredBuffer<FVector4f>, SpawnPositions)
        SHADER_PARAMETER_TEXTURE(Texture2D, SceneDepthTexture)
        SHADER_PARAMETER_SAMPLER(SamplerState, SceneDepthSampler)
        SHADER_PARAMETER_TEXTURE(Texture2D, StencilTexture)
        SHADER_PARAMETER_SAMPLER(SamplerState, StencilSampler)
        SHADER_PARAMETER(FVector3f, CameraPosition)
        SHADER_PARAMETER(FVector3f, CameraForward)
        SHADER_PARAMETER(FVector3f, CameraRight)
        SHADER_PARAMETER(FVector3f, CameraUp)
        SHADER_PARAMETER(float, OrthoWidth)
        SHADER_PARAMETER(float, OrthoHeight)
        SHADER_PARAMETER(uint32, NumInstances)
        SHADER_PARAMETER(float, GridCellSize)
        SHADER_PARAMETER(float, SpawnDensity)
        SHADER_PARAMETER(float, VerticalOffset)
        SHADER_PARAMETER(float, MaxRayDistance)
        SHADER_PARAMETER(float, RaymarchStepSize)
        SHADER_PARAMETER(uint32, MaxRaymarchSteps)
        SHADER_PARAMETER(uint32, TargetStencilValue)
    END_SHADER_PARAMETER_STRUCT()

public:
    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
    {
        return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
    }

    static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
    {
        FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
        OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), 64);
    }
};