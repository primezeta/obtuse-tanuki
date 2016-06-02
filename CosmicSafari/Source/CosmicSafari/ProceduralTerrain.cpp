// Fill out your copyright notice in the Description page of Project Settings.

#include "CosmicSafari.h"
#include "FirstPersonCPPCharacter.h"
#include "ProceduralTerrain.h"

// Sets default values
AProceduralTerrain::AProceduralTerrain(const FObjectInitializer& ObjectInitializer)
{
	VdbHandle = ObjectInitializer.CreateDefaultSubobject<UVdbHandle>(this, TEXT("VDBHandle"));
	check(VdbHandle != nullptr);
	Material = ObjectInitializer.CreateDefaultSubobject<UMaterial>(this, TEXT("TerrainMaterial"));
	check(Material != nullptr);

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
	TerrainMesh->SectionCount = 1; //TODO: One section per material?
	TerrainMesh->MeshID = VdbHandle->AddGrid(name, gridIndex, VoxelSize);
	TerrainMesh->RegisterComponent();
	TerrainMesh->AttachTo(RootComponent);
	if (Material)
	{
		TerrainMesh->SetMaterial(0, Material);
	}
	TerrainMeshComponents.Add(TerrainMesh);
}

// Called when the game starts or when spawned
void AProceduralTerrain::BeginPlay()
{
	Super::BeginPlay();

	for (auto i = TerrainMeshComponents.CreateIterator(); i; ++i)
	{
		UProceduralTerrainMeshComponent &TerrainMeshComponent = **i;
		for (auto j = 0; j < TerrainMeshComponent.SectionCount; ++j)
		{
			TSharedPtr<VertexBufferType> VertexBufferPtr;
			TSharedPtr<PolygonBufferType> PolygonBufferPtr;
			TSharedPtr<NormalBufferType> NormalBufferPtr;
			TSharedPtr<UVMapBufferType> UVMapBufferPtr;
			TSharedPtr<VertexColorBufferType> VertexColorsBufferPtr;
			TSharedPtr<TangentBufferType> TangentsBufferPtr;
			VdbHandle->MeshGrid(
				TerrainMeshComponent.MeshID,
				VertexBufferPtr,
				PolygonBufferPtr,
				NormalBufferPtr,
				UVMapBufferPtr,
				VertexColorsBufferPtr,
				TangentsBufferPtr);

			if (!TerrainMeshComponent.IsGridSectionMeshed)
			{
				//First time, copy mesh geometry to the procedural mesh renderer
				if (VertexBufferPtr.IsValid() && PolygonBufferPtr.IsValid() && NormalBufferPtr.IsValid() && UVMapBufferPtr.IsValid() && VertexColorsBufferPtr.IsValid() && TangentsBufferPtr.IsValid())
				{
					TerrainMeshComponent.CreateMeshSection(
						j,
						*VertexBufferPtr,
						*PolygonBufferPtr,
						*NormalBufferPtr,
						*UVMapBufferPtr,
						*VertexColorsBufferPtr,
						*TangentsBufferPtr,
						bCreateCollision);
					TerrainMeshComponent.IsGridSectionMeshed = true;
				}
			}
			else
			{
				//Already meshed, just update dynamic geometry
				TerrainMeshComponent.UpdateMeshSection(
					j,
					*VertexBufferPtr,
					*NormalBufferPtr,
					*UVMapBufferPtr,
					*VertexColorsBufferPtr,
					*TangentsBufferPtr);
			}
			TerrainMeshComponent.SetMeshSectionVisible(j, TerrainMeshComponent.IsGridSectionMeshed);
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
	FVector TerrainLocation;
	if (Character)
	{
		TerrainLocation = Character->GetActorLocation() - TerrainMeshComponents[0]->StartLocation;
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