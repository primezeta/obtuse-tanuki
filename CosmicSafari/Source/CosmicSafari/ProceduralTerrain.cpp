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

	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.bCanEverTick = true;
	//SetActorEnableCollision(true); TODO: Need to enable collision on all parents?

	VoxelSize = FVector(1.0f, 1.0f, 1.0f);
	RegionRadiusX = 0; //Radius 0 means the start region will be generated, but 0 surrounding regions (in the respective axis) will be generated
	RegionRadiusY = 0;
	RegionRadiusZ = 0;
	NumberTotalGridStates = 0;
	NumberRegionsComplete = 0;
	NumberMeshingStatesRemaining = 0;
	PercentMeshingComplete = 0.0f;
	bWantsInitializeComponent = true;

	//SetTickableWhenPaused(true); TODO: Tick when paused to allow rendering to finish?
	//TODO: Set to not replicate subobjects?
}

void UProceduralTerrain::InitializeComponent()
{
	Super::InitializeComponent();
	RegisterComponent();

	check(VdbHandle);
	check(RegionRadiusX >= 0);
	check(RegionRadiusY >= 0);
	check(RegionRadiusZ >= 0);
	//Set the number of voxels per grid region index
	if (VdbHandle->SetRegionScale(RegionDimensions))
	{
		//Add the start region
		StartRegion = AddTerrainComponent(FIntVector(0, 0, 0));
		//Add the first grid region and surrounding regions, skipping the already added start region
		for (int32 x = -RegionRadiusX; x <= RegionRadiusX; ++x)
		{
			for (int32 y = -RegionRadiusY; y <= RegionRadiusY; ++y)
			{
				for (int32 z = -RegionRadiusZ; z <= RegionRadiusZ; ++z)
				{
					if (x != 0 || y != 0 || z != 0)
					{
						//Add regions surrounding the start region
						AddTerrainComponent(FIntVector(x, y, z));
					}
				}
			}
		}
	}
}

FString UProceduralTerrain::AddTerrainComponent(const FIntVector &gridIndex)
{
	const FString regionName = TEXT("[") + gridIndex.ToString() + TEXT("]");
	for (auto i = TerrainMeshComponents.CreateConstIterator(); i; ++i)
	{
		check((*i)->MeshName != regionName);
	}

	//Initialize the mesh component for the grid region
	UProceduralTerrainMeshComponent * TerrainMesh = NewObject<UProceduralTerrainMeshComponent>(this);
	check(TerrainMesh != nullptr);
	UProceduralTerrainMeshComponent &terrainMesh = *TerrainMesh;
	terrainMesh.MeshName = regionName;
	terrainMesh.bGenerateOverlapEvents = true;
	terrainMesh.IsGridDirty = true;
	terrainMesh.NumReadySections = 0;
	terrainMesh.CreateCollision = bCreateCollision;
	terrainMesh.VoxelSize = FVector(1.0f);
	terrainMesh.VdbHandle = VdbHandle;
	terrainMesh.RegionIndex = gridIndex;
	terrainMesh.RegionState = EGridState::GRID_STATE_INIT;
	terrainMesh.SetWorldScale3D(VoxelSize);
	terrainMesh.RegisterComponent();

	//Initialize an empty mesh section per voxel type and create a material for each voxel type
	for (int32 i = 0; i < FVoxelData::VOXEL_TYPE_COUNT; ++i)
	{
		const int32 sectionIndex = terrainMesh.SectionCount;
		terrainMesh.SectionCount++;
		bool createSectionCollision = bCreateCollision && i != (int32)EVoxelType::VOXEL_WATER;
		FString logMsg = FString::Printf(TEXT("Creating empty mesh section %s[%d]"), *terrainMesh.MeshName, sectionIndex);
		terrainMesh.CreateEmptyMeshSection(sectionIndex, createSectionCollision);
		UMaterial * sectionMat = MeshMaterials[i];
		if (sectionMat != nullptr)
		{
			terrainMesh.SetMaterial(sectionIndex, sectionMat);
			logMsg += FString::Printf(TEXT(" (%s)"), *sectionMat->GetName());
		}
		UE_LOG(LogFlying, Display, TEXT("%s"), *logMsg);
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
	for (auto i = TerrainMeshComponents.CreateConstIterator(); i; ++i)
	{
		check(*i);
		check((*i)->NumStatesRemaining >= 0);
		//(*i)->SetTickableWhenPaused(true); TODO: Allow ticking when paused?
		(*i)->SetComponentTickEnabled(true); //TODO: Async set component tick enabled in order to finish BeginPlay quicker?
	}
}

// Called every frame
void UProceduralTerrain::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	//Update percentage of sections meshing states that are done
	int32 numStates = 0;
	for (auto i = TerrainMeshComponents.CreateConstIterator(); i; ++i)
	{
		check(*i);
		check((*i)->NumStatesRemaining >= 0);
		numStates += (*i)->NumStatesRemaining;
	}
	NumberMeshingStatesRemaining = numStates;
	PercentMeshingComplete = 100.0f - 100.0f * ((float)NumberMeshingStatesRemaining / (float)NumberTotalGridStates);
}