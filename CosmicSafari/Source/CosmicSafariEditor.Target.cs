// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class CosmicSafariEditorTarget : TargetRules
{
	public CosmicSafariEditorTarget(TargetInfo Target)
	{
		Type = TargetType.Editor;
        //The following does not seem to be working...
        //PrivateDependencyModuleNames.AddRange(new string[] { "ProceduralMeshComponent" });
        //PrivateIncludePathModuleNames.AddRange(new string[] { "ProceduralMeshComponent" });
        //Does this need to be called instead of having OpenVDBModule in OutExtraModuleNames?
        //DynamicallyLoadedModuleNames.AddRange(new string[] { "OpenVDBModule" });
    }

    //
    // TargetRules interface.
    //

    public override void SetupBinaries(
		TargetInfo Target,
		ref List<UEBuildBinaryConfiguration> OutBuildBinaryConfigurations,
		ref List<string> OutExtraModuleNames
		)
	{
		OutExtraModuleNames.AddRange(new string[] { "CosmicSafari", "OpenVDBModule" });
	}
}
