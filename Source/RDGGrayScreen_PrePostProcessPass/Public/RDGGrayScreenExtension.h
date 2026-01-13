// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SceneViewExtension.h"

/**
 * 
 */
class FRDGGrayScreenExtension : public FWorldSceneViewExtension
{
public:
	FRDGGrayScreenExtension();
	virtual ~FRDGGrayScreenExtension();
	
public:
	void SetParameters(float in_gray_screen, float in_gray_gamma);
};
