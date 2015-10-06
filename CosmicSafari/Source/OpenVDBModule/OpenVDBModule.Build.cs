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
        //Type = ModuleType.External;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore" });
        //System.Console.WriteLine(string.Format("ModulePath {0}:\nThirdPartyPath: {1}\nThirdPublicIncludes: {2}\nPrivateIncludes: {3}\nPublicLibraryPaths: {4}\nPublicAdditionalLibraries: {5}\nPlatformPath: {6}\nConfigurationPath: {7}", ModulePath, ThirdPartyPath, PublicIncludes, PrivateIncludes, LibPaths, LibNames, PlatformPath, ConfigurationPath));
        PublicIncludePaths.AddRange(PublicIncludes);
        PrivateIncludePaths.AddRange(PrivateIncludes);
        PublicLibraryPaths.AddRange(LibPaths);
        PublicAdditionalLibraries.AddRange(LibNames);

        //AddThirdPartyPrivateStaticDependencies(Target, );
        Definitions.AddRange(new string[] { "OPENVDB_OPENEXR_STATICLIB", "OPENVDB_STATICLIB" });
        //Definitions.AddRange(new string[] { "OPENVDB_DLL" });        
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
                path = "Debug";
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
                Path.Combine(ThirdPartyPath, "libovdb")
            };
        }
    }

    private string[] PrivateIncludes
    {
        get
        {
            return new string[]
            {
                Path.Combine(ModulePath, "Private")
            };
        }
    }

    private string[] LibPaths
    {
        get
        {
            return new string[]
            {
                Path.Combine(ThirdPartyPath, "Build", PlatformPath, ConfigurationPath),
                Path.Combine(ThirdPartyPath, "OpenVDB", "dependencies", "lib", PlatformPath),
                Path.Combine(ThirdPartyPath, "boost")
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
                "zlibstat.lib",
                "Half.lib"
            };
        }
    }
}