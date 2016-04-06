// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;
using System;

public class OpenVDBModule : ModuleRules
{
	public OpenVDBModule(TargetInfo Target)
	{
        Platform = Target.Platform;
        Configuration = Target.Configuration;
        Type = ModuleType.CPlusPlus;
        OpenVDBOpenEXRAreShared = true;

        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore" });
        PublicIncludePaths.AddRange(PublicIncludes);
        PrivateIncludePaths.AddRange(PrivateIncludes);
        PublicSystemIncludePaths.AddRange(ThirdPartyIncludes);
        PublicLibraryPaths.AddRange(ThirdPartyLibPaths);
        PublicAdditionalLibraries.AddRange(ThirdPartyLibNames);
        Definitions.AddRange(new string[] { "OPENVDB_DLL", "OPENEXR_DLL", "ZLIB_STATIC" });
        MinFilesUsingPrecompiledHeaderOverride = 1;
        bFasterWithoutUnity = true;
        bUseRTTI = true;
        bEnableExceptions = true;
        //UEBuildConfiguration.bForceEnableExceptions = true;
    }

    private UnrealTargetPlatform Platform;
    private UnrealTargetConfiguration Configuration;
    private bool OpenVDBOpenEXRAreShared;

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
            string path = "Release";
            if(Configuration == UnrealTargetConfiguration.Debug ||
               Configuration == UnrealTargetConfiguration.Development)
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
                Path.Combine(ModuleDirectory, "Public"),
            };
        }
    }

    private string[] PrivateIncludes
    {
        get
        {
            return new string[]
            {
                Path.Combine(ModuleDirectory, "Private"),
            };
        }
    }

    private string ThirdPartyPath
    {
        get
        {
            return Path.Combine(ModuleDirectory, "..", "..", "ThirdParty");
        }
    }

    private string[] ThirdPartyIncludes
    {
        get
        {
            return new string[]
            {
                Path.Combine(ThirdPartyPath, "OpenVDB", "src"),
                Path.Combine(ThirdPartyPath, "OpenVDB", "dependencies", "include"),
                Path.Combine(ThirdPartyPath, "LibNoise"),
                Path.Combine(Path.GetPathRoot(Environment.SystemDirectory), "boost", "boost_1_59_0"),
            };
        }
    }

    private string[] ThirdPartyLibPaths
    {
        get
        {
            string libType = "static";
            if (OpenVDBOpenEXRAreShared)
            {
                libType = "shared";
            }
            return new string[]
            {
                Path.Combine(ModuleDirectory, "..", "..", "Build", PlatformPath, ConfigurationPath),
                Path.Combine(ThirdPartyPath, "OpenVDB", "dependencies", "lib", PlatformPath, ConfigurationPath, libType),
                Path.Combine(Path.GetPathRoot(Environment.SystemDirectory), "boost", "boost_1_59_0", "lib64-msvc-14.0"),
            };
        }
    }

    private string[] ThirdPartyLibNames
    {
        get
        {
            return new string[]
            {
                "openvdb.lib",
                "libnoise.lib",
                "Half.lib",
                //"Iex-2_2.lib",
                //"IexMath-2_2.lib",
                //"IlmThread-2_2.lib",
                //"Imath-2_2.lib",
                //"zlibstat.lib",
                "tbb.lib",
                "tbbmalloc.lib",
            };
        }
    }
}