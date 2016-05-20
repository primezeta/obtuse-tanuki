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

FString UVdbHandle::AddGrid(const FString &gridName, const FIntVector &regionIndex, const FVector &voxelSize, bool bCreateCollision)
{
	FString gridID;
	if (FOpenVDBModule::IsAvailable())
	{
		UProceduralTerrainMeshComponent * TerrainMesh = NewObject<UProceduralTerrainMeshComponent>(this, FName(*gridName));
		check(TerrainMesh != nullptr);
		gridID = FOpenVDBModule::AddGrid(this, gridName, regionIndex, voxelSize);
		TerrainMesh->bGenerateOverlapEvents = true;
		TerrainMesh->MeshName = gridName;
		TerrainMesh->MeshID = gridID;
		TerrainMesh->IsGridSectionMeshed = false;
		TerrainMesh->CreateCollision = bCreateCollision;
		TerrainMeshComponents.Add(TerrainMesh);
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

void UVdbHandle::ReadGridTrees()
{
	if (FOpenVDBModule::IsAvailable())
	{
		FIntVector StartFill; //dummy value (not used)
		FIntVector EndFill; //dummy value (not used)
		for (TArray<UProceduralTerrainMeshComponent*>::TConstIterator i = TerrainMeshComponents.CreateConstIterator(); i; ++i)
		{
			FOpenVDBModule::ReadGridTree(this, (*i)->MeshID, MeshMethod, StartFill, EndFill);
		}
	}
}

void UVdbHandle::GetVoxelCoord(const FString &gridID, const FVector &worldLocation, FIntVector &outVoxelCoord)
{
	if (FOpenVDBModule::IsAvailable())
	{
		FOpenVDBModule::GetVoxelCoord(this, gridID, worldLocation, outVoxelCoord);
	}
}

void UVdbHandle::MeshGrids(UWorld * World,
	FVector &worldStart,
	FVector &worldEnd,
	TArray<FVector> &startLocations)
{
	if (FOpenVDBModule::IsAvailable())
	{
		for (auto i = TerrainMeshComponents.CreateConstIterator(); i; ++i)
		{
			FVector startLocation;
			FOpenVDBModule::MeshGrid(this,
				World,
				*i,
				MeshMethod,
				worldStart,
				worldEnd,
				startLocation);
			startLocations.Add(startLocation);
		}
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