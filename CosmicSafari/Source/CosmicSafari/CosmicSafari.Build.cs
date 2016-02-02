// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CosmicSafari : ModuleRules
{
	public CosmicSafari(TargetInfo Target)
	{
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "OpenVDBModule", "ProceduralMeshComponent" });
        //The other approach with dynamic loading would require use of LoadModule
        //DynamicallyLoadedModuleNames.AddRange(new string[] { "ProceduralMeshComponent" });        
        PrivateIncludePathModuleNames.AddRange(new string[] { "ProceduralMeshComponent" });

        MinFilesUsingPrecompiledHeaderOverride = 1;
        bFasterWithoutUnity = true;
	}

    public virtual void SetupGlobalEnvironment(
        TargetInfo Target,
        ref LinkEnvironmentConfiguration OutLinkEnvironmentConfiguration,
        ref CPPEnvironmentConfiguration OutCPPEnvironmentConfiguration
        )
    {
        //OutCPPEnvironmentConfiguration.Definitions.Add("OPENVDBMODULE_IMPORT");
    }
}
