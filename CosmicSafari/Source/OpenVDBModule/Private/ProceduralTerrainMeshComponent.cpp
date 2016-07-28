// Fill out your copyright notice in the Description page of Project Settings.

#include "OpenVDBModule.h"
#include "ProceduralTerrainMeshComponent.h"

UProceduralTerrainMeshComponent::UProceduralTerrainMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RegionState = EGridState::GRID_STATE_NULL;
	IsTreeReady = false;
	IsGridDirty = true;
	IsQueued = false;
	SectionCount = 0;
	NumReadySections = 0;
	//One state per actual grid state except the final one, and a grid state per voxel type
	NumStatesRemaining = NUM_GRID_STATES;
	SectionBounds = FBox(EForceInit::ForceInit);
	BodyInstance.SetUseAsyncScene(true); //TODO: Need async scene?
	bWantsInitializeComponent = true;
	SetComponentTickEnabled(false);
	RegionStart = CreateDefaultSubobject<APlayerStart>(*FString::Printf(TEXT("%s_%s"), TEXT("PlayerStart"), *GetName()));
	check(RegionStart);
	const float test = PrimaryComponentTick.TickInterval;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.bCanEverTick = true;
	for (auto i = 0; i < NUM_GRID_STATES; ++i)
	{
		GridStateStatus[i] = 0;
	}
}

void UProceduralTerrainMeshComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	//TODO: Try FPlatformTLS::GetCurrentThreadId()
	//if (RegionState == EGridState::GRID_STATE_FINISHED)

	if (GridStateIsDirty(EGridState::GRID_STATE_EMPTY))
	{
		//Read the grid tree data from file, fill with values (if needed), and extract the isosurface
		GridStateStatus[(int32)EGridState::GRID_STATE_EMPTY] = 1;
		NumStatesRemaining--;
		ReadGridTree();
	}

	if (GridStateIsDirty(EGridState::GRID_STATE_DATA_DESYNC) || GridStateIsDirty(EGridState::GRID_STATE_ACTIVE_STATES_DESYNC))
	{
		//Start changes to write to file when either data or active states changed
		GridStateStatus[(int32)EGridState::GRID_STATE_DATA_DESYNC] = 1;
		NumStatesRemaining--;
		GridStateStatus[(int32)EGridState::GRID_STATE_ACTIVE_STATES_DESYNC] = 1;
		NumStatesRemaining--;
		MeshGrid();
	}
	
	if (GridStateIsDirty(EGridState::GRID_STATE_CLEAN))
	{
		//Mark the render state dirty
		GridStateStatus[(int32)EGridState::GRID_STATE_CLEAN] = 1;
		NumStatesRemaining--;
		RenderGrid();
	}
	
	if (GridStateIsDirty(EGridState::GRID_STATE_RENDERED))
	{
		//Calculate collision on the game thread
		GridStateStatus[(int32)EGridState::GRID_STATE_RENDERED] = 1;
		NumStatesRemaining--;
		FinishAllSections();
	}
}

void UProceduralTerrainMeshComponent::InitializeGrid()
{
	if (GridStateIsNull())
	{
		//Initialize the grid
		MeshID = VdbHandle->AddGrid(MeshName, RegionIndex, VoxelSize, ProcMeshSections);
		StartGridState(EGridState::GRID_STATE_EMPTY);
		SetComponentTickEnabled(true);
	}
}

bool UProceduralTerrainMeshComponent::GridIsInState(EGridState GridState)
{
	return GridState != EGridState::GRID_STATE_NULL && RegionState == GridState;
}

bool UProceduralTerrainMeshComponent::GridStateIsNull()
{
	return RegionState == EGridState::GRID_STATE_NULL;
}

bool UProceduralTerrainMeshComponent::GridStateIsDirty(EGridState GridState)
{
	return GridIsInState(GridState) && GridStateStatus[(int32)GridState] == 0;
}

bool UProceduralTerrainMeshComponent::GridStateIsRunning(EGridState GridState)
{
	return GridIsInState(GridState) && GridStateStatus[(int32)GridState] == 1;
}

bool UProceduralTerrainMeshComponent::GridStateIsComplete(EGridState GridState)
{
	return GridIsInState(GridState) && GridStateStatus[(int32)GridState] == 2;
}

void UProceduralTerrainMeshComponent::StartGridState(EGridState GridState)
{
	if (!GridStateIsRunning(GridState))
	{
		RegionState = GridState;
		GridStateStatus[(int32)GridState] = 0;
	}
}

void UProceduralTerrainMeshComponent::ReadGridTree()
{
	check(VdbHandle != nullptr);
	check(RegionState == EGridState::GRID_STATE_EMPTY);
	//Calling GetGridDimensions at this point may result in a SectionBounds that contains the entire grid volume because no voxels may yet be active.
	//The actual bounds of active voxels are valid after calling ExtractIsoSurface in which voxels spanning the isosurface are set to active.
	VdbHandle->RunVoxelDatabaseTask(MeshID + TEXT("ReadGridTree"),
		TFunction<void(void)>([&]() {
			VdbHandle->ReadGridTree(MeshID, StartIndex, EndIndex);
			VdbHandle->FillTreePerlin(MeshID, StartIndex, EndIndex);
			GridStateStatus[(int32)EGridState::GRID_STATE_EMPTY] = 2;
			StartGridState(EGridState::GRID_STATE_ACTIVE_STATES_DESYNC);
			StartGridState(EGridState::GRID_STATE_DATA_DESYNC);
	}));
}

void UProceduralTerrainMeshComponent::MeshGrid()
{
	check(VdbHandle != nullptr);
	check(RegionState == EGridState::GRID_STATE_DATA_DESYNC || RegionState == EGridState::GRID_STATE_ACTIVE_STATES_DESYNC);
	
	VdbHandle->WriteAllGrids();
	GridStateStatus[(int32)EGridState::GRID_STATE_ACTIVE_STATES_DESYNC] = 2;

	VdbHandle->RunVoxelDatabaseTask(MeshID + TEXT("MeshGrid"),
		TFunction<void(void)>([&]() {
			if (RegionState == EGridState::GRID_STATE_DATA_DESYNC)
			{
				VdbHandle->ExtractIsoSurface(MeshID, SectionMaterialIDs, SectionBounds, StartLocation);
			}
			VdbHandle->MeshGrid(MeshID);
			GridStateStatus[(int32)EGridState::GRID_STATE_DATA_DESYNC] = 2;
			StartGridState(EGridState::GRID_STATE_CLEAN);
	}));
}

void UProceduralTerrainMeshComponent::RenderGrid()
{
	check(RegionState == EGridState::GRID_STATE_CLEAN);
	//All sections are meshed so calculate collision
	FinishRender();
	check(RegionStart);
	RegionStart->SetActorLocation(StartLocation);
	GridStateStatus[(int32)EGridState::GRID_STATE_CLEAN] = 2;
	StartGridState(EGridState::GRID_STATE_RENDERED);
}

void UProceduralTerrainMeshComponent::FinishAllSections()
{
	check(RegionState == EGridState::GRID_STATE_RENDERED);

	//Calculate collision
	FinishCollision();

	//Last - set each section visible
	for (auto i = 0; i < FVoxelData::VOXEL_TYPE_COUNT; ++i)
	{
		NumReadySections++;
		check(NumReadySections >= 0 && NumReadySections <= FVoxelData::VOXEL_TYPE_COUNT);
		SetMeshSectionVisible(i, true);
	}

	GridStateStatus[(int32)EGridState::GRID_STATE_RENDERED] = 2;
	StartGridState(EGridState::GRID_STATE_COMPLETE);
	NumStatesRemaining--;
	check(NumStatesRemaining == 0);
}