// Fill out your copyright notice in the Description page of Project Settings.

#include "OpenVDBModule.h"
#include "ProceduralTerrainMeshComponent.h"

UProceduralTerrainMeshComponent::UProceduralTerrainMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	IsGridDirty = true;
	IsGridReady = false;
	for (int32 i = 0; i < FVoxelData::VOXEL_TYPE_COUNT; ++i)
	{
		IsSectionReady[i] = 0;
	}
}

void UProceduralTerrainMeshComponent::InitMeshComponent(UVdbHandle * vdbHandle)
{
	check(VdbHandle == nullptr);
	check(vdbHandle != nullptr);
	VdbHandle = vdbHandle;
	SectionBounds = FBox(EForceInit::ForceInit);
}

FString UProceduralTerrainMeshComponent::AddGrid(const FIntVector &regionIndex, const FVector &voxelSize)
{
	check(VdbHandle != nullptr);
	return VdbHandle->AddGrid(MeshID, regionIndex, voxelSize, ProcMeshSections);
}

void UProceduralTerrainMeshComponent::RemoveGrid()
{
	//TODO
	check(VdbHandle != nullptr);
}

void UProceduralTerrainMeshComponent::ReadGridTree(TArray<TEnumAsByte<EVoxelType>> &sectionMaterialIDs)
{
	check(VdbHandle != nullptr);
	VdbHandle->ReadGridTree(MeshID, sectionMaterialIDs, StartLocation);
	VdbHandle->GetGridDimensions(MeshID, SectionBounds, StartLocation);
}

void UProceduralTerrainMeshComponent::MeshGrid()
{
	check(VdbHandle != nullptr);
	VdbHandle->MeshGrid(MeshID);
}