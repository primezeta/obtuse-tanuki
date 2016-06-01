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

	//Set the number of voxels per grid region index
	VdbHandle->SetRegionScale(RegionDimensions);

	//Add the first grid region and all 8 surrounding regions
	AddTerrainComponent(TEXT("Region[0,0,0]"), FIntVector(0, 0, 0));
	AddTerrainComponent(TEXT("Region[1,0,0]"), FIntVector(1, 0, 0));
	AddTerrainComponent(TEXT("Region[1,-1,0]"), FIntVector(1, -1, 0));
	AddTerrainComponent(TEXT("Region[0,-1,0]"), FIntVector(0, -1, 0));
	AddTerrainComponent(TEXT("Region[-1,-1,0]"), FIntVector(-1, -1, 0));
	AddTerrainComponent(TEXT("Region[-1,0,0]"), FIntVector(-1, 0, 0));
	AddTerrainComponent(TEXT("Region[-1,1,0]"), FIntVector(-1, 1, 0));
	AddTerrainComponent(TEXT("Region[0,1,0]"), FIntVector(0, 1, 0));
	AddTerrainComponent(TEXT("Region[1,1,0]"), FIntVector(1, 1, 0));

	//Read all added grids from file
	for (TArray<UProceduralTerrainMeshComponent*>::TConstIterator i = TerrainMeshComponents.CreateConstIterator(); i; ++i)
	{
		VdbHandle->ReadGridTree((*i)->MeshID);
	}
}

void AProceduralTerrain::AddTerrainComponent(const FString &name, const FIntVector &gridIndex)
{
	//TODO: Check if terrain component already exists
	UProceduralTerrainMeshComponent * TerrainMesh = NewObject<UProceduralTerrainMeshComponent>(this, FName(*name));
	check(TerrainMesh != nullptr);
	TerrainMesh->bGenerateOverlapEvents = true;
	TerrainMesh->MeshName = name;
	TerrainMesh->IsGridSectionMeshed = false;
	TerrainMesh->CreateCollision = bCreateCollision;
	TerrainMesh->SectionIndex = TerrainMeshComponents.Num();
	TerrainMesh->MeshID = VdbHandle->AddGrid(name, gridIndex, VoxelSize);
	TerrainMeshComponents.Add(TerrainMesh);
}

// Called when the game starts or when spawned
void AProceduralTerrain::BeginPlay()
{
	Super::BeginPlay();

	UWorld * World = GetWorld();
	ACharacter* Character = UGameplayStatics::GetPlayerCharacter(World, 0);
	FVector PlayerLocation;
	if (Character)
	{
		PlayerLocation = Character->GetActorLocation();
	}

	TArray<FVector> StartLocations;
	for (auto i = TerrainMeshComponents.CreateIterator(); i; ++i)
	{
		UProceduralTerrainMeshComponent &TerrainMeshComponent = **i;
		if (!TerrainMeshComponent.IsGridSectionMeshed)
		{
			TSharedPtr<VertexBufferType> VertexBufferPtr;
			TSharedPtr<PolygonBufferType> PolygonBufferPtr;
			TSharedPtr<NormalBufferType> NormalBufferPtr;
			TSharedPtr<UVMapBufferType> UVMapBufferPtr;
			TSharedPtr<VertexColorBufferType> VertexColorsBufferPtr;
			TSharedPtr<TangentBufferType> TangentsBufferPtr;
			FVector ActiveWorldStart;
			FVector ActiveWorldEnd;
			FVector StartLocation;
			VdbHandle->MeshGrid(
				TerrainMeshComponent.MeshID,
				PlayerLocation,
				VertexBufferPtr,
				PolygonBufferPtr,
				NormalBufferPtr,
				UVMapBufferPtr,
				VertexColorsBufferPtr,
				TangentsBufferPtr,
				ActiveWorldStart,
				ActiveWorldEnd,
				StartLocation);
			TerrainMeshComponent.CreateMeshSection(
				TerrainMeshComponent.SectionIndex,
				*VertexBufferPtr,
				*PolygonBufferPtr,
				*NormalBufferPtr,
				*UVMapBufferPtr,
				*VertexColorsBufferPtr,
				*TangentsBufferPtr,
				bCreateCollision);
			TerrainMeshComponent.IsGridSectionMeshed = true;
			StartLocations.Add(StartLocation);
		}
	}
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