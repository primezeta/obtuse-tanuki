// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;
using System;

public class UEOpenVdb : ModuleRules
{
    private UnrealTargetPlatform Platform;
    private UnrealTargetConfiguration Configuration;
    
    public UEOpenVdb(TargetInfo Target)
    {
        Platform = Target.Platform;
        Configuration = Target.Configuration;
        Type = ModuleType.External;
        OptimizeCode = CodeOptimization.InNonDebugBuilds;
        bUseRTTI = true;
        bEnableExceptions = true;
        PublicIncludePaths.AddRange(PublicIncludes);
        PrivateIncludePaths.AddRange(PrivateIncludes);
        PublicSystemIncludePaths.AddRange(ThirdPartyIncludes);
        PublicLibraryPaths.AddRange(ThirdPartyLibPaths);
        PublicAdditionalLibraries.AddRange(ThirdPartyLibNames);
        PublicDependencyModuleNames.AddRange(new string[] {"UEOpenExr", "IntelTBB", "zlib"});
        Definitions.AddRange(new string[] { "ZLIB_STATIC", "OPENVDB_STATICLIB", "OPENVDB_OPENEXR_STATICLIB" });
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
                Path.Combine(ModuleDirectory, "src"),
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
            return Path.Combine(ModuleDirectory, "..");
        }
    }

    private string[] ThirdPartyIncludes
    {
        get
        {
            return new string[]
            {
                Path.Combine(Path.GetPathRoot(Environment.SystemDirectory), "boost", "boost_1_59_0"),
            };
        }
    }

    private string[] ThirdPartyLibPaths
    {
        get
        {
            return new string[]
            {
                //if (WindowsPlatform.Compiler == WindowsCompiler.VisualStudio2015)
                Path.Combine(ModuleDirectory, "Binaries", "VS2015", PlatformPath, ConfigurationPath),
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
            };
        }
    }
}