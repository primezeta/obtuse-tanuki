// Fill out your copyright notice in the Description page of Project Settings.

#include "OpenVDBModule.h"
#include "ProceduralTerrainMeshComponent.h"

UProceduralTerrainMeshComponent::UProceduralTerrainMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RegionState = EGridState::GRID_STATE_INIT;
	IsTreeReady = false;
	IsGridDirty = true;
	IsQueued = false;
	SectionCount = 0;
	NumReadySections = 0;
	//One state per actual grid state except the final one, and a grid state per voxel type
	NumStatesRemaining = NUM_TOTAL_GRID_STATES;
	for (int32 i = 0; i < FVoxelData::VOXEL_TYPE_COUNT; ++i)
	{
		IsSectionFinished[i] = (int32)false;
	}
	SectionBounds = FBox(EForceInit::ForceInit);
	BodyInstance.SetUseAsyncScene(true);
	bWantsInitializeComponent = true;
	bTickStateAfterFinish = true;
	SetComponentTickEnabled(IsComponentTickEnabled());
}

void UProceduralTerrainMeshComponent::InitializeComponent()
{
	Super::InitializeComponent();
}

void UProceduralTerrainMeshComponent::AddGrid()
{
	check(VdbHandle != nullptr);
	check(RegionState == EGridState::GRID_STATE_INIT);
	MeshID = VdbHandle->AddGrid(MeshID, RegionIndex, VoxelSize, ProcMeshSections);
	RegionState = EGridState::GRID_STATE_READ_TREE;
	NumStatesRemaining--;
}

void UProceduralTerrainMeshComponent::ReadGridTree()
{
	check(VdbHandle != nullptr);
	check(RegionState == EGridState::GRID_STATE_READ_TREE);
	VdbHandle->ReadGridTree(MeshID, StartIndex, EndIndex);
	//Calling GetGridDimensions at this point may result in a SectionBounds that contains the entire grid volume because no voxels may yet be active.
	//The actual bounds of active voxels are valid after calling ExtractIsoSurface in which voxels spanning the isosurface are set to active.
	VdbHandle->GetGridDimensions(MeshID, SectionBounds);
	RegionState = EGridState::GRID_STATE_FILL_VALUES;
	NumStatesRemaining--;
}

void UProceduralTerrainMeshComponent::FillTreeValues()
{
	check(VdbHandle != nullptr);
	check(RegionState == EGridState::GRID_STATE_FILL_VALUES);
	VdbHandle->FillTreePerlin(MeshID, StartIndex, EndIndex);
	RegionState = EGridState::GRID_STATE_EXTRACT_SURFACE;
	NumStatesRemaining--;
}

void UProceduralTerrainMeshComponent::ExtractIsoSurface()
{
	check(VdbHandle != nullptr);
	check(RegionState == EGridState::GRID_STATE_EXTRACT_SURFACE);
	VdbHandle->ExtractIsoSurface(MeshID, SectionMaterialIDs, SectionBounds, StartLocation);
	RegionState = EGridState::GRID_STATE_MESH;
	NumStatesRemaining--;
}

void UProceduralTerrainMeshComponent::MeshGrid()
{
	check(VdbHandle != nullptr);
	check(RegionState == EGridState::GRID_STATE_MESH);
	VdbHandle->MeshGrid(MeshID);
	RegionState = EGridState::GRID_STATE_READY;
	NumStatesRemaining--;
}

bool UProceduralTerrainMeshComponent::FinishSection(int32 SectionIndex, bool isVisible)
{
	bool sectionChangedToFinished = false;
	check(VdbHandle != nullptr);
	check(RegionState == EGridState::GRID_STATE_READY);
	if (IsSectionFinished[SectionIndex] == (int32)false)
	{
		IsSectionFinished[SectionIndex] = (int32)true;
		NumReadySections++;
		check(NumReadySections <= FVoxelData::VOXEL_TYPE_COUNT);
		if (NumReadySections < FVoxelData::VOXEL_TYPE_COUNT)
		{
			SetMeshSectionVisible(SectionIndex, isVisible);
		}
		else if (NumReadySections == FVoxelData::VOXEL_TYPE_COUNT)
		{
			RegionState = EGridState::GRID_STATE_FINISHED;
			NumStatesRemaining--;
			//Calculate collision
			FinishCollison();
			sectionChangedToFinished = true;
			SetComponentTickEnabled(bTickStateAfterFinish);
		}
		NumStatesRemaining--;
	}
	check(NumStatesRemaining > -1);
	return sectionChangedToFinished;
}

void UProceduralTerrainMeshComponent::RemoveGrid()
{
	//TODO
	check(VdbHandle != nullptr);
}