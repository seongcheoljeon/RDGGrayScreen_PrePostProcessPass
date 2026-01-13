// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SceneViewExtension.h"

class URDGGrayScreenSubsystem;

/**
 * FworldSceneViewExtension 상속:
 * - world 생명주기와 자동 연결
 * - world 파괴 시, 자동 정리
 * game thread와 render thread 데이터 분리
 */

class FRDGGrayScreenExtension : public FWorldSceneViewExtension
{
public:
	FRDGGrayScreenExtension(const FAutoRegister& auto_register, UWorld* in_world, URDGGrayScreenSubsystem* in_subsystem);;
	virtual ~FRDGGrayScreenExtension() = default;
	
public:
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override {};
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {};
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;
	virtual void PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& InView,
		const FPostProcessingInputs& Inputs) override;

protected:
	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override;
	
public:
	// render thread: 파라미터 설정
	void SetParameters(float in_gray_screen, float in_gray_gamma);
	// render thread: 파라미터 읽기
	void GetParameters(float& out_gray_screen, float& out_gray_gamma) const;
	
private:
	// 0: original, 1: full grayscale screen
	float _renderthread_gray_screen = 0.0;
	// 0: black, 1: normal
	float _renderthread_gray_gamma = 1.0;
	// critical section
	mutable FCriticalSection _render_parameter_cs;
	// subsystem reference (IsActiveThisFrame 위임용)
	URDGGrayScreenSubsystem* _world_subsystem;
};
