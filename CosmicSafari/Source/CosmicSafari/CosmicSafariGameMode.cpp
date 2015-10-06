// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CosmicSafari.h"
#include "CosmicSafariGameMode.h"
#include "CosmicSafariPawn.h"

ACosmicSafariGameMode::ACosmicSafariGameMode()
{
	// set default pawn class to our flying pawn
	DefaultPawnClass = ACosmicSafariPawn::StaticClass();
}
