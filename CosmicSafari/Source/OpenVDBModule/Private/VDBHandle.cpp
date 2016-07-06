// Fill out your copyright notice in the Description page of Project Settings.

#include "OpenVDBModule.h"

UVdbHandle::UVdbHandle(const FObjectInitializer& ObjectInitializer)
{
	MeshMethod = EMeshType::MESH_TYPE_CUBES;
	FilePath = "";
	EnableDelayLoad = true;
	EnableGridStats = true;
	WorldName = "";
	PerlinSeed = 0;
	PerlinFrequency = 4.0f;
	PerlinLacunarity = 0.49f;
	PerlinPersistence = 2.01f;
	PerlinOctaveCount = 9;
	bWantsInitializeComponent = true;
}

void UVdbHandle::InitializeComponent()
{
	Super::InitializeComponent();
	isRegistered = FOpenVDBModule::IsAvailable() && !FilePath.IsEmpty() && FOpenVDBModule::RegisterVdb(this);
}

void UVdbHandle::BeginDestroy()
{
	if (isRegistered)
	{
		FOpenVDBModule::UnregisterVdb(this);
	}
	Super::BeginDestroy();
}

FString UVdbHandle::AddGrid(const FString &gridName, const FIntVector &regionIndex, const FVector &voxelSize, TArray<FProcMeshSection> &sectionBuffers)
{
	FString gridID;
	if (isRegistered)
	{
		gridID = FOpenVDBModule::AddGrid(this, gridName, regionIndex, voxelSize, sectionBuffers);
	}
	return gridID;
}

TArray<FString> UVdbHandle::GetAllGridIDs()
{
	TArray<FString> GridIDs;
	if (isRegistered)
	{
		GridIDs = FOpenVDBModule::GetAllGridIDs(this);
	}
	return GridIDs;
}

void UVdbHandle::RemoveGrid(const FString &gridID)
{
	if (isRegistered)
	{
		FOpenVDBModule::RemoveGrid(this, gridID); //TODO: Remove mesh component
	}
}

void UVdbHandle::SetRegionScale(const FIntVector &regionScale)
{
	if (isRegistered)
	{
		check(regionScale.X > 0 && regionScale.Y > 0 && regionScale.Z > 0);
		FOpenVDBModule::SetRegionScale(this, regionScale);
	}
}

void UVdbHandle::ReadGridTree(const FString &gridID, TArray<TEnumAsByte<EVoxelType>> &sectionMaterialIDs, FVector &initialLocation)
{
	if (isRegistered)
	{
		FIntVector StartFill; //dummy value (not used)
		FIntVector EndFill; //dummy value (not used)
		FOpenVDBModule::ReadGridTree(this, gridID, MeshMethod, StartFill, EndFill, sectionMaterialIDs, initialLocation);
	}
}

void UVdbHandle::GetVoxelCoord(const FString &gridID, const FVector &worldLocation, FIntVector &outVoxelCoord)
{
	if (isRegistered)
	{
		FOpenVDBModule::GetVoxelCoord(this, gridID, worldLocation, outVoxelCoord);
	}
}

void UVdbHandle::MeshGrid(const FString &gridID)
{
	if (isRegistered)
	{
		FOpenVDBModule::MeshGrid(this, gridID, MeshMethod);
	}
}

void UVdbHandle::GetGridDimensions(const FString &gridID, FBox &worldBounds, FVector &firstActive)
{
	if (isRegistered)
	{
		FOpenVDBModule::GetGridDimensions(this, gridID, worldBounds, firstActive);
	}
}

FIntVector UVdbHandle::GetRegionIndex(const FVector &worldLocation)
{
	FIntVector regionIndex;
	if (isRegistered)
	{
		regionIndex = FOpenVDBModule::GetRegionIndex(this, worldLocation);
	}
	return regionIndex;
}

void UVdbHandle::WriteAllGrids()
{
	if (isRegistered)
	{
		FOpenVDBModule::WriteAllGrids(this);
	}
}