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

	VoxelSize = FVector(1.0f, 1.0f, 1.0f);
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

	VdbHandle->SetRegionScale(RegionDimensions);

	ACharacter* FirstPlayerCharacter = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
	FVector PlayerLocation = FVector(0,0,0);
	if (FirstPlayerCharacter != nullptr)
	{
		PlayerLocation = FirstPlayerCharacter->GetActorLocation();
	}
	FString GridID = VdbHandle->AddGrid(TEXT("StartRegion"), PlayerLocation, VoxelSize);

	FIntVector StartFill;
	FIntVector EndFill;
	FIntVector ActiveStart;
	FIntVector ActiveEnd;
	VdbHandle->ReadGridTreeIndex(GridID, StartFill, EndFill, ActiveStart, ActiveEnd);

	MeshSectionIndices.Add(0);
	MeshSectionIDs.Add(0, GridID);
	IsGridSectionMeshed.Add(0, false);

	for (auto i = MeshSectionIndices.CreateConstIterator(); i; ++i)
	{
		const int32 &sectionIndex = *i;
		if (!IsGridSectionMeshed[sectionIndex])
		{
			TSharedPtr<TArray<FVector>> VertexBufferPtr;
			TSharedPtr<TArray<int32>> PolygonBufferPtr;
			TSharedPtr<TArray<FVector>> NormalBufferPtr;
			TSharedPtr<TArray<FVector2D>> UVMapBufferPtr;
			TSharedPtr<TArray<FColor>> VertexColorsBufferPtr;
			TSharedPtr<TArray<FProcMeshTangent>> TangentsBufferPtr;
			VdbHandle->MeshGrid(MeshSectionIDs[sectionIndex], MeshSurfaceValue, VertexBufferPtr, PolygonBufferPtr, NormalBufferPtr, UVMapBufferPtr, VertexColorsBufferPtr, TangentsBufferPtr);
			TerrainMeshComponent->CreateTerrainMeshSection(
				sectionIndex,
				bCreateCollision,
				*VertexBufferPtr,
				*PolygonBufferPtr,
				*UVMapBufferPtr,
				*NormalBufferPtr,
				*VertexColorsBufferPtr,
				*TangentsBufferPtr);
			TerrainMeshComponent->SetMeshSectionVisible(sectionIndex, true);
			IsGridSectionMeshed[sectionIndex] = true;
		}
	}
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