// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;
using System;

public class LibNoise : ModuleRules
{
    private UnrealTargetPlatform Platform;
    private UnrealTargetConfiguration Configuration;
    
    public LibNoise(TargetInfo Target)
    {
        Platform = Target.Platform;
        Configuration = Target.Configuration;
        Type = ModuleType.External;
        OptimizeCode = CodeOptimization.InNonDebugBuilds;
        PublicIncludePaths.AddRange(PublicIncludes);
        PrivateIncludePaths.AddRange(PrivateIncludes);
        PublicSystemIncludePaths.AddRange(ThirdPartyIncludes);
        PublicLibraryPaths.AddRange(ThirdPartyLibPaths);
        PublicAdditionalLibraries.AddRange(ThirdPartyLibNames);
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
                Path.Combine(ModuleDirectory),
            };
        }
    }

    private string[] PrivateIncludes
    {
        get
        {
            return new string[]
            {
            };
        }
    }

    private string ThirdPartyPath
    {
        get
        {
            return Path.Combine(ModuleDirectory, "..", "..");
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
                Path.Combine(ModuleDirectory, "..", "..", "Binaries", "VS2015", PlatformPath, ConfigurationPath),
            };
        }
    }

    private string[] ThirdPartyLibNames
    {
        get
        {
            return new string[]
            {
                "libnoise.lib",
            };
        }
    }    
}