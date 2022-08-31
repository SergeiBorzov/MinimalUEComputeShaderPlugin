// Copyright Epic Games, Inc. All Rights Reserved.

#include "TestPlugin.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"


void FTestPluginModule::StartupModule()
{
	FString ShaderDirectory = FPaths::Combine(FPaths::ProjectDir(), TEXT("Plugins/TestPlugin/Shaders"));
	AddShaderSourceDirectoryMapping("/CustomShaders", ShaderDirectory);
}

void FTestPluginModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}
	
IMPLEMENT_MODULE(FTestPluginModule, TestPlugin)