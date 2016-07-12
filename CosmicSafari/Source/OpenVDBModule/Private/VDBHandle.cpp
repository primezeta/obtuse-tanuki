// Fill out your copyright notice in the Description page of Project Settings.

#include "OpenVDBModule.h"

UVdbHandle::UVdbHandle(const FObjectInitializer& ObjectInitializer)
	: VdbName(GetFName().ToString())
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
	ThreadedGridOps = true;
	bWantsInitializeComponent = true;
}

void UVdbHandle::InitializeComponent()
{
	Super::InitializeComponent();
	isRegistered = FOpenVDBModule::IsAvailable() && !FilePath.IsEmpty() && FOpenVDBModule::RegisterVdb(VdbName, FilePath, EnableGridStats, EnableDelayLoad);
}

void UVdbHandle::BeginDestroy()
{
	if (isRegistered)
	{
		FOpenVDBModule::UnregisterVdb(VdbName);
	}
	Super::BeginDestroy();
}

FString UVdbHandle::AddGrid(const FString &gridName, const FIntVector &regionIndex, const FVector &voxelSize, TArray<FProcMeshSection> &sectionBuffers)
{
	FString gridID;
	if (isRegistered)
	{
		gridID = FOpenVDBModule::AddGrid(VdbName, gridName, regionIndex, voxelSize, sectionBuffers);
	}
	return gridID;
}

void UVdbHandle::ReadGridTree(const FString &gridID, FIntVector &startIndex, FIntVector &endIndex)
{
	if (isRegistered)
	{
		FOpenVDBModule::ReadGridTree(VdbName, gridID, startIndex, endIndex);
	}
}

void UVdbHandle::FillTreePerlin(const FString &gridID, FIntVector &startFill, FIntVector &endFill)
{
	if (isRegistered)
	{
		FOpenVDBModule::FillTreePerlin(VdbName, gridID, startFill, endFill, PerlinSeed, PerlinFrequency, PerlinLacunarity, PerlinPersistence, PerlinOctaveCount, ThreadedGridOps);
	}
}

void UVdbHandle::ExtractIsoSurface(const FString &gridID, TArray<TEnumAsByte<EVoxelType>> &sectionMaterialIDs, FBox &gridDimensions, FVector &initialLocation)
{
	if (isRegistered)
	{
		FOpenVDBModule::ExtractIsoSurface(VdbName, gridID, MeshMethod, sectionMaterialIDs, gridDimensions, initialLocation, ThreadedGridOps);
	}
}

void UVdbHandle::MeshGrid(const FString &gridID)
{
	if (isRegistered)
	{
		FOpenVDBModule::MeshGrid(VdbName, gridID, MeshMethod);
	}
}

TArray<FString> UVdbHandle::GetAllGridIDs()
{
	TArray<FString> GridIDs;
	if (isRegistered)
	{
		GridIDs = FOpenVDBModule::GetAllGridIDs(VdbName);
	}
	return GridIDs;
}

void UVdbHandle::RemoveGrid(const FString &gridID)
{
	if (isRegistered)
	{
		FOpenVDBModule::RemoveGrid(VdbName, gridID); //TODO: Remove mesh component
	}
}

void UVdbHandle::SetRegionScale(const FIntVector &regionScale)
{
	if (isRegistered)
	{
		check(regionScale.X > 0 && regionScale.Y > 0 && regionScale.Z > 0);
		FOpenVDBModule::SetRegionScale(VdbName, regionScale);
	}
}

void UVdbHandle::GetVoxelCoord(const FString &gridID, const FVector &worldLocation, FIntVector &outVoxelCoord)
{
	if (isRegistered)
	{
		FOpenVDBModule::GetVoxelCoord(VdbName, gridID, worldLocation, outVoxelCoord);
	}
}

bool UVdbHandle::GetGridDimensions(const FString &gridID, FVector &startLocation)
{
	bool hasActiveVoxels = false;
	if (isRegistered)
	{
		hasActiveVoxels = FOpenVDBModule::GetGridDimensions(VdbName, gridID, startLocation);
	}
	return hasActiveVoxels;
}

bool UVdbHandle::GetGridDimensions(const FString &gridID, FBox &worldBounds)
{
	bool hasActiveVoxels = false;
	if (isRegistered)
	{
		hasActiveVoxels = FOpenVDBModule::GetGridDimensions(VdbName, gridID, worldBounds);
	}
	return hasActiveVoxels;
}

bool UVdbHandle::GetGridDimensions(const FString &gridID, FBox &worldBounds, FVector &startLocation)
{
	bool hasActiveVoxels = false;
	if (isRegistered)
	{
		hasActiveVoxels = FOpenVDBModule::GetGridDimensions(VdbName, gridID, worldBounds, startLocation);
	}
	return hasActiveVoxels;
}

FIntVector UVdbHandle::GetRegionIndex(const FVector &worldLocation)
{
	FIntVector regionIndex;
	if (isRegistered)
	{
		regionIndex = FOpenVDBModule::GetRegionIndex(VdbName, worldLocation);
	}
	return regionIndex;
}

void UVdbHandle::WriteAllGrids()
{
	if (isRegistered)
	{
		FOpenVDBModule::WriteAllGrids(VdbName);
	}
}