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

	//Add the first grid region and all 8 surrounding regions
	StartRegion = AddTerrainComponent(FIntVector(0, 0, 0));
	AddTerrainComponent(FIntVector(1, 0, 0));
	AddTerrainComponent(FIntVector(1, -1, 0));
	AddTerrainComponent(FIntVector(0, -1, 0));
	AddTerrainComponent(FIntVector(-1, -1, 0));
	AddTerrainComponent(FIntVector(-1, 0, 0));
	AddTerrainComponent(FIntVector(-1, 1, 0));
	AddTerrainComponent(FIntVector(0, 1, 0));
	AddTerrainComponent(FIntVector(1, 1, 0));
}

FString AProceduralTerrain::AddTerrainComponent(const FIntVector &gridIndex)
{
	//TODO: Check if terrain component already exists
	const FString regionName = TEXT("Region.") + gridIndex.ToString();
	const FString gridID = VdbHandle->AddGrid(regionName, gridIndex, VoxelSize);
	UProceduralTerrainMeshComponent * TerrainMesh = NewObject<UProceduralTerrainMeshComponent>(this, FName(*gridID));
	check(TerrainMesh != nullptr);
	TerrainMesh->MeshID = gridID;
	TerrainMesh->bGenerateOverlapEvents = true;
	TerrainMesh->IsGridSectionMeshed = false;
	TerrainMesh->CreateCollision = bCreateCollision;
	TerrainMesh->RegisterComponent();
	TerrainMesh->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	GridRegions.Add(regionName);

	//Read the grid from file
	TArray<TEnumAsByte<EVoxelType>> sectionMaterialIDs;
	VdbHandle->ReadGridTree(gridID, sectionMaterialIDs);

	//Create a mesh section index for each material ID that exists in this grid region
	int32 sectionIndex = 0;
	for (auto i = sectionMaterialIDs.CreateConstIterator(); i; ++i, ++sectionIndex)
	{
		TerrainMesh->MeshTypes.Add(sectionIndex, *i);
		TerrainMesh->SectionCount++;
		UMaterial * sectionMat = MeshMaterials[i.GetIndex()];
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
			TArray<FGridMeshBuffers> meshBuffers;
			meshBuffers.SetNum(VOXEL_TYPE_COUNT);

			UProceduralTerrainMeshComponent &TerrainMeshComponent = *TerrainMeshComponents[regionName];
			VdbHandle->MeshGrid(TerrainMeshComponent.MeshID, meshBuffers);
			for (auto j = TerrainMeshComponent.MeshTypes.CreateConstIterator(); j; ++j)
			{
				FGridMeshBuffers &buffers = meshBuffers[(int32)j.Value()];
				TerrainMeshComponent.CreateMeshSection(
					j.Key(),
					buffers.VertexBuffer,
					buffers.PolygonBuffer,
					buffers.NormalBuffer,
					buffers.UVMapBuffer,
					buffers.VertexColorBuffer,
					buffers.TangentBuffer,
					bCreateCollision);
				//TODO: Create logic for using UpdateMeshSection
				//TODO: Use non-deprecated CreateMeshSection_Linear
				TerrainMeshComponent.SetMeshSectionVisible(j.Key(), true);
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