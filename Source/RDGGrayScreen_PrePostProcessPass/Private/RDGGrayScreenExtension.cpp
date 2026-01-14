// Fill out your copyright notice in the Description page of Project Settings.


#include "RDGGrayScreenExtension.h"
#include "RDGGrayScreenSubsystem.h"
#include "RDGGrayScreenShaders.h"
#include "RDGGrayScreenLog.h"
#include "PostProcess/PostProcessInputs.h"


FRDGGrayScreenExtension::FRDGGrayScreenExtension(const FAutoRegister& auto_register, UWorld* in_world,
                                                 URDGGrayScreenSubsystem* in_subsystem)
		: FWorldSceneViewExtension(auto_register, in_world), _world_subsystem(in_subsystem)
{
	UE_LOG(LogRDGGrayScreen, Log, TEXT("GrayScreenExtension: Created! (FWorldSceneViewExtension"));
}

void FRDGGrayScreenExtension::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
	// game thread / render thread 양쪽에서 호출 가능
	// editor viewport는 game thread에서 호출
	// PIE/Game은 render thread에서 호출
	if (::IsValid(_world_subsystem))
	{
		_world_subsystem->TransferStateToExtension(this);
	}
}

void FRDGGrayScreenExtension::PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& InView,
	const FPostProcessingInputs& Inputs)
{
	check(::IsInRenderingThread());

	// render thread 데이터 읽기
	float local_gray_screen, local_gray_gamma;
	GetParameters(local_gray_screen, local_gray_gamma);

	if (FMath::IsNearlyZero(local_gray_screen))
	{
		return;
	}

	/////////////////////////////////////////////////////////
	// Debug log
	static double last_logtime = 0.0;
	double curt_time = FPlatformTime::Seconds();
	if (curt_time - last_logtime > 1.0)
	{
		UE_LOG(LogRDGGrayScreen, Warning, TEXT("GrayScreenExtension [RT]: GrayScreen=%.3f, GrayGamma=%.3f"),
			local_gray_screen, local_gray_gamma);
		last_logtime = curt_time;
	}
	/////////////////////////////////////////////////////////

	// 입력 검증
	Inputs.Validate();

	FRDGTextureRef scene_color_texture = (*Inputs.SceneTextures)->SceneColorTexture;
	if (!scene_color_texture)
	{
		UE_LOG(LogRDGGrayScreen, Error, TEXT("GrayScreenExtension: SceneColorTexture is NULL!"));
		return;
	}

	// 전체 텍스처 크기
	FIntPoint texture_size = scene_color_texture->Desc.Extent;

	// output_texture를 SceneColor와 같은 크기로 생성
	FRDGTextureDesc output_desc = scene_color_texture->Desc;
	output_desc.Flags |= TexCreate_UAV | TexCreate_ShaderResource;
	output_desc.ClearValue = FClearValueBinding::Black;
	FRDGTexture* output_texture = GraphBuilder.CreateTexture(output_desc, TEXT("GrayScreenOutput"));

    // shader parameters 설정
	FRDGGrayScreenShaders::FParameters* pass_parameters = GraphBuilder.AllocParameters<FRDGGrayScreenShaders::FParameters>();
	pass_parameters->InputTexture = scene_color_texture;
	pass_parameters->InputSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	pass_parameters->OutputTexture = GraphBuilder.CreateUAV(output_texture);
	pass_parameters->GrayScreen = local_gray_screen;
	pass_parameters->GrayGamma = local_gray_gamma;
	pass_parameters->InputTextureSize = FVector2f(texture_size.X, texture_size.Y);

	// shader permutation 설정
	FRDGGrayScreenShaders::FPermutationDomain permutation_vector;
	bool is_gamma_enabled = !FMath::IsNearlyEqual(local_gray_gamma, 1.0f, KINDA_SMALL_NUMBER);
	permutation_vector.Set<FRDGGrayScreenShaders::FGammaEnabled>(is_gamma_enabled);

	// shader 가져오기
	FGlobalShaderMap* global_shader_map = GetGlobalShaderMap(InView.FeatureLevel);
	TShaderMapRef<FRDGGrayScreenShaders> compute_shader(global_shader_map, permutation_vector);

	if (!compute_shader.IsValid())
	{
		UE_LOG(LogRDGGrayScreen, Error, TEXT("GrayScreenExtension: Shader not found!"));
		return;
	}

    // thread group 크기 계산
	const int32 thread_group_size = 8;
	FIntVector thread_group_count(
		FMath::DivideAndRoundUp(texture_size.X, thread_group_size)
		, FMath::DivideAndRoundUp(texture_size.Y, thread_group_size)
		, 1);

	// rdg grp stat scope
	RDG_GPU_STAT_SCOPE(GraphBuilder, RDGGrayScreenEffect);

	// 상세한 이벤트 이름
	FString event_name = FString::Printf(
		TEXT("RDGGrayScreenPass(Screen=%.2f,Gamma=%.2f,Permutation=%s)")
		, local_gray_screen
		, local_gray_gamma
		, is_gamma_enabled ? TEXT("GammaEnabled") : TEXT("GammaDisabled")
	);

    // compute pass 추가
	FComputeShaderUtils::AddPass(
		GraphBuilder
		, RDG_EVENT_NAME("%s", *event_name)
		, ERDGPassFlags::Compute
		, compute_shader
		, pass_parameters
		, thread_group_count
	);

    // scene color를 output texture로 교체
	AddDrawTexturePass(GraphBuilder, InView, output_texture, scene_color_texture);
}

bool FRDGGrayScreenExtension::IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const
{
	if (::IsValid(_world_subsystem))
	{
		// gray_screen이 0이 아닐때만 활성화되도록
		return _world_subsystem->ShouldBeActiveThisFrame();
	}
	return false;
}

void FRDGGrayScreenExtension::SetParameters(float in_gray_screen, float in_gray_gamma)
{
	check(::IsInRenderingThread());
	FScopeLock lock_(&_render_parameter_cs);
	_renderthread_gray_screen = in_gray_screen;
	_renderthread_gray_gamma = in_gray_gamma;
}

void FRDGGrayScreenExtension::GetParameters(float& out_gray_screen, float& out_gray_gamma) const
{
	check(IsInRenderingThread());
	FScopeLock lock_(&_render_parameter_cs);
	out_gray_screen = _renderthread_gray_screen;
	out_gray_gamma = _renderthread_gray_gamma;
}
