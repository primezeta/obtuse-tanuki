// Fill out your copyright notice in the Description page of Project Settings.

#include "CosmicSafari.h"
#include "FirstPersonCPPCharacter.h"
#include "ProceduralTerrain.h"
#include "Engine.h"

// Sets default values
AProceduralTerrain::AProceduralTerrain(const FObjectInitializer& ObjectInitializer)
{
	VdbHandle = ObjectInitializer.CreateDefaultSubobject<UVdbHandle>(this, TEXT("VDBHandle"));
	check(VdbHandle != nullptr);

	MeshMaterials.SetNum(FVoxelData::VOXEL_TYPE_COUNT);
	for (int32 i = 0; i < FVoxelData::VOXEL_TYPE_COUNT; ++i)
	{
		UMaterial * Material = ObjectInitializer.CreateDefaultSubobject<UMaterial>(this, *FString::Printf(TEXT("TerrainMaterial.%d"), i));
		check(Material != nullptr);
		MeshMaterials[i] = Material;
	}

	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	SetActorEnableCollision(true);

	VoxelSize = FVector(1.0f, 1.0f, 1.0f);
	GridMeshingThread = nullptr;
	SetActorTickEnabled(true);
	RegionRadiusX = 0; //Radius 0 means the start region will be generated, but 0 surrounding regions (in the respective axis) will be generated
	RegionRadiusY = 0;
	RegionRadiusZ = 0;
	NumTotalGridStates = 0;
}

void AProceduralTerrain::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	//Set the number of voxels per grid region index
	VdbHandle->SetRegionScale(RegionDimensions);

	//Add the first grid region and surrounding regions
	StartRegion = AddTerrainComponent(FIntVector(0, 0, 0));
	for (int32 x = 0; x <= RegionRadiusX; ++x)
	{
		for (int32 y = 0; y <= RegionRadiusY; ++y)
		{
			for (int32 z = 0; z <= RegionRadiusZ; ++z)
			{
				if (x != 0 || y != 0 || z != 0)
				{
					AddTerrainComponent(FIntVector(x, y, z));
					AddTerrainComponent(FIntVector(-x, -y, -z));
				}
			}
		}
	}

	SetActorRelativeLocation(-TerrainMeshComponents[StartRegion]->StartLocation);
}

FString AProceduralTerrain::AddTerrainComponent(const FIntVector &gridIndex)
{
	//TODO: Check if terrain component already exists
	const FString regionName = TEXT("[") + gridIndex.ToString() + TEXT("]");	

	//Initialize the mesh component for the grid region
	UProceduralTerrainMeshComponent * TerrainMesh = NewObject<UProceduralTerrainMeshComponent>(this);
	check(TerrainMesh != nullptr);
	UProceduralTerrainMeshComponent &terrainMesh = *TerrainMesh;
	terrainMesh.MeshID = regionName;
	terrainMesh.bGenerateOverlapEvents = true;
	terrainMesh.IsGridDirty = true;
	terrainMesh.NumReadySections = 0;
	terrainMesh.CreateCollision = bCreateCollision;
	terrainMesh.VoxelSize = FVector(1.0f);
	terrainMesh.SetWorldScale3D(VoxelSize);
	terrainMesh.RegisterComponent();
	terrainMesh.AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	terrainMesh.VdbHandle = VdbHandle;

	//Initialize an empty mesh section per voxel type and create a material for each voxel type
	for (int32 i = 0; i < FVoxelData::VOXEL_TYPE_COUNT; ++i)
	{
		const int32 sectionIndex = terrainMesh.SectionCount;
		terrainMesh.SectionCount++;
		bool createSectionCollision = bCreateCollision && i != (int32)EVoxelType::VOXEL_WATER;
		terrainMesh.CreateEmptyMeshSection(sectionIndex, createSectionCollision);
		terrainMesh.MeshTypes.Add(sectionIndex, (EVoxelType)i);
		UMaterial * sectionMat = MeshMaterials[i];
		if (sectionMat != nullptr)
		{
			terrainMesh.SetMaterial(sectionIndex, sectionMat);
			UE_LOG(LogFlying, Display, TEXT("%s section %d material set to %s"), *terrainMesh.MeshID, i, *sectionMat->GetName());
		}
	}
	TerrainMeshComponents.Add(regionName, TerrainMesh);
	NumTotalGridStates = TerrainMeshComponents.Num() * NUM_TOTAL_GRID_STATES;
	return regionName;
}

// Called when the game starts or when spawned
void AProceduralTerrain::BeginPlay()
{
	Super::BeginPlay();
	check(!GridMeshingThread.IsValid());
	GridMeshingThread = TSharedPtr<FGridMeshingThread>(new FGridMeshingThread(DirtyGridRegions));
	FString name;
	GetName(name);
	FRunnableThread::Create(GridMeshingThread.Get(), *FString::Printf(TEXT("GridMeshingThread:%s"), *name));
}

void AProceduralTerrain::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	VdbHandle->WriteAllGrids();
	if (GridMeshingThread.IsValid())
	{
		GridMeshingThread->Stop();
	}
}

// Called every frame
void AProceduralTerrain::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	int32 numStates = 0;
	for (auto i = TerrainMeshComponents.CreateIterator(); i; ++i)
	{
		numStates += i.Value()->NumStatesRemaining;
	}
	check(NumTotalGridStates > 0);
	const float percentComplete = 100.0f - 100.0f * ((float)numStates / (float)NumTotalGridStates);
	GEngine->AddOnScreenDebugMessage(0, 5.f, FColor::Red, FString::Printf(TEXT("%d"), (int32)percentComplete));
	
	if (numStates == 0)
	{
		return;
	}

	UProceduralTerrainMeshComponent *terrainMeshComponentPtr = nullptr;
	for (auto i = TerrainMeshComponents.CreateIterator(); i; ++i)
	{
		UProceduralTerrainMeshComponent *terrainMeshComponentPtr = i.Value();
		check(terrainMeshComponentPtr != nullptr);
		UProceduralTerrainMeshComponent& terrainMeshComponent = *terrainMeshComponentPtr;
		if (terrainMeshComponent.RegionState == EGridState::GRID_STATE_FINISHED)
		{
			continue;
		}

		if (terrainMeshComponent.RegionState == EGridState::GRID_STATE_INIT &&
			!terrainMeshComponent.IsQueued)
		{
			terrainMeshComponent.IsQueued = true;
			DirtyGridRegions.Enqueue(terrainMeshComponentPtr);
		}
		else if (terrainMeshComponent.RegionState == EGridState::GRID_STATE_READY)
		{
			for (int32 j = 0; j < FVoxelData::VOXEL_TYPE_COUNT; ++j)
			{
				if (terrainMeshComponent.IsSectionFinished[j] == (int32)true)
				{
					continue;
				}
				terrainMeshComponent.FinishSection(j, true);
				terrainMeshComponent.NumReadySections++;
				terrainMeshComponent.NumStatesRemaining--;
				if (terrainMeshComponent.NumReadySections == FVoxelData::VOXEL_TYPE_COUNT)
				{
					terrainMeshComponent.RegionState = EGridState::GRID_STATE_FINISHED;
					terrainMeshComponent.NumStatesRemaining--;
				}
				return; //short-circuit out so that one section of the region is finished at a time
			}
		}
	}
}