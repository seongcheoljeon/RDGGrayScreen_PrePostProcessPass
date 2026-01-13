// Fill out your copyright notice in the Description page of Project Settings.


#include "RDGGrayScreenSubsystem.h"
#include "RDGGrayScreenExtension.h"
#include "RDGGrayScreenLog.h"
#include "SceneViewExtension.h"

void URDGGrayScreenSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	// FWorldSceneViewExtension 생성 (World와 자동 연결)
	_scene_view_extension = FSceneViewExtensions::NewExtension<FRDGGrayScreenExtension>(GetWorld(), this);
	
	UE_LOG(LogRDGGrayScreen, Log, TEXT("RDGGrayScreen: SceneViewExtension Created!"));
}

void URDGGrayScreenSubsystem::Deinitialize()
{
	// extension 정리
	if (_scene_view_extension.IsValid())
	{
		_scene_view_extension.Reset();
		
		UE_LOG(LogRDGGrayScreen, Log, TEXT("RDGGrayScreen: SceneViewExtension destroyed!"));
	}
	
	Super::Deinitialize();
}

FRDGGrayScreenExtension* URDGGrayScreenSubsystem::GetExtension() const
{
	return _scene_view_extension.Get();
}

void URDGGrayScreenSubsystem::SetParameters(float in_gray_screen, float in_gray_gamma)
{
	// game thread 전용 데이터 저장
	FScopeLock lock_(&_gamethread_parameter_cs);
	_gamethread_gray_screen = in_gray_screen;
	_gamethread_gray_gamma = in_gray_gamma;
}

bool URDGGrayScreenSubsystem::ShouldBeActiveThisFrame() const
{
	// game thread에서 안전하게 IsActive 체크
	FScopeLock lock_(&_gamethread_parameter_cs);
	return _gamethread_gray_screen > KINDA_SMALL_NUMBER;
}

void URDGGrayScreenSubsystem::TransferStateToExtension(FRDGGrayScreenExtension* extension) const
{
	if (!extension)
	{
		return;
	}
	
	float local_gray_screen, local_gray_gamma;
	{
		FScopeLock lock_(&_gamethread_parameter_cs);
		local_gray_screen = _gamethread_gray_screen;
		local_gray_gamma = _gamethread_gray_gamma;
	}
	
	if (::IsInRenderingThread())
	{
		// render thread: 직접 전달
		extension->SetParameters(local_gray_screen, local_gray_gamma);
	}
	else
	{
		// game thread: ENQUEUE...로 전달
		ENQUEUE_RENDER_COMMAND(TransferSCiiRDGGrayScreenState)(
			[extension, local_gray_screen, local_gray_gamma](FRHICommandListImmediate& rhi_cmd_list) -> void
			{
				extension->SetParameters(local_gray_screen, local_gray_gamma);
			});
	}
}
