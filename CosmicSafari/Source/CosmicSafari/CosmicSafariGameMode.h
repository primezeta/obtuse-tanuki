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
		UProceduralTerrain * Grids;

	virtual void Tick(float DeltaSeconds) override;

private:
	bool bIsInitialLocationSet;
};