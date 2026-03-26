#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphUtils.h"

/**
 * Depth-driven foliage compute shader.
 *
 * Reads an orthographic top-down depth render target and writes one
 * MeshRenderInstance (3×float4 column-major TRS matrix) per grid cell into
 * SpawnInstances.  It also atomically increments IndirectArgs[1] (InstanceCount)
 * so the indirect draw call knows how many live instances were placed.
 *
 * Both output buffers are owned by the scene proxy and must be created before
 * this shader is dispatched.
 */
class FInstancesComputeShader : public FGlobalShader
{
    DECLARE_GLOBAL_SHADER(FInstancesComputeShader);
    SHADER_USE_PARAMETER_STRUCT(FInstancesComputeShader, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
        // Outputs — written by the compute shader, read by the vertex factory
        SHADER_PARAMETER_UAV(RWStructuredBuffer<FVector4f>, SpawnInstances)   // MeshRenderInstance[]
        SHADER_PARAMETER_UAV(RWBuffer<uint32>,              IndirectArgs)     // DrawIndexedIndirect args

        // Depth capture inputs
        SHADER_PARAMETER_TEXTURE(Texture2D, SceneDepthTexture)
        SHADER_PARAMETER_SAMPLER(SamplerState, SceneDepthSampler)

        // Camera / projection
        SHADER_PARAMETER(FVector3f, CameraPosition)
        SHADER_PARAMETER(FVector3f, CameraForward)
        SHADER_PARAMETER(FVector3f, CameraRight)
        SHADER_PARAMETER(FVector3f, CameraUp)
        SHADER_PARAMETER(float,     OrthoWidth)
        SHADER_PARAMETER(float,     OrthoHeight)

        // Spawn parameters
        SHADER_PARAMETER(uint32, NumInstances)
        SHADER_PARAMETER(uint32, MeshNumIndices)
        SHADER_PARAMETER(float,  GridCellSize)
        SHADER_PARAMETER(float,  SpawnDensity)
        SHADER_PARAMETER(float,  VerticalOffset)
        SHADER_PARAMETER(float,  ScaleMin)
        SHADER_PARAMETER(float,  ScaleMax)
    END_SHADER_PARAMETER_STRUCT()

public:
    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
    {
        return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
    }

    static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
    {
        FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
    }
};
