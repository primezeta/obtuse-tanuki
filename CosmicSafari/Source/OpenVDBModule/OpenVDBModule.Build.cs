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

        PublicIncludePaths.AddRange(new string[] { Path.Combine(ModulePath, "Public"), Path.Combine(ModulePath, "..", "..", "ThirdParty", "libovdb") });
        PrivateIncludePaths.AddRange(new string[] { ModulePath, Path.Combine(ModulePath, "Private") });
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
            return new string[]
            {
                Path.Combine(ModulePath, "..", "..", "ThirdParty", "Build", "x64", "Debug"),
                Path.Combine(ModulePath, "..", "..", "ThirdParty", "OpenVDB", "dependencies", "lib", "x64")
            };
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