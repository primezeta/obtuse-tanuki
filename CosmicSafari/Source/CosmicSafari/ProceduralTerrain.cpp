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
	OpenVDBModule->InitializeVDB(VDBLocation, VolumeName);
	FString regionID = "all";
	OpenVDBModule->CreateRegion(VolumeName, regionID, MapBoundsStart, FIntVector(MapBoundsEnd.X, MapBoundsEnd.Y, HeightMapRange));
	MeshSectionIndices.Add(0);
	MeshSectionIDs.Add(0, regionID);
	OpenVDBModule->LoadRegion(regionID);
	OpenVDBModule->FillRegionWithPerlinDensity(regionID, scaleXYZ, frequency, lacunarity, persistence, octaveCount);
	OpenVDBModule->GenerateMesh(regionID, MeshSurfaceValue);

	for (auto i = MeshSectionIndices.CreateConstIterator(); i; ++i)
	{
		int32 sectionIndex = *i;
		IsGridSectionMeshed.Add(sectionIndex, false);
		MeshSectionVertices.Add(sectionIndex, TArray<FVector>());
		MeshSectionPolygons.Add(sectionIndex, TArray<int32>());
		MeshSectionUVMap.Add(sectionIndex, TArray<FVector2D>());
		MeshSectionNormals.Add(sectionIndex, TArray<FVector>());
		MeshSectionVertexColors.Add(sectionIndex, TArray<FColor>());
		MeshSectionTangents.Add(sectionIndex, TArray<FProcMeshTangent>());
	}
}

// Called when the game starts or when spawned
void AProceduralTerrain::BeginPlay()
{
	Super::BeginPlay();

	for (auto i = MeshSectionIndices.CreateConstIterator(); i; ++i)
	{
		const int32 &sectionIndex = *i;
		if (!IsGridSectionMeshed[sectionIndex])
		{
			OpenVDBModule->GetMeshGeometry(MeshSectionIDs[sectionIndex], MeshSectionVertices[sectionIndex], MeshSectionPolygons[sectionIndex], MeshSectionNormals[sectionIndex]);
			TerrainMeshComponent->CreateTerrainMeshSection(*i, bCreateCollision,
				MeshSectionVertices[sectionIndex],
				MeshSectionPolygons[sectionIndex],
				MeshSectionUVMap[sectionIndex],
				MeshSectionNormals[sectionIndex],
				MeshSectionVertexColors[sectionIndex],
				MeshSectionTangents[sectionIndex]);
			TerrainMeshComponent->SetMeshSectionVisible(sectionIndex, true);
			IsGridSectionMeshed[sectionIndex] = true;
		}
	}
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