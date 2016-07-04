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
		MeshBuffers.Add(FGridMeshBuffers());
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

	int32 testRegionDim = 0;
	//Add the first grid region and all 8 surrounding regions
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
	const FString gridID = VdbHandle->AddGrid(regionName, gridIndex, FVector(1.0f), MeshBuffers);
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
		VdbHandle->MeshGrid(TerrainMeshComponent.MeshID);

		for (auto j = TerrainMeshComponent.MeshTypes.CreateConstIterator(); j; ++j)
		{
			const int32 &sectionIndex = j.Key();
			const EVoxelType &voxelType = j.Value();
			const int32 &buffIdx = (int32)voxelType;
			auto &meshBuffs = MeshBuffers[buffIdx];
			UE_LOG(LogFlying, Display, TEXT("%s num vertices[%d] = %d"), *TerrainMeshComponent.MeshID, buffIdx, meshBuffs.VertexBuffer.Num());
			UE_LOG(LogFlying, Display, TEXT("%s num normals[%d]  = %d"), *TerrainMeshComponent.MeshID, buffIdx, meshBuffs.NormalBuffer.Num());
			UE_LOG(LogFlying, Display, TEXT("%s num colors[%d]   = %d"), *TerrainMeshComponent.MeshID, buffIdx, meshBuffs.VertexColorBuffer.Num());
			UE_LOG(LogFlying, Display, TEXT("%s num tangents[%d] = %d"), *TerrainMeshComponent.MeshID, buffIdx, meshBuffs.TangentBuffer.Num());
			UE_LOG(LogFlying, Display, TEXT("%s num polygons[%d] = %d"), *TerrainMeshComponent.MeshID, buffIdx, meshBuffs.PolygonBuffer.Num());
			UE_LOG(LogFlying, Display, TEXT("%s num uv maps[%d]  = %d"), *TerrainMeshComponent.MeshID, buffIdx, meshBuffs.UVMapBuffer.Num());
			TerrainMeshComponent.CreateMeshSection(
				sectionIndex,
				meshBuffs.VertexBuffer,
				meshBuffs.PolygonBuffer,
				meshBuffs.NormalBuffer,
				meshBuffs.UVMapBuffer,
				meshBuffs.VertexColorBuffer,
				meshBuffs.TangentBuffer,
				bCreateCollision && voxelType != EVoxelType::VOXEL_WATER);
			TerrainMeshComponent.SetMeshSectionVisible(sectionIndex, true);
			//TODO: Create logic for using UpdateMeshSection
			//TODO: Use non-deprecated CreateMeshSection_Linear
		}

		FVector worldStart;
		FVector worldEnd;
		FVector firstActive;
		VdbHandle->GetGridDimensions(TerrainMeshComponent.MeshID, worldStart, worldEnd, firstActive);
		TerrainMeshComponent.StartLocation = firstActive;
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