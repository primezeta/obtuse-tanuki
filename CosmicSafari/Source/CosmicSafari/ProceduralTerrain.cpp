// Fill out your copyright notice in the Description page of Project Settings.

#include "CosmicSafari.h"
#include "OpenVDBModule/Public/OpenVDBModule.h"
#include "OvdbTypes.h"
#include "ProceduralTerrain.h"

static FOpenVDBModule * openVDBModule = nullptr;

// Sets default values
AProceduralTerrain::AProceduralTerrain()
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

void AProceduralTerrain::CreateGridVolumes()
{
	int32 meshSectionIndex = 0;
	if (SectionIDs.Num() < 1)
	{
		//Note: For now it is assumed that the region count evenly divides the map dimensions
		FIntVector regionDims(MapBounds.X / RegionCount.X, MapBounds.Y / RegionCount.Y, MapBounds.Z / RegionCount.Z);
		for (int32 x = 0; x < RegionCount.X; x++)
		{
			int32 startX = regionDims.X * x;
			int32 endX = regionDims.X * (x + 1) - 1;
			for (int32 y = 0; y < RegionCount.Y; y++)
			{
				int32 startY = regionDims.Y * y;
				int32 endY = regionDims.Y * (y + 1) - 1;
				for (int32 z = 0; z < RegionCount.Z; z++)
				{
					int32 startZ = regionDims.Z * z;
					int32 endZ = regionDims.Z * (z + 1) - 1;
					//For now make sure to always set the region count Z to 1.
					//Eventually multiple Z regions could be used through use of openvdb topology intersection.
					FIntVector boundsStart(startX, startY, startZ);
					FIntVector boundsEnd(endX, endY, endZ);
					float isovalue;
					FString regionID = openVDBModule->CreateDynamicVdb(MeshSurfaceValue, boundsStart, boundsEnd, (uint32)LibnoiseRange, isovalue); //TODO: Range check dimensions since internally they are unsigned
					if (regionID == TEXT(""))
					{
						UE_LOG(LogFlying, Fatal, TEXT("Dynamic grid is invalid!"));
					}
					SectionIDs.Add(regionID, false);
					MeshSectionIndices.Add(regionID, meshSectionIndex++);
					SurfaceIsovalues.Add(regionID, isovalue); //TODO: Possibly handle retention of isovalue in OpenVDBModule or libovdb?
					MeshSectionVertices.Add(regionID, TArray<FVector>());
					MeshSectionTriangleIndices.Add(regionID, TArray<int32>());
					MeshSectionUVMap.Add(regionID, TArray<FVector2D>());
					MeshSectionNormals.Add(regionID, TArray<FVector>());
					MeshSectionVertexColors.Add(regionID, TArray<FColor>());
					MeshSectionTangents.Add(regionID, TArray<FProcMeshTangent>());
				}
			}
		}
	}
}

// Called when the game starts or when spawned
void AProceduralTerrain::BeginPlay()
{
	Super::BeginPlay();
	CreateGridVolumes();
	for (TMap<FString, bool>::TConstIterator i = SectionIDs.CreateConstIterator(); i; ++i)
	{
		bool isSectionMeshed = i->Value;
		if (isSectionMeshed)
		{
			//This section is alread meshed, skip it
			continue;
		}

		FString regionID = i->Key;
		int32 * meshSectionIndex = MeshSectionIndices.Find(regionID);
		float * isovalue = SurfaceIsovalues.Find(regionID);
		TArray<FVector> * vertices = MeshSectionVertices.Find(regionID);
		TArray<int32> * indices = MeshSectionTriangleIndices.Find(regionID);
		TArray<FVector2D> * uvs = MeshSectionUVMap.Find(regionID);
		TArray<FVector> * normals = MeshSectionNormals.Find(regionID);
		TArray<FColor> * colors = MeshSectionVertexColors.Find(regionID);
		TArray<FProcMeshTangent> * tangents = MeshSectionTangents.Find(regionID);

		checkf(isovalue != nullptr, TEXT("ProceduralTerrain: null mesh section isovalue (mesh ID %d)"), regionID);
		checkf(vertices != nullptr, TEXT("ProceduralTerrain: null mesh section vertices (mesh ID %d)"), regionID);
		checkf(indices != nullptr, TEXT("ProceduralTerrain: null mesh section indices (mesh ID %d)"), regionID);
		checkf(uvs != nullptr, TEXT("ProceduralTerrain: null mesh section UVs (mesh ID %d)"), regionID);
		checkf(normals != nullptr, TEXT("ProceduralTerrain: null mesh section normals (mesh ID %d)"), regionID);
		checkf(colors != nullptr, TEXT("ProceduralTerrain: null mesh section colors (mesh ID %d)"), regionID);
		checkf(tangents != nullptr, TEXT("ProceduralTerrain: null mesh section tangents (mesh ID %d)"), regionID);

		if (openVDBModule->GetVDBMesh(regionID, *isovalue, *vertices, *indices, *normals))
		{
			TerrainMeshComponent->CreateTerrainMeshSection(*meshSectionIndex, bCreateCollision, *vertices, *indices, *uvs, *normals, *colors, *tangents);
			TerrainMeshComponent->SetMeshSectionVisible(*meshSectionIndex, true);
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