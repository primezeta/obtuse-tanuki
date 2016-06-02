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

FString UVdbHandle::AddGrid(const FString &gridName, const FIntVector &regionIndex, const FVector &voxelSize)
{
	FString gridID;
	if (isRegistered)
	{
		gridID = FOpenVDBModule::AddGrid(this, gridName, regionIndex, voxelSize);
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

void UVdbHandle::ReadGridTree(const FString &gridID)
{
	if (isRegistered)
	{
		FIntVector StartFill; //dummy value (not used)
		FIntVector EndFill; //dummy value (not used)
		FOpenVDBModule::ReadGridTree(this, gridID, MeshMethod, StartFill, EndFill);
	}
}

void UVdbHandle::GetVoxelCoord(const FString &gridID, const FVector &worldLocation, FIntVector &outVoxelCoord)
{
	if (isRegistered)
	{
		FOpenVDBModule::GetVoxelCoord(this, gridID, worldLocation, outVoxelCoord);
	}
}

void UVdbHandle::MeshGrid(const FString &gridID,
	TSharedPtr<VertexBufferType> &VertexBuffer,
	TSharedPtr<PolygonBufferType> &PolygonBuffer,
	TSharedPtr<NormalBufferType> &NormalBuffer,
	TSharedPtr<UVMapBufferType> &UVMapBuffer,
	TSharedPtr<VertexColorBufferType> &VertexColorBuffer,
	TSharedPtr<TangentBufferType> &TangentBuffer)
{
	if (isRegistered)
	{
		FOpenVDBModule::MeshGrid(
			this,
			gridID,
			VertexBuffer,
			PolygonBuffer,
			NormalBuffer,
			UVMapBuffer,
			VertexColorBuffer,
			TangentBuffer,
			MeshMethod);
	}
}

void UVdbHandle::GetGridDimensions(const FString &gridID, FVector &worldStart, FVector &worldEnd, FVector &firstActive)
{
	if (isRegistered)
	{
		FOpenVDBModule::GetGridDimensions(this, gridID, worldStart, worldEnd, firstActive);
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