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
		FOpenVDBModule::RemoveGrid(this, gridID); //TODO: Remove mesh component
	}
}

void UVdbHandle::SetRegionScale(const FIntVector &regionScale)
{
	if (FOpenVDBModule::IsAvailable())
	{
		FOpenVDBModule::SetRegionScale(this, regionScale);
	}
}

void UVdbHandle::ReadGridTree(const FString &gridID)
{
	if (FOpenVDBModule::IsAvailable())
	{
		FIntVector StartFill; //dummy value (not used)
		FIntVector EndFill; //dummy value (not used)
		FOpenVDBModule::ReadGridTree(this, gridID, MeshMethod, StartFill, EndFill);
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
	const FVector &playerLocation,
	TSharedPtr<VertexBufferType> &VertexBuffer,
	TSharedPtr<PolygonBufferType> &PolygonBuffer,
	TSharedPtr<NormalBufferType> &NormalBuffer,
	TSharedPtr<UVMapBufferType> &UVMapBuffer,
	TSharedPtr<VertexColorBufferType> &VertexColorBuffer,
	TSharedPtr<TangentBufferType> &TangentBuffer,
	FVector &worldStart,
	FVector &worldEnd,
	FVector &startLocation)
{
	if (FOpenVDBModule::IsAvailable())
	{
		FOpenVDBModule::MeshGrid(
			this,
			gridID,
			playerLocation,
			VertexBuffer,
			PolygonBuffer,
			NormalBuffer,
			UVMapBuffer,
			VertexColorBuffer,
			TangentBuffer,
			MeshMethod,
			worldStart,
			worldEnd,
			startLocation);
	}
}

FIntVector UVdbHandle::GetRegionIndex(const FVector &worldLocation)
{
	FIntVector regionIndex;
	if (FOpenVDBModule::IsAvailable())
	{
		regionIndex = FOpenVDBModule::GetRegionIndex(this, worldLocation);
	}
	return regionIndex;
}

void UVdbHandle::WriteAllGrids()
{
	if (FOpenVDBModule::IsAvailable())
	{
		FOpenVDBModule::WriteAllGrids(this);
	}
}