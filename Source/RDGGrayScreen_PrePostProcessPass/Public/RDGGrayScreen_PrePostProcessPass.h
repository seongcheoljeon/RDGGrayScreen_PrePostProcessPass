// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

/*
 * SceneViewExtension은 SCii_RDGGrayScreenSubsystem에서 생성됨
*/

class FRDGGrayScreen_PrePostProcessPassModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
