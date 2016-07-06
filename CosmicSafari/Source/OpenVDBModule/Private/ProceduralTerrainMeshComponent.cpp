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
		IsSectionReady[i] = 0;
	}
	SectionBounds = FBox(EForceInit::ForceInit);
}

void UProceduralTerrainMeshComponent::AddGrid()
{
	check(VdbHandle != nullptr);
	check(RegionState == EGridState::GRID_STATE_INIT);
	RegionState = EGridState::GRID_STATE_READ_TREE;
	MeshID = VdbHandle->AddGrid(MeshID, RegionIndex, VoxelSize, ProcMeshSections);
}

void UProceduralTerrainMeshComponent::ReadGridTree()
{
	check(VdbHandle != nullptr);
	check(RegionState == EGridState::GRID_STATE_READ_TREE);
	VdbHandle->ReadGridTree(MeshID, StartFill, EndFill, SectionMaterialIDs, StartLocation);
	VdbHandle->GetGridDimensions(MeshID, SectionBounds, StartLocation);
	RegionState = EGridState::GRID_STATE_FILL_VALUES;
}

void UProceduralTerrainMeshComponent::FillTreeValues()
{
	check(VdbHandle != nullptr);
	check(RegionState == EGridState::GRID_STATE_FILL_VALUES);
	VdbHandle->FillTreePerlin(MeshID, StartFill, EndFill);
	RegionState = EGridState::GRID_STATE_EXTRACT_SURFACE;
}

void UProceduralTerrainMeshComponent::ExtractIsoSurface()
{
	check(VdbHandle != nullptr);
	check(RegionState == EGridState::GRID_STATE_EXTRACT_SURFACE);
	VdbHandle->ExtractIsoSurface(MeshID, SectionMaterialIDs, SectionBounds, StartLocation);
	RegionState = EGridState::GRID_STATE_MESH;
}

void UProceduralTerrainMeshComponent::MeshGrid()
{
	check(VdbHandle != nullptr);
	check(RegionState == EGridState::GRID_STATE_MESH);
	VdbHandle->MeshGrid(MeshID);
	RegionState = EGridState::GRID_STATE_READY;
}

void UProceduralTerrainMeshComponent::RemoveGrid()
{
	//TODO
	check(VdbHandle != nullptr);
}