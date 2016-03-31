// Fill out your copyright notice in the Description page of Project Settings.

#include "CosmicSafari.h"
#include "ProceduralTerrain.h"

FOpenVDBModule * AProceduralTerrain::OpenVDBModule = nullptr;

void AProceduralTerrain::InitializeOpenVDBModule()
{
	if (OpenVDBModule == nullptr)
	{
		OpenVDBModule = &FOpenVDBModule::Get();
		if (!OpenVDBModule->IsAvailable())
		{
			UE_LOG(LogFlying, Warning, TEXT("Failed to start OpenVDBModule!"));
		}
		else
		{
			OpenVDBModule->StartupModule();
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
	
	//MeshSectionIndices.Add(0);
	//IsGridSectionMeshed.Add(0, false);
	//MeshSectionVertices.Add(0, TArray<FVector>());
	//MeshSectionPolygons.Add(0, TArray<int32>());
	//MeshSectionUVMap.Add(0, TArray<FVector2D>());
	//MeshSectionNormals.Add(0, TArray<FVector>());
	//MeshSectionVertexColors.Add(0, TArray<FColor>());
	//MeshSectionTangents.Add(0, TArray<FProcMeshTangent>());

	//Vdb::HandleType handle = OpenVDBModule->CreateVDB(VDBLocation, true, true);
	//VDBHandles.Add(0, handle);

	//Vdb::UniformScaleTransformType scaleXform;
	//scaleXform.Scale = scaleXYZ;
	//OpenVDBModule->InitializeGrid(handle, VolumeName, scaleXform);

	//FString regionName = "all";
	//FString regionID = OpenVDBModule->AddRegionDefinition(handle, VolumeName, regionName, MapBoundsStart, FIntVector(MapBoundsEnd.X, MapBoundsEnd.Y, HeightMapRange));
	//MeshSectionIDs[0] = regionID;
	//OpenVDBModule->PopulateRegionDensity_Perlin(handle, regionID, frequency, lacunarity, persistence, octaveCount);
	//OpenVDBModule->LoadRegion(handle, regionID, MeshSectionVertices[0], MeshSectionPolygons[0], MeshSectionNormals[0]);
}

// Called when the game starts or when spawned
void AProceduralTerrain::BeginPlay()
{
	Super::BeginPlay();

	//for (auto i = MeshSectionIndices.CreateConstIterator(); i; ++i)
	//{
	//	const int32 &sectionIndex = *i;
	//	if (!IsGridSectionMeshed[sectionIndex])
	//	{
	//		OpenVDBModule->MeshRegion(VDBHandles[sectionIndex], MeshSectionIDs[sectionIndex], MeshSurfaceValue);
	//		TerrainMeshComponent->CreateTerrainMeshSection(*i, bCreateCollision,
	//			MeshSectionVertices[sectionIndex],
	//			MeshSectionPolygons[sectionIndex],
	//			MeshSectionUVMap[sectionIndex],
	//			MeshSectionNormals[sectionIndex],
	//			MeshSectionVertexColors[sectionIndex],
	//			MeshSectionTangents[sectionIndex]);
	//		TerrainMeshComponent->SetMeshSectionVisible(sectionIndex, true);
	//		IsGridSectionMeshed[sectionIndex] = true;
	//	}
	//}
}

// Called every frame
void AProceduralTerrain::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AProceduralTerrain::PostEditChangeProperty(struct FPropertyChangedEvent& e)
{
	FName PropertyName = (e.Property != NULL) ? e.Property->GetFName() : NAME_None;
	if (PropertyName == GET_MEMBER_NAME_CHECKED(AProceduralTerrain, RegionCount))
	{
		if (RegionCount.X < 1)
		{
			RegionCount.X = 1;
		}
		if (RegionCount.Y < 1)
		{
			RegionCount.Y = 1;
		}
		if (RegionCount.Z < 1)
		{
			RegionCount.Z = 1;
		}
	}
	Super::PostEditChangeProperty(e);
}