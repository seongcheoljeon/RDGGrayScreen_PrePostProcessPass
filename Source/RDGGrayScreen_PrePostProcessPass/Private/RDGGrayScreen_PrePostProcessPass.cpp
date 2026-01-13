// Copyright Epic Games, Inc. All Rights Reserved.

#include "RDGGrayScreen_PrePostProcessPass.h"

#define LOCTEXT_NAMESPACE "FRDGGrayScreen_PrePostProcessPassModule"

void FRDGGrayScreen_PrePostProcessPassModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

void FRDGGrayScreen_PrePostProcessPassModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FRDGGrayScreen_PrePostProcessPassModule, RDGGrayScreen_PrePostProcessPass)