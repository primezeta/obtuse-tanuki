// Fill out your copyright notice in the Description page of Project Settings.

#include "CosmicSafari.h"
#include "FirstPersonCPPCharacter.h"
#include "ProceduralTerrain.h"

// Sets default values
AProceduralTerrain::AProceduralTerrain(const FObjectInitializer& ObjectInitializer)
{
	UVdbHandle * VdbHandle = ObjectInitializer.CreateDefaultSubobject<UVdbHandle>(this, TEXT("VDBHandle"));
	check(VdbHandle != nullptr);

	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	SetActorEnableCollision(true);

	//TerrainMeshComponent->SetMaterial(0, TerrainMaterial);
	//static ConstructorHelpers::FObjectFinder<UMaterial> TerrainMaterialObject(TEXT("Material'/Engine/EngineMaterials/DefaultDeferredDecalMaterial.DefaultDeferredDecalMaterial'"));
	//if (TerrainMaterialObject.Succeeded())
	//{
	//	TerrainMaterial = (UMaterial*)TerrainMaterialObject.Object;
	//	TerrainDynamicMaterial = UMaterialInstanceDynamic::Create(TerrainMaterial, this);
	//	TerrainMeshComponent->SetMaterial(0, TerrainDynamicMaterial);
	//}
	//if (TerrainMaterial)
	//{
	//	auto material = TerrainMaterial->GetClass();
	//	UMaterialInstanceDynamic::Create(material,)
	//	material->Create
	//	TerrainMeshComponent->SetMaterial(0, TerrainDynamicMaterial);
	//}

	VoxelSize = FVector(1.0f, 1.0f, 1.0f);
}

void AProceduralTerrain::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	//Set the scale of all grid regions
	VdbHandle->SetRegionScale(RegionDimensions);

	//Add the first grid region and all 8 regions surrounding the first
	VdbHandle->AddGrid(TEXT("Region[0,0,0]"), FIntVector(0, 0, 0), VoxelSize, bCreateCollision);
	VdbHandle->AddGrid(TEXT("Region[1,0,0]"), FIntVector(1, 0, 0), VoxelSize, bCreateCollision);
	VdbHandle->AddGrid(TEXT("Region[1,-1,0]"), FIntVector(1, -1, 0), VoxelSize, bCreateCollision);
	VdbHandle->AddGrid(TEXT("Region[0,-1,0]"), FIntVector(0, -1, 0), VoxelSize, bCreateCollision);
	VdbHandle->AddGrid(TEXT("Region[-1,-1,0]"), FIntVector(-1, -1, 0), VoxelSize, bCreateCollision);
	VdbHandle->AddGrid(TEXT("Region[-1,0,0]"), FIntVector(-1, 0, 0), VoxelSize, bCreateCollision);
	VdbHandle->AddGrid(TEXT("Region[-1,1,0]"), FIntVector(-1, 1, 0), VoxelSize, bCreateCollision);
	VdbHandle->AddGrid(TEXT("Region[0,1,0]"), FIntVector(0, 1, 0), VoxelSize, bCreateCollision);
	VdbHandle->AddGrid(TEXT("Region[1,1,0]"), FIntVector(1, 1, 0), VoxelSize, bCreateCollision);

	//Read all added grids from file
	VdbHandle->ReadGridTrees();
}

// Called when the game starts or when spawned
void AProceduralTerrain::BeginPlay()
{
	Super::BeginPlay();

	FVector ActiveWorldStart;
	FVector ActiveWorldEnd;
	TArray<FVector> StartLocations;
	VdbHandle->MeshGrids(GetWorld(),
		ActiveWorldStart,
	    ActiveWorldEnd,
		StartLocations);
	SetActorRelativeLocation(StartLocations[0]);
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