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
}

void AProceduralTerrain::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	//Set the number of voxels per grid region index
	VdbHandle->SetRegionScale(RegionDimensions);

	int32 testRegionDim = 2;
	//Add the first grid region and surrounding regions
	StartRegion = AddTerrainComponent(FIntVector(0, 0, 0));
	for (int32 x = -testRegionDim; x <= testRegionDim; ++x)
	{
		for (int32 y = -testRegionDim; y <= testRegionDim; ++y)
		{
			int32 z = 0;
			if (x != 0 || y != 0)
			{
				AddTerrainComponent(FIntVector(x, y, z));
			}
		}
	}
}

FString AProceduralTerrain::AddTerrainComponent(const FIntVector &gridIndex)
{
	//TODO: Check if terrain component already exists
	const FString regionName = TEXT("[") + gridIndex.ToString() + TEXT("]");	
	GridRegions.Add(regionName);

	//Initialize the mesh component for the grid region
	UProceduralTerrainMeshComponent * TerrainMesh = NewObject<UProceduralTerrainMeshComponent>(this);
	check(TerrainMesh != nullptr);
	TerrainMesh->InitMeshComponent(VdbHandle);
	TerrainMesh->bGenerateOverlapEvents = true;
	TerrainMesh->IsGridSectionMeshed = false;
	TerrainMesh->CreateCollision = bCreateCollision;
	TerrainMesh->SetWorldScale3D(VoxelSize);
	TerrainMesh->RegisterComponent();
	TerrainMesh->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);

	//Add the grid and read it from file
	const FString gridID = TerrainMesh->AddGrid(gridIndex, FVector(1.0f));
	TerrainMesh->MeshID = gridID;
	TArray<TEnumAsByte<EVoxelType>> sectionMaterialIDs;
	TerrainMesh->ReadGridTree(sectionMaterialIDs);

	//Initialize an empty mesh section per voxel type
	for (int32 i = 0; i < FVoxelData::VOXEL_TYPE_COUNT; ++i)
	{
		bool createSectionCollision = bCreateCollision && i != (int32)EVoxelType::VOXEL_NONE && i != (int32)EVoxelType::VOXEL_WATER;
		TerrainMesh->CreateEmptyMeshSection(i, createSectionCollision);
	}

	//Create a material for each voxel type
	int32 sectionIndex = 0;
	for (auto i = sectionMaterialIDs.CreateConstIterator(); i; ++i, ++sectionIndex)
	{
		TerrainMesh->MeshTypes.Add(sectionIndex, *i);
		TerrainMesh->SectionCount++;
		UMaterial * sectionMat = MeshMaterials[(int32)i->GetValue()-1];
		if (sectionMat != nullptr)
		{
			TerrainMesh->SetMaterial(sectionIndex, sectionMat);
			UE_LOG(LogFlying, Display, TEXT("%s section %d material set to %s"), *TerrainMesh->MeshID, (int32)i->GetValue(), *sectionMat->GetName());
		}
	}
	TerrainMeshComponents.Add(regionName, TerrainMesh);
	return regionName;
}

// Called when the game starts or when spawned
void AProceduralTerrain::BeginPlay()
{
	Super::BeginPlay();

	for (auto i = GridRegions.CreateConstIterator(); i; ++i)
	{
		const FString &regionName = *i;
		check(TerrainMeshComponents.Contains(regionName));
		UProceduralTerrainMeshComponent &TerrainMeshComponent = *TerrainMeshComponents[regionName];
		TerrainMeshComponent.MeshGrid();

		FBox sectionBounds = TerrainMeshComponent.GetGridDimensions();
		for (auto j = TerrainMeshComponent.MeshTypes.CreateConstIterator(); j; ++j)
		{
			const int32 &sectionIndex = j.Key();
			TerrainMeshComponent.FinishMeshSection(sectionIndex, true);
			//TODO: Create logic for using UpdateMeshSection
			//TODO: Use non-deprecated CreateMeshSection_Linear
		}
	}

	//If there's a character position the terrain under the character
	UWorld * World = GetWorld();
	ACharacter* Character = UGameplayStatics::GetPlayerCharacter(World, 0);
	FVector TerrainLocation = -TerrainMeshComponents[StartRegion]->StartLocation;
	if (Character)
	{
		TerrainLocation += Character->GetActorLocation();
	}
	SetActorRelativeLocation(TerrainLocation);
}

void AProceduralTerrain::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	VdbHandle->WriteAllGrids();
}

// Called every frame
void AProceduralTerrain::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}