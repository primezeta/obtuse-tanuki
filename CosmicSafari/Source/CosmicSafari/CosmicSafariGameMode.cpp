// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CosmicSafari.h"
#include "CosmicSafariGameMode.h"
#include "CosmicSafariPawn.h"

ACosmicSafariGameMode::ACosmicSafariGameMode()
{
	// set default pawn class to our flying pawn
	DefaultPawnClass = ACosmicSafariPawn::StaticClass();
	bIsInitialLocationSet = false;
	bStartPlayersAsSpectators = true;
	Grids = CreateDefaultSubobject<AProceduralTerrain>(TEXT("Grids"));
	check(Grids);
	VdbHandle = CreateDefaultSubobject<UVdbHandle>(TEXT("VDBConfiguration"));
	check(VdbHandle != nullptr);
	VdbHandle->bWantsInitializeComponent = true;
	VdbHandle->bNeverNeedsRenderUpdate = true;
	VdbHandle->bAutoRegister = true;
}

void ACosmicSafariGameMode::PreInitializeComponents()
{
	Grids->VdbHandle = VdbHandle; //TODO
	Super::PreInitializeComponents();
}

void ACosmicSafariGameMode::Tick(float DeltaSeconds)
{
	if (Grids->OldestGridState == EGridState::GRID_STATE_FINISHED && !bIsInitialLocationSet)
	{
		//Wait until all grid regions are finished meshing then define a player start location.
		//TODO: Get player locations in a more robust way (instead of just from the first player controller)
		UWorld * World = GetWorld();
		check(World);
		APlayerController * PlayerController = World->GetFirstPlayerController();
		check(PlayerController);
		UProceduralTerrainMeshComponent * TerrainMeshComponent = Grids->GetTerrainComponent(FIntVector::ZeroValue);
		check(TerrainMeshComponent);
		APawn * SpawnedPawn = SpawnDefaultPawnFor(PlayerController, TerrainMeshComponent->RegionStart);
		check(SpawnedPawn);
		bIsInitialLocationSet = true; //TODO: Setup logic according to game state
	}
}