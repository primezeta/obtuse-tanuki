// Fill out your copyright notice in the Description page of Project Settings.

#include "CosmicSafari.h"
#include "ProceduralTerrain.h"

FOpenVDBModule * AProceduralTerrain::openVDBModule = nullptr;

void AProceduralTerrain::InitializeOpenVDBModule()
{
	if (openVDBModule == nullptr)
	{
		openVDBModule = &FOpenVDBModule::Get();
		if (!openVDBModule->IsAvailable())
		{
			UE_LOG(LogFlying, Warning, TEXT("Failed to start OpenVDBModule!"));
		}
		else
		{
			openVDBModule->StartupModule();
		}
	}
}

// Sets default values
AProceduralTerrain::AProceduralTerrain()
{
	AProceduralTerrain::InitializeOpenVDBModule();

	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	SetActorEnableCollision(true);
	TerrainMeshComponent = CreateDefaultSubobject<UProceduralTerrainMeshComponent>(TEXT("GeneratedTerrain"));
	//RootComponent = TerrainMeshComponent;
	TerrainMeshComponent->AttachTo(RootComponent);
	TerrainMeshComponent->SetWorldScale3D(FVector(100.0f, 100.0f, 100.0f));
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
}

void AProceduralTerrain::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	int32 maxHeight = 0;
	GridID = openVDBModule->CreateDynamicVdb(MeshSurfaceValue, MapBoundsStart, MapBoundsEnd, HeightMapRange);
	if (GridID == TEXT(""))
	{
		UE_LOG(LogFlying, Fatal, TEXT("Dynamic grid is invalid!")); //TODO: Better error handling
	}
	InitializeMeshSections();
}

// Called when the game starts or when spawned
void AProceduralTerrain::BeginPlay()
{
	Super::BeginPlay();	

	for (TArray<int32>::TConstIterator i = MeshSectionIndices.CreateConstIterator(); i; ++i)
	{
		int32 sectionIndex = *i;
		bool * isSectionMeshed = IsGridSectionMeshed.Find(sectionIndex);
		checkf(isSectionMeshed != nullptr, TEXT("ProceduralTerrain: null mesh section (section %d)"), sectionIndex);
		if (*isSectionMeshed)
		{
			//This section is alread meshed - skip it
			continue;
		}

		FIntVector * boundsStart = MeshSectionStart.Find(sectionIndex);
		FIntVector * boundsEnd = MeshSectionEnd.Find(sectionIndex);
		TArray<FVector> * vertices = MeshSectionVertices.Find(sectionIndex);
		TArray<int32> * indices = MeshSectionTriangleIndices.Find(sectionIndex);
		TArray<FVector2D> * uvs = MeshSectionUVMap.Find(sectionIndex);
		TArray<FVector> * normals = MeshSectionNormals.Find(sectionIndex);
		TArray<FColor> * colors = MeshSectionVertexColors.Find(sectionIndex);
		TArray<FProcMeshTangent> * tangents = MeshSectionTangents.Find(sectionIndex);

		checkf(boundsStart != nullptr, TEXT("ProceduralTerrain: null mesh section bounds start (section %d)"), sectionIndex);
		checkf(boundsEnd != nullptr, TEXT("ProceduralTerrain: null mesh section bounds end (section %d)"), sectionIndex);
		checkf(vertices != nullptr, TEXT("ProceduralTerrain: null mesh section vertices (section %d)"), sectionIndex);
		checkf(indices != nullptr, TEXT("ProceduralTerrain: null mesh section indices (section %d)"), sectionIndex);
		checkf(uvs != nullptr, TEXT("ProceduralTerrain: null mesh section UVs (section %d)"), sectionIndex);
		checkf(normals != nullptr, TEXT("ProceduralTerrain: null mesh section normals (section %d)"), sectionIndex);
		checkf(colors != nullptr, TEXT("ProceduralTerrain: null mesh section colors (section %d)"), sectionIndex);
		checkf(tangents != nullptr, TEXT("ProceduralTerrain: null mesh section tangents (section %d)"), sectionIndex);

		ovdb::meshing::VolumeDimensions dims(boundsStart->X, boundsEnd->X, boundsStart->Y, boundsEnd->Y, boundsStart->Z, HeightMapRange);
		FString regionID = openVDBModule->CreateGridMeshRegion(GridID, sectionIndex, dims, GridIsoValue, *vertices, *indices, *normals);
		if (regionID != FString(ovdb::meshing::INVALID_GRID_ID.data()))
		{
			TerrainMeshComponent->CreateTerrainMeshSection(sectionIndex, bCreateCollision, *vertices, *indices, *uvs, *normals, *colors, *tangents);
			TerrainMeshComponent->SetMeshSectionVisible(sectionIndex, true);
		}
		else
		{
			UE_LOG(LogFlying, Fatal, TEXT("Failed to load vdb geometry!"));
		}
	}
}

// Called every frame
void AProceduralTerrain::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AProceduralTerrain::InitializeMeshSections()
{
	int32 meshSectionIndex = 0;
	if (MeshSectionIndices.Num() > 0)
	{
		//Already initialized - do nothing
		return;
	}

	//Note: For now it is assumed that the region count evenly divides the map dimensions
	int32 dimX = (MapBoundsEnd.X - MapBoundsStart.X) / RegionCount.X;
	int32 dimY = (MapBoundsEnd.Y - MapBoundsStart.Y) / RegionCount.Y;
	int32 dimZ = (MapBoundsEnd.Z - MapBoundsStart.Z) / RegionCount.Z;
	for (int32 x = 0; x < RegionCount.X; x++)
	{
		int32 startX = dimX * x;
		int32 endX = dimX * (x + 1) - 1;
		for (int32 y = 0; y < RegionCount.Y; y++)
		{
			int32 startY = dimY * y;
			int32 endY = dimY * (y + 1) - 1;
			for (int32 z = 0; z < RegionCount.Z; z++)
			{
				//For now make sure to always set the region count Z to 1 in the editor. TODO: Improve this
				int32 startZ = dimZ * z;
				int32 endZ = dimZ * (z + 1) - 1;

				int32 sectionIndex = meshSectionIndex++;
				MeshSectionIndices.Add(sectionIndex);
				IsGridSectionMeshed.Add(sectionIndex, false);
				MeshSectionStart.Add(sectionIndex, FIntVector(startX, startY, startZ));
				MeshSectionEnd.Add(sectionIndex, FIntVector(endX, endY, endZ));
				MeshSectionVertices.Add(sectionIndex, TArray<FVector>());
				MeshSectionTriangleIndices.Add(sectionIndex, TArray<int32>());
				MeshSectionUVMap.Add(sectionIndex, TArray<FVector2D>());
				MeshSectionNormals.Add(sectionIndex, TArray<FVector>());
				MeshSectionVertexColors.Add(sectionIndex, TArray<FColor>());
				MeshSectionTangents.Add(sectionIndex, TArray<FProcMeshTangent>());
			}
		}
	}
}