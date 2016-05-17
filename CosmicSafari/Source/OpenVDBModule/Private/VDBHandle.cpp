// Fill out your copyright notice in the Description page of Project Settings.

#include "OpenVDBModule.h"

UVdbHandle::UVdbHandle(const FObjectInitializer& ObjectInitializer)
{
	FilePath = "";
	EnableDelayLoad = true;
	EnableGridStats = true;
	WorldName = "";
	PerlinSeed = 0;
	PerlinFrequency = 2.01f;
	PerlinLacunarity = 2.0f;
	PerlinPersistence = 0.5f;
	PerlinOctaveCount = 8;
	//RegisterComponent();
	bWantsInitializeComponent = true;
}

void UVdbHandle::InitializeComponent()
{
	Super::InitializeComponent();
	if (FOpenVDBModule::IsAvailable() && !FilePath.IsEmpty())
	{
		FOpenVDBModule::RegisterVdb(this);
	}
}

void UVdbHandle::BeginDestroy()
{
	if (FOpenVDBModule::IsAvailable() && !FilePath.IsEmpty())
	{
		FOpenVDBModule::UnregisterVdb(this);
	}
	Super::BeginDestroy();
}

FString UVdbHandle::AddGrid(const FString &gridName, const FIntVector &regionIndex, const FVector &voxelSize)
{
	FString gridID;
	if (FOpenVDBModule::IsAvailable())
	{
		gridID = FOpenVDBModule::AddGrid(this, gridName, regionIndex, voxelSize);
	}
	return gridID;
}

TArray<FString> UVdbHandle::GetAllGridIDs()
{
	TArray<FString> GridIDs;
	if (FOpenVDBModule::IsAvailable())
	{
		GridIDs = FOpenVDBModule::GetAllGridIDs(this);
	}
	return GridIDs;
}

void UVdbHandle::RemoveGrid(const FString &gridID)
{
	if (FOpenVDBModule::IsAvailable())
	{
		FOpenVDBModule::RemoveGrid(this, gridID);
	}
}

void UVdbHandle::SetRegionScale(const FIntVector &regionScale)
{
	if (FOpenVDBModule::IsAvailable())
	{
		FOpenVDBModule::SetRegionScale(this, regionScale);
	}
}

void UVdbHandle::ReadGridTree(const FString &gridID, FIntVector &startFill, FIntVector &endFill)
{
	if (FOpenVDBModule::IsAvailable())
	{
		FOpenVDBModule::ReadGridTree(this, gridID, MeshMethod, startFill, endFill);
	}
}

void UVdbHandle::GetVoxelCoord(const FString &gridID, const FVector &worldLocation, FIntVector &outVoxelCoord)
{
	if (FOpenVDBModule::IsAvailable())
	{
		FOpenVDBModule::GetVoxelCoord(this, gridID, worldLocation, outVoxelCoord);
	}
}

void UVdbHandle::MeshGrid(const FString &gridID,
						  TSharedPtr<TArray<FVector>> &OutVertexBufferPtr,
						  TSharedPtr<TArray<int32>> &OutPolygonBufferPtr,
						  TSharedPtr<TArray<FVector>> &OutNormalBufferPtr,
						  TSharedPtr<TArray<FVector2D>> &OutUVMapBufferPtr,
						  TSharedPtr<TArray<FColor>> &OutVertexColorsBufferPtr,
						  TSharedPtr<TArray<FProcMeshTangent>> &OutTangentsBufferPtr,
	                      FVector &worldStart,
	                      FVector &worldEnd,
	                      FVector &firstActive)
{
	if (FOpenVDBModule::IsAvailable())
	{
		FOpenVDBModule::MeshGrid(this,
			                     gridID,
			                     MeshMethod,
			                     OutVertexBufferPtr,
			                     OutPolygonBufferPtr,
			                     OutNormalBufferPtr,
			                     OutUVMapBufferPtr,
			                     OutVertexColorsBufferPtr,
			                     OutTangentsBufferPtr,
			                     worldStart,
			                     worldEnd,
			                     firstActive);
	}
}

FIntVector UVdbHandle::GetRegionIndex(const FVector &worldLocation)
{
	openvdb::Vec3d regionIndex;
	if (FOpenVDBModule::IsAvailable())
	{
		FOpenVDBModule::GetRegionIndex(this, worldLocation);
	}
	return FIntVector((int)regionIndex.x(), (int)regionIndex.y(), (int)regionIndex.z());
}

void UVdbHandle::WriteAllGrids()
{
	if (FOpenVDBModule::IsAvailable())
	{
		FOpenVDBModule::WriteAllGrids(this);
	}
}