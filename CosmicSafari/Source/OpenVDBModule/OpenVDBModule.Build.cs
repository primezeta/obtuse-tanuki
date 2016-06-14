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

        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "UEOpenVdb", "LibNoise", "FastNoise", "ProceduralMeshComponent" });
        PrivateIncludePathModuleNames.AddRange(new string[] { "ProceduralMeshComponent" });
        PublicIncludePaths.AddRange(PublicIncludes);
        PrivateIncludePaths.AddRange(PrivateIncludes);
        PublicSystemIncludePaths.AddRange(ThirdPartyIncludes);
        PublicLibraryPaths.AddRange(ThirdPartyLibPaths);
        PublicAdditionalLibraries.AddRange(ThirdPartyLibNames);
        //Definitions.AddRange(new string[] { "ZLIB_STATIC", "OPENVDB_STATICLIB" });
        MinFilesUsingPrecompiledHeaderOverride = 1;
        bFasterWithoutUnity = true;
        bUseRTTI = true;
        bEnableExceptions = true;
        OptimizeCode = CodeOptimization.InNonDebugBuilds;
    }

    private UnrealTargetPlatform Platform;
    private UnrealTargetConfiguration Configuration;

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

    private string[] ThirdPartyIncludes
    {
        get
        {
            return new string[]
            {
            };
        }
    }

    private string[] ThirdPartyLibPaths
    {
        get
        {
            return new string[]
            {
            };
        }
    }

    private string[] ThirdPartyLibNames
    {
        get
        {
            return new string[]
            {
            };
        }
    }
}