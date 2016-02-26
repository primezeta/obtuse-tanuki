// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;
using System;

public class OpenVDBModule : ModuleRules
{
	public OpenVDBModule(TargetInfo Target)
	{
        Platform = Target.Platform;
        Config = Target.Configuration;

        Type = ModuleType.CPlusPlus;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore" });
        PublicIncludePaths.AddRange(PublicIncludes);
        PrivateIncludePaths.AddRange(PrivateIncludes);
        PublicLibraryPaths.AddRange(LibPaths);
        PublicAdditionalLibraries.AddRange(LibNames);
        Definitions.AddRange(new string[] { "LIB_OVDB_DLL" });

        MinFilesUsingPrecompiledHeaderOverride = 1;
        bFasterWithoutUnity = true;
	}

    private UnrealTargetPlatform Platform;
    private UnrealTargetConfiguration Config;

    private string ModulePath
    {
        get
        {
            return Path.Combine(Path.GetDirectoryName(RulesCompiler.GetModuleFilename(this.GetType().Name)));
        }
    }

    private string ThirdPartyPath
    {
        get
        {
            return Path.Combine(ModulePath, "..", "..", "ThirdParty");
        }
    }

    private string PlatformPath
    {
        get
        {
            string path = "Win32";
            if (Platform == UnrealTargetPlatform.Win64)
            {
                path = "x64";
            }
            return path;
        }
    }

    private string ConfigurationPath
    {
        get
        {
            //UnrealTargetConfiguration doesn't have a field for Release, so I guess default to Release
            string path = "Release";
            if (Config == UnrealTargetConfiguration.Debug ||
                Config == UnrealTargetConfiguration.DebugGame ||
                Config == UnrealTargetConfiguration.Development)
            {
                //Note that we will still link to Release directories because the UE4 build system
                //is made to mainly build release builds. The OpenVDB stuff can still be built with
                //the debug database to allow debugging.
                path = "Debug";
                //path = "Release";
            }
            return path;
        }
    }

    private string[] PublicIncludes
    {
        get
        {
            return new string[]
            {
                Path.Combine(ModulePath, "Public"),
                Path.Combine(ThirdPartyPath, "libovdb"),
            };
        }
    }

    private string[] PrivateIncludes
    {
        get
        {
            return new string[]
            {
                Path.Combine(ModulePath, "Private"),
            };
        }
    }

    private string[] LibPaths
    {
        get
        {
            return new string[]
            {
                Path.Combine(ModulePath, "..", "..", "Build", PlatformPath, ConfigurationPath),
            };
        }
    }

    private string[] LibNames
    {
        get
        {
            return new string[]
            {
                "libovdb.lib",
            };
        }
    }
}