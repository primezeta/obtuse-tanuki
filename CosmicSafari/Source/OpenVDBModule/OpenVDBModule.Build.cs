// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class OpenVDBModule : ModuleRules
{
	public OpenVDBModule(TargetInfo Target)
	{
        Type = ModuleType.CPlusPlus;
        //Type = ModuleType.External;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore" });

        PublicIncludePaths.AddRange(new string[] { Path.Combine(ModulePath, "Public") });
        PrivateIncludePaths.AddRange(new string[] { Path.Combine(ModulePath, "Private") });
        PublicLibraryPaths.AddRange(LibPaths);
        PublicAdditionalLibraries.AddRange(LibNames);
	}

    private string ModulePath
    {
        get
        {
            return Path.Combine(Path.GetDirectoryName(RulesCompiler.GetModuleFilename(this.GetType().Name)));
        }
    }

    private string[] LibPaths
    {
        get
        {
            return new string[] { Path.Combine(ModulePath, "Private") };
        }
    }

    private string[] LibNames
    {
        get
        {
            return new string[] { "libovdb.lib" };
        }
    }
}