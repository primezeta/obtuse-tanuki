// Fill out your copyright notice in the Description page of Project Settings.

#include "CosmicSafari.h"
#include "FirstPersonCPPCharacter.h"
#include "ProceduralTerrain.h"

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
}

void AProceduralTerrain::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	//Set the number of voxels per grid region index
	VdbHandle->SetRegionScale(RegionDimensions);

	//Add the first grid region and surrounding regions
	StartRegion = AddTerrainComponent(FIntVector(0, 0, 0));
	DirtyGridRegions.Enqueue(StartRegion);
	for (int32 x = 0; x <= RegionRadiusX; ++x)
	{
		for (int32 y = 0; y <= RegionRadiusY; ++y)
		{
			for (int32 z = 0; z <= RegionRadiusZ; ++z)
			{
				if (x != 0 || y != 0 || z != 0)
				{
					DirtyGridRegions.Enqueue(AddTerrainComponent(FIntVector(x, y, z)));
					DirtyGridRegions.Enqueue(AddTerrainComponent(FIntVector(-x, -y, -z)));
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
	terrainMesh.InitMeshComponent(VdbHandle);
	terrainMesh.bGenerateOverlapEvents = true;
	terrainMesh.IsGridDirty = true;
	terrainMesh.IsGridReady = false;
	terrainMesh.CreateCollision = bCreateCollision;
	terrainMesh.SetWorldScale3D(VoxelSize);
	terrainMesh.RegisterComponent();
	terrainMesh.AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);

	//Add the grid and read it from file
	const FString gridID = terrainMesh.AddGrid(gridIndex, FVector(1.0f));
	terrainMesh.MeshID = gridID;
	TArray<TEnumAsByte<EVoxelType>> sectionMaterialIDs;
	terrainMesh.ReadGridTree(sectionMaterialIDs);

	//Initialize an empty mesh section per voxel type
	for (int32 i = 0; i < FVoxelData::VOXEL_TYPE_COUNT; ++i)
	{
		bool createSectionCollision = bCreateCollision && i != (int32)EVoxelType::VOXEL_NONE && i != (int32)EVoxelType::VOXEL_WATER;
		terrainMesh.CreateEmptyMeshSection(i, createSectionCollision);
	}

	//Create a material for each voxel type
	int32 sectionIndex = 0;
	for (auto i = sectionMaterialIDs.CreateConstIterator(); i; ++i, ++sectionIndex)
	{
		terrainMesh.MeshTypes.Add(sectionIndex, *i);
		terrainMesh.SectionCount++;
		UMaterial * sectionMat = MeshMaterials[(int32)i->GetValue()-1];
		if (sectionMat != nullptr)
		{
			terrainMesh.SetMaterial(sectionIndex, sectionMat);
			UE_LOG(LogFlying, Display, TEXT("%s section %d material set to %s"), *terrainMesh.MeshID, (int32)i->GetValue(), *sectionMat->GetName());
		}
	}
	TerrainMeshComponents.Add(regionName, TerrainMesh);
	return regionName;
}

// Called when the game starts or when spawned
void AProceduralTerrain::BeginPlay()
{
	Super::BeginPlay();
	check(!GridMeshingThread.IsValid());
	GridMeshingThread = TSharedPtr<FGridMeshingThread>(new FGridMeshingThread(DirtyGridRegions, TerrainMeshComponents));
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

	for (auto i = TerrainMeshComponents.CreateIterator(); i; ++i)
	{
		check(i.Value() != nullptr);
		UProceduralTerrainMeshComponent& terrainMeshComponent = *(i.Value());
		if (!terrainMeshComponent.IsGridReady)
		{
			continue;
		}
		for (auto j = terrainMeshComponent.MeshTypes.CreateConstIterator(); j; ++j)
		{
			const int32 &sectionIndex = j.Key();
			if (!terrainMeshComponent.IsSectionReady[sectionIndex])
			{
				continue;
			}
			terrainMeshComponent.FinishMeshSection(sectionIndex, true);
			//TODO: Create logic for using UpdateMeshSection
			//TODO: Use non-deprecated CreateMeshSection_Linear
			terrainMeshComponent.IsSectionReady[sectionIndex] = false;
			return; //short-circuit return out of the loop so that one mesh section per tick is finished
		}
		terrainMeshComponent.IsGridReady = false;
	}
}