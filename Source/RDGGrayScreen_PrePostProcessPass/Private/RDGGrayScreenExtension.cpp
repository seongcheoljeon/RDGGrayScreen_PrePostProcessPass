// Fill out your copyright notice in the Description page of Project Settings.


#include "RDGGrayScreenExtension.h"

#include "RDGGrayScreenLog.h"
#include "RDGGrayScreenSubsystem.h"


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

// #TODO: 여기 작업해야 함.
void FRDGGrayScreenExtension::PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& InView,
	const FPostProcessingInputs& Inputs)
{
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
