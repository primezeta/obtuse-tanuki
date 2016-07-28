// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CosmicSafari.h"
#include "CosmicSafariGameMode.h"
#include "CosmicSafariPawn.h"

ACosmicSafariGameMode::ACosmicSafariGameMode()
{
	// set default pawn class to our flying pawn
	DefaultPawnClass = ACosmicSafariPawn::StaticClass();
	bStartPlayersAsSpectators = true;
}

void ACosmicSafariGameMode::Tick(float DeltaSeconds)
{
	////TODO: Register a delegate for when number states remaining transitions to 0?
	//if (Grids->NumberMeshingStatesRemaining == 0 && !bIsInitialLocationSet)
	//{
	//	//Wait until all grid regions are finished meshing then define a player start location.
	//	//TODO: Get player locations in a more robust way (instead of just from the first player controller)
	//	UWorld * World = GetWorld();
	//	check(World);
	//	APlayerController * PlayerController = World->GetFirstPlayerController();
	//	check(PlayerController);
	//	//Get the "start region" (i.e. the terrain mesh component with index 0,0,0)
	//	UProceduralTerrainMeshComponent * TerrainMeshComponent = Grids->GetTerrainComponent(FIntVector(0, 0, 0));
	//	if (TerrainMeshComponent)
	//	{
	//		APawn * SpawnedPawn = SpawnDefaultPawnFor(PlayerController, TerrainMeshComponent->RegionStart);
	//		check(SpawnedPawn);
	//		bIsInitialLocationSet = true; //TODO: Setup logic according to game state
	//	}
	//}
}