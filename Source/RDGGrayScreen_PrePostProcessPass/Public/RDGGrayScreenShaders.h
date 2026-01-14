#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"

/**
 * GrayScreen Compute Shader
 */
class FRDGGrayScreenShaders : public FGlobalShader
{
public:
    DECLARE_GLOBAL_SHADER(FRDGGrayScreenShaders);
    SHADER_USE_PARAMETER_STRUCT(FRDGGrayScreenShaders, FGlobalShader);

    // shader permutation: gamma 곱셈 최적화
    class FGammaEnabled : SHADER_PERMUTATION_BOOL("GAMMA_ENABLED");
    using FPermutationDomain = TShaderPermutationDomain<FGammaEnabled>;

    // shader parameters
    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
        SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
        SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)
        SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutputTexture)
        SHADER_PARAMETER(float, GrayScreen)
        SHADER_PARAMETER(float, GrayGamma)
        SHADER_PARAMETER(FVector2f, InputTextureSize)
    END_SHADER_PARAMETER_STRUCT()

    // permutation을 컴파일할지 결정. @return true면 컴파일
    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& parameters);

    // shader 컴파일 환경 설정. permutation에 따라 #define 추가
    static void ModifyCompilationEnvironment(
        const FGlobalShaderPermutationParameters& parameters
        , FShaderCompilerEnvironment& out_environment);
};
