#include "RDGGrayScreenShaders.h"


// shader 등록 (USF 파일과 연결)
IMPLEMENT_GLOBAL_SHADER(FRDGGrayScreenShaders, "/Plugin/RDGGrayScreen_PrePostProcessPass/Private/GrayScreen.usf", "MainPS", SF_Compute);


bool FRDGGrayScreenShaders::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& parameters)
{
    // 모든 플랫폼에서 컴파일
    return true;
}

void FRDGGrayScreenShaders::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& parameters, FShaderCompilerEnvironment& out_environment)
{
    FGlobalShader::ModifyCompilationEnvironment(parameters, out_environment);

    // 최적화 플래그
    out_environment.CompilerFlags.Add(CFLAG_StandardOptimization);
    
    // permutation에 따라 #define 추가
    FPermutationDomain permutation_vector(parameters.PermutationId);
    if (permutation_vector.Get<FGammaEnabled>())
    {
        out_environment.SetDefine(TEXT("GAMMA_ENABLED"), 1);
    }
    else 
    {
        out_environment.SetDefine(TEXT("GAMMA_ENABLED"), 0);
    }
}
