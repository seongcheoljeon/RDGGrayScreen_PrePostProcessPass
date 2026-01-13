// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RDGGrayScreenSubsystem.generated.h"

class FRDGGrayScreenExtension;

/**
 * World가 생성될 때, SceneViewExtension을 등록
 * game thread 파라미터를 저장하고 render thread로 전달
 * game thread와 render thread 데이터 완전 분리
 */

UCLASS()
class RDGGRAYSCREEN_PREPOSTPROCESSPASS_API URDGGrayScreenSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	
public:
	// extension 가져오기
	FRDGGrayScreenExtension* GetExtension() const;	
	// game thread에서 파라미터 설정
	void SetParameters(float in_gray_screen, float in_gray_gamma);
	// game thread에서 IsActive 체크 (thread safe)
	bool ShouldBeActiveThisFrame() const;
	// state 전달, BeginRenderViewFamily에서 호출
	void TransferStateToExtension(FRDGGrayScreenExtension* extension) const;
	
private:
	// SceneViewExtension 인스턴스
	TSharedPtr<FRDGGrayScreenExtension, ESPMode::ThreadSafe> _scene_view_extension;
	// game thread 전용 파라미터
	float _gamethread_gray_screen = 0.0f;
	float _gamethread_gray_gamma = 1.0f;
	// critical section - game thread 파라미터
	mutable FCriticalSection _gamethread_parameter_cs;
};
