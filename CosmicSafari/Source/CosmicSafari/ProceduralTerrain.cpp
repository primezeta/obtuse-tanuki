// Fill out your copyright notice in the Description page of Project Settings.

#include "CosmicSafari.h"
#include "FirstPersonCPPCharacter.h"
#include "ProceduralTerrain.h"
#include "Engine.h"

// Sets default values
UProceduralTerrain::UProceduralTerrain(const FObjectInitializer& ObjectInitializer)
{
	VdbHandle = CreateDefaultSubobject<UVdbHandle>(TEXT("VDBConfiguration"));
	check(VdbHandle != nullptr);

	MeshMaterials.SetNum(FVoxelData::VOXEL_TYPE_COUNT);
	for (int32 i = 0; i < FVoxelData::VOXEL_TYPE_COUNT; ++i)
	{
		UMaterial * Material = ObjectInitializer.CreateDefaultSubobject<UMaterial>(this, *FString::Printf(TEXT("TerrainMaterial.%d"), i));
		check(Material != nullptr);
		MeshMaterials[i] = Material;
	}

	PrimaryComponentTick.bCanEverTick = true;
	SetComponentTickEnabled(true);
	//SetActorEnableCollision(true);

	VoxelSize = FVector(1.0f, 1.0f, 1.0f);
	GridMeshingThread = nullptr;
	RegionRadiusX = 0; //Radius 0 means the start region will be generated, but 0 surrounding regions (in the respective axis) will be generated
	RegionRadiusY = 0;
	RegionRadiusZ = 0;
	NumberTotalGridStates = 0;
	NumberRegionsComplete = 0;
	NumberMeshingStatesRemaining = 0;
	PercentMeshingComplete = 0.0f;
	OldestGridState = EGridState::GRID_STATE_INIT;
	bWantsInitializeComponent = true;
}

void UProceduralTerrain::InitializeComponent()
{
	Super::InitializeComponent();
	RegisterComponent();
}

FString UProceduralTerrain::AddTerrainComponent(const FIntVector &gridIndex)
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
	//terrainMesh.AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform);
	terrainMesh.VdbHandle = VdbHandle;
	check(VdbHandle);
	terrainMesh.RegionIndex = gridIndex;
	terrainMesh.bTickStateAfterFinish = true;
	terrainMesh.RegionState = EGridState::GRID_STATE_INIT;

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
	TerrainMeshComponents.Add(TerrainMesh);
	NumberTotalGridStates += terrainMesh.NumStatesRemaining;
	NumberMeshingStatesRemaining = NumberTotalGridStates;
	return regionName;
}

UProceduralTerrainMeshComponent * UProceduralTerrain::GetTerrainComponent(const FIntVector &gridIndex)
{
	for (auto i = TerrainMeshComponents.CreateConstIterator(); i; ++i)
	{
		if ((*i)->RegionIndex == gridIndex)
		{
			return *i;
		}
	}
	return nullptr;
}

// Called when the game starts or when spawned
void UProceduralTerrain::BeginPlay()
{	
	Super::BeginPlay();

	//Set the number of voxels per grid region index
	check(VdbHandle);
	if (VdbHandle->SetRegionScale(RegionDimensions))
	{
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

	OldestGridState = EGridState::GRID_STATE_INIT;
	check(!GridMeshingThread.IsValid());
	GridMeshingThread = TSharedPtr<FGridMeshingThread>(new FGridMeshingThread(DirtyGridRegions));
	FString name;
	GetName(name);
	FRunnableThread::Create(GridMeshingThread.Get(), *FString::Printf(TEXT("GridMeshingThread:%s"), *name));
}

void UProceduralTerrain::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	check(VdbHandle);
	if (GridMeshingThread.IsValid())
	{
		GridMeshingThread->Stop();
		while (GridMeshingThread->isRunning);
	}
}

// Called every frame
void UProceduralTerrain::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (NumberMeshingStatesRemaining == 0)
	{
		//Nothing left to do for any sections
		return;
	}

	int32 earliestGridState = NUM_GRID_STATES;
	const int32 prevEarliestGridState = (int32)OldestGridState;
	int32 numStates = 0;
	for (auto i = TerrainMeshComponents.CreateIterator(); i; ++i)
	{
		check(*i);
		UProceduralTerrainMeshComponent * terrainMeshComponentPtr = *i;
		//Queue up the section to be asynchronously meshed, or if the async meshing operations are complete,
		//finish the section for anything that must be done on the game thread (e.g. physx collisions)
		numStates += EnqueueOrFinishSection(terrainMeshComponentPtr);
		const EGridState regionCurrentState = terrainMeshComponentPtr->RegionState;
		if (earliestGridState > (int32)regionCurrentState)
		{
			earliestGridState = (int32)regionCurrentState;
		}
	}

	NumberMeshingStatesRemaining = numStates;
	PercentMeshingComplete = 100.0f - 100.0f * ((float)NumberMeshingStatesRemaining / (float)NumberTotalGridStates);

	//Did one of the regions' state change during this tick?
	check(prevEarliestGridState > -1 && prevEarliestGridState < NUM_GRID_STATES);
	check(earliestGridState > -1);
	if (earliestGridState < NUM_GRID_STATES && earliestGridState != prevEarliestGridState)
	{
		//Notify of the changed grid state
		OldestGridState = (EGridState)earliestGridState;
	}
}

int32 UProceduralTerrain::EnqueueOrFinishSection(UProceduralTerrainMeshComponent *terrainMeshComponentPtr)
{
	UProceduralTerrainMeshComponent& terrainMeshComponent = *terrainMeshComponentPtr;

	const bool isVisible = true; //TODO
	int32 numStates = -1;
	if (terrainMeshComponent.RegionState == EGridState::GRID_STATE_READY)
	{
		//The region is ready. Now complete the parts of the section that must be done on the game thread (e.g. physx collision)
		for (int32 j = 0; j < FVoxelData::VOXEL_TYPE_COUNT; ++j)
		{
			const bool isChangedToFinished = terrainMeshComponent.FinishSection(j, isVisible);
			UE_LOG(LogFlying, Display, TEXT("%s section %d finished"), *terrainMeshComponent.MeshID, j);
			if (isChangedToFinished)
			{
				NumberRegionsComplete++;
				//Collision creation is expensive so finish only one region per call
				break;
			}
		}
		numStates = terrainMeshComponent.NumStatesRemaining;
	}
	else if (terrainMeshComponent.RegionState < EGridState::GRID_STATE_READY)
	{
		numStates = terrainMeshComponent.NumStatesRemaining; //get state count prior to queuing to prevent a potential race condition
		if (terrainMeshComponent.IsGridDirty && !terrainMeshComponent.IsQueued)
		{
			//The grid changed and is not currently queued - queue it up
			UE_LOG(LogFlying, Display, TEXT("%s queued for meshing"), *terrainMeshComponent.MeshID);
			terrainMeshComponent.IsQueued = true;
			DirtyGridRegions.Enqueue(terrainMeshComponentPtr);
		}
	}
	else
	{
		//Grid state is finished. Nothing to do here
		check(terrainMeshComponent.RegionState == EGridState::GRID_STATE_FINISHED);
		numStates = terrainMeshComponent.NumStatesRemaining;
		check(numStates == 0);
		UE_LOG(LogFlying, Display, TEXT("%s all sections meshed"), *terrainMeshComponent.MeshID);
	}
	check(numStates > -1);
	return numStates;
}