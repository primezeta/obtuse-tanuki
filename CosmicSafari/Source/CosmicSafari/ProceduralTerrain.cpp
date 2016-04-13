// Fill out your copyright notice in the Description page of Project Settings.

#include "CosmicSafari.h"
#include "ProceduralTerrain.h"

// Sets default values
AProceduralTerrain::AProceduralTerrain(const FObjectInitializer& ObjectInitializer)
{
	VdbHandle = ObjectInitializer.CreateDefaultSubobject<UVdbHandle>(this, TEXT("VDB Handle"));
	check(VdbHandle != nullptr);
	TerrainMeshComponent = ObjectInitializer.CreateDefaultSubobject<UProceduralTerrainMeshComponent>(this, TEXT("GeneratedTerrain"));
	check(TerrainMeshComponent != nullptr);

	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	SetActorEnableCollision(true);
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
	VdbHandle->InitVdb();
}

// Called when the game starts or when spawned
void AProceduralTerrain::BeginPlay()
{
	Super::BeginPlay();

	ACharacter* FirstPlayerCharacter = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
	FIntVector RegionIndexCoords(0,0,0);
	if (FirstPlayerCharacter != nullptr)
	{
		RegionIndexCoords = VdbHandle->GetRegionIndex(FirstPlayerCharacter->GetActorLocation());
	}
	FIntVector IndexStart;
	FIntVector IndexEnd;
	FString GridID = VdbHandle->AddGrid(TEXT("StartRegion"), RegionIndexCoords, IndexStart, IndexEnd);
	VdbHandle->ReadGridTreeIndex(GridID, IndexStart, IndexEnd);

	MeshSectionIndices.Add(0);
	MeshSectionIDs.Add(0, GridID);
	IsGridSectionMeshed.Add(0, false);
	MeshSectionVertices.Add(0, TArray<FVector>());
	MeshSectionPolygons.Add(0, TArray<int32>());
	MeshSectionUVMap.Add(0, TArray<FVector2D>());
	MeshSectionNormals.Add(0, TArray<FVector>());
	MeshSectionVertexColors.Add(0, TArray<FColor>());
	MeshSectionTangents.Add(0, TArray<FProcMeshTangent>());

	for (auto i = MeshSectionIndices.CreateConstIterator(); i; ++i)
	{
		const int32 &sectionIndex = *i;
		if (!IsGridSectionMeshed[sectionIndex])
		{
			VdbHandle->MeshGrid(MeshSectionIDs[sectionIndex], MeshSurfaceValue, MeshSectionVertices[sectionIndex], MeshSectionPolygons[sectionIndex], MeshSectionNormals[sectionIndex]);
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