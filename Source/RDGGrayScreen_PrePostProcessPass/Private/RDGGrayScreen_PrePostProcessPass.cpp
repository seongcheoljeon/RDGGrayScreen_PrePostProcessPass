// Copyright Epic Games, Inc. All Rights Reserved.

#include "RDGGrayScreen_PrePostProcessPass.h"
#include "Interfaces/IPluginManager.h"
#include "RDGGrayScreenLog.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "FRDGGrayScreen_PrePostProcessPassModule"


void FRDGGrayScreen_PrePostProcessPassModule::StartupModule()
{
	TSharedPtr<IPlugin> plugin = IPluginManager::Get().FindPlugin(TEXT("RDGGrayScreen_PrePostProcessPass"));
	if (plugin.IsValid())
	{
		FString plugin_shader_dir = FPaths::Combine(plugin->GetBaseDir(), TEXT("Shaders"));
		AddShaderSourceDirectoryMapping(TEXT("/Plugin/RDGGrayScreen_PrePostProcessPass"), plugin_shader_dir);

		UE_LOG(LogRDGGrayScreen, Log, TEXT("RDGGrayScreen_PrePostProcessPass plugin shader directory mapped: %s"), *plugin_shader_dir);
	}
	else
	{
		UE_LOG(LogRDGGrayScreen, Error, TEXT("Failed to find RDGGrayScreen_PrePostProcessPass plugin"));
	}
}

void FRDGGrayScreen_PrePostProcessPassModule::ShutdownModule()
{
	UE_LOG(LogRDGGrayScreen, Log, TEXT("RDGGrayScreen_PrePostProcessPass plugin shutdown"));
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FRDGGrayScreen_PrePostProcessPassModule, RDGGrayScreen_PrePostProcessPass)