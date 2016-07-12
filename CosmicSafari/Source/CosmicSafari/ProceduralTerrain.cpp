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
	NumberMeshingStatesRemaining = 0;
	bIsInitialLocationSet = false;
	PercentMeshingComplete = 0.0f;
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
	terrainMesh.RegionIndex = gridIndex;

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
	NumberMeshingStatesRemaining = NumTotalGridStates;
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

	//If initial location has not yet been set, set it based on the defined start region name
	if (!bIsInitialLocationSet && TerrainMeshComponents[StartRegion]->RegionState > EGridState::GRID_STATE_EXTRACT_SURFACE)
	{
		SetActorRelativeLocation(-TerrainMeshComponents[StartRegion]->StartLocation);
		bIsInitialLocationSet = true; //Only need to set once
	}

	if (NumberMeshingStatesRemaining == 0)
	{
		//Nothing left to do
		return;
	}

	//Queue up each section to be asynchronously meshed and then finish each section for anything that must be done on the game thread
	int32 numStates = 0;
	UProceduralTerrainMeshComponent *terrainMeshComponentPtr = nullptr;
	for (auto i = TerrainMeshComponents.CreateIterator(); i; ++i)
	{
		terrainMeshComponentPtr = i.Value();
		check(terrainMeshComponentPtr != nullptr);
		numStates += EnqueueOrFinishSection(terrainMeshComponentPtr);
	}

	NumberMeshingStatesRemaining = numStates;
	PercentMeshingComplete = 100.0f - 100.0f * ((float)NumberMeshingStatesRemaining / (float)NumTotalGridStates);
}

int32 AProceduralTerrain::EnqueueOrFinishSection(UProceduralTerrainMeshComponent *terrainMeshComponentPtr)
{
	UProceduralTerrainMeshComponent& terrainMeshComponent = *terrainMeshComponentPtr;

	const bool isVisible = true; //TODO
	int32 numStates = 0;
	if (terrainMeshComponent.RegionState == EGridState::GRID_STATE_FINISHED)
	{
		//Nothing to do
		numStates = terrainMeshComponent.NumStatesRemaining;
	}
	else if (terrainMeshComponent.RegionState == EGridState::GRID_STATE_READY)
	{
		//The region is ready. Now complete the parts of the section that must be done on the game thread (e.g. physx collision)
		for (int32 j = 0; j < FVoxelData::VOXEL_TYPE_COUNT; ++j)
		{
			const bool isChangedToFinished = terrainMeshComponent.FinishSection(j, isVisible);
			if (isChangedToFinished)
			{
				static int32 idx = 1;
				//Collision creation is expensive so finish only one region per call
				GEngine->AddOnScreenDebugMessage(idx++, 1.f, FColor::Red, FString::Printf(TEXT("%s finished (visible %d) %s %s"), *terrainMeshComponent.GetName(), terrainMeshComponent.IsVisible(), *terrainMeshComponent.SectionBounds.Min.ToString(), *terrainMeshComponent.SectionBounds.Max.ToString()));
				numStates = terrainMeshComponent.NumStatesRemaining;
				break;
			}
		}
	}
	else if (terrainMeshComponent.IsGridDirty && !terrainMeshComponent.IsQueued)
	{
		//The grid changed and is not currently queued - queue it up
		terrainMeshComponent.IsQueued = true;
		numStates = terrainMeshComponent.NumStatesRemaining; //get state count so that the state value doesn't change unexpectedly
		DirtyGridRegions.Enqueue(terrainMeshComponentPtr);
	}
	return numStates;
}