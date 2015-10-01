// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class CosmicSafari : ModuleRules
{
	public CosmicSafari(TargetInfo Target)
	{
        //PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "OpenVDBModule" });
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "OpenVDBModule" });
        //AddThirdPartyPrivateDynamicDependencies(Target, "OpenVDBModule");
    }

    private string ModulePath
    {
        get
        {
            return Path.Combine(Path.GetDirectoryName(RulesCompiler.GetModuleFilename(this.GetType().Name)));
        }
    }
}
