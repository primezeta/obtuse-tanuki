// Fill out your copyright notice in the Description page of Project Settings.

#include "CosmicSafari.h"
#include "FirstPersonCPPCharacter.h"
#include "ProceduralTerrain.h"

// Sets default values
AProceduralTerrain::AProceduralTerrain(const FObjectInitializer& ObjectInitializer)
{
	VdbHandle = ObjectInitializer.CreateDefaultSubobject<UVdbHandle>(this, TEXT("VDBHandle"));
	check(VdbHandle != nullptr);

	MeshMaterials.SetNum(VOXEL_TYPE_COUNT);
	for (int32 i = 0; i < FVoxelData::NumMaterials(); ++i)
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

	int32 testRegionDim = 3;
	//Add the first grid region and all 8 surrounding regions
	for (int32 x = -(testRegionDim-1); x < testRegionDim; ++x)
	{
		for (int32 y = -(testRegionDim - 1); y < testRegionDim; ++y)
		{
			int32 z = 0;
			if (x == 0 && y == 0)
			{
				StartRegion = AddTerrainComponent(FIntVector(x, y, z));
			}
			else
			{
				AddTerrainComponent(FIntVector(x, y, z));
			}
		}
	}
}

FString AProceduralTerrain::AddTerrainComponent(const FIntVector &gridIndex)
{
	//TODO: Check if terrain component already exists
	const FString regionName = TEXT("Region.") + gridIndex.ToString();

	GridRegions.Add(regionName);
	MeshBuffers.Add(regionName);
	MeshBuffers[regionName].SetNum(VOXEL_TYPE_COUNT);
	const FString gridID = VdbHandle->AddGrid(regionName, gridIndex, FVector(1.0f), MeshBuffers[regionName]);
	UProceduralTerrainMeshComponent * TerrainMesh = NewObject<UProceduralTerrainMeshComponent>(this, FName(*gridID));
	check(TerrainMesh != nullptr);
	TerrainMesh->MeshID = gridID;
	TerrainMesh->bGenerateOverlapEvents = true;
	TerrainMesh->IsGridSectionMeshed = false;
	TerrainMesh->CreateCollision = bCreateCollision;
	TerrainMesh->SetWorldScale3D(VoxelSize);
	TerrainMesh->RegisterComponent();
	TerrainMesh->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);

	//Read the grid from file
	TArray<TEnumAsByte<EVoxelType>> sectionMaterialIDs;
	VdbHandle->ReadGridTree(gridID, sectionMaterialIDs);

	//Create a mesh section index for each material ID that exists in this grid region
	int32 sectionIndex = 0;
	for (auto i = sectionMaterialIDs.CreateConstIterator(); i; ++i, ++sectionIndex)
	{
		TerrainMesh->MeshTypes.Add(sectionIndex, *i);
		TerrainMesh->SectionCount++;
		UMaterial * sectionMat = MeshMaterials[(int32)i->GetValue()-1];
		if (sectionMat != nullptr)
		{
			TerrainMesh->SetMaterial(sectionIndex, sectionMat);
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
		if (TerrainMeshComponents.Contains(regionName))
		{
			auto &meshBuffers = MeshBuffers[regionName];
			UProceduralTerrainMeshComponent &TerrainMeshComponent = *TerrainMeshComponents[regionName];
			VdbHandle->MeshGrid(TerrainMeshComponent.MeshID);
			for (auto j = TerrainMeshComponent.MeshTypes.CreateConstIterator(); j; ++j)
			{
				const EVoxelType voxelType = j.Value();
				const int32 &buffIdx = (int32)voxelType;
				const int32 &meshIdx = j.Key();
				FGridMeshBuffers &buffers = meshBuffers[buffIdx];
				TerrainMeshComponent.CreateMeshSection(
					meshIdx,
					buffers.VertexBuffer,
					buffers.PolygonBuffer,
					buffers.NormalBuffer,
					buffers.UVMapBuffer,
					buffers.VertexColorBuffer,
					buffers.TangentBuffer,
					bCreateCollision && voxelType != EVoxelType::VOXEL_WATER);
				TerrainMeshComponent.SetMeshSectionVisible(meshIdx, true);
				buffers.ClearBuffers(); //Clear buffers after copying to the mesh section
				//TODO: Create logic for using UpdateMeshSection
				//TODO: Use non-deprecated CreateMeshSection_Linear
			}

			FVector worldStart;
			FVector worldEnd;
			FVector firstActive;
			VdbHandle->GetGridDimensions(TerrainMeshComponent.MeshID, worldStart, worldEnd, firstActive);
			TerrainMeshComponent.StartLocation = firstActive;
		}
	}

	//If there's a character position the terrain under the character
	UWorld * World = GetWorld();
	ACharacter* Character = UGameplayStatics::GetPlayerCharacter(World, 0);
	FVector TerrainLocation;
	if (Character)
	{
		TerrainLocation = Character->GetActorLocation() - TerrainMeshComponents[StartRegion]->StartLocation;
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