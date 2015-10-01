// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CosmicSafari.h"
#include "CosmicSafariGameMode.h"
#include "CosmicSafariHUD.h"
#include "CosmicSafariCharacter.h"
#include "OpenVDBModule.h"

ACosmicSafariGameMode::ACosmicSafariGameMode()
	: Super()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/FirstPersonCPP/Blueprints/FirstPersonCharacter"));
	DefaultPawnClass = PlayerPawnClassFinder.Class;

	// use our custom HUD class
	HUDClass = ACosmicSafariHUD::StaticClass();


}
