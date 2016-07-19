// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "GameFramework/GameMode.h"
#include "OpenVDBModule.h"
#include "ProceduralTerrain.h"
#include "CosmicSafariGameMode.generated.h"

UCLASS(minimalapi)
class ACosmicSafariGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	ACosmicSafariGameMode();

	UPROPERTY()
		AProceduralTerrain * Grids;
	UPROPERTY(BlueprintReadOnly, Category = "Voxel database configuration", Meta = (DisplayName = "Voxel Database", ToolTip = "Configure VBD properties"))
		UVdbHandle * VdbHandle;

	virtual void PreInitializeComponents() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	bool bIsInitialLocationSet;
};