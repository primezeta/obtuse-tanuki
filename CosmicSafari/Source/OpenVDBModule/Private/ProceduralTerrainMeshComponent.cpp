// Fill out your copyright notice in the Description page of Project Settings.

#include "OpenVDBModule.h"
#include "ProceduralTerrainMeshComponent.h"

void UProceduralTerrainMeshComponent::InitMeshComponent(UVdbHandle * vdbHandle)
{
	check(VdbHandle == nullptr);
	check(vdbHandle != nullptr);
	VdbHandle = vdbHandle;
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
	VdbHandle->ReadGridTree(MeshID, sectionMaterialIDs);
}

void UProceduralTerrainMeshComponent::MeshGrid()
{
	check(VdbHandle != nullptr);
	VdbHandle->MeshGrid(MeshID);
}

FBox UProceduralTerrainMeshComponent::GetGridDimensions()
{
	check(VdbHandle != nullptr);
	FBox sectionBounds;
	VdbHandle->GetGridDimensions(MeshID, sectionBounds, StartLocation);
	return sectionBounds;
}