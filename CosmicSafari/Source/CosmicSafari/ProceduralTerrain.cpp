// Fill out your copyright notice in the Description page of Project Settings.

#include "CosmicSafari.h"
#include "FirstPersonCPPCharacter.h"
#include "ProceduralTerrain.h"

// Sets default values
AProceduralTerrain::AProceduralTerrain(const FObjectInitializer& ObjectInitializer)
{
	UVdbHandle * VdbHandle = ObjectInitializer.CreateDefaultSubobject<UVdbHandle>(this, TEXT("VDBHandle"));
	check(VdbHandle != nullptr);
	TerrainMeshComponent = ObjectInitializer.CreateDefaultSubobject<UProceduralTerrainMeshComponent>(this, TEXT("GeneratedTerrain"));
	check(TerrainMeshComponent != nullptr);
	TerrainMeshComponent->bGenerateOverlapEvents = true;

	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	SetActorEnableCollision(true);
	//RootComponent = TerrainMeshComponent;
	TerrainMeshComponent->AttachTo(RootComponent);
	TerrainMeshComponent->SetWorldScale3D(FVector(1.0f, 1.0f, 1.0f));
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

	if (TerrainMeshComponent->bGenerateOverlapEvents)
	{
		TerrainMeshComponent->OnComponentBeginOverlap.AddDynamic(this, &AProceduralTerrain::OnOverlapBegin);
		TerrainMeshComponent->OnComponentEndOverlap.AddDynamic(this, &AProceduralTerrain::OnOverlapEnd);
	}

	VdbHandle->SetRegionScale(RegionDimensions);
	FString GridID = VdbHandle->AddGrid(TEXT("StartRegion"), FVector(0, 0, 0), VoxelSize);

	FIntVector StartFill;
	FIntVector EndFill;
	VdbHandle->ReadGridTree(GridID, StartFill, EndFill);

	int32 sectionIndex = 0;
	TArray<FString> AllGridIDs = VdbHandle->GetAllGridIDs();
	for (TArray<FString>::TConstIterator i = AllGridIDs.CreateConstIterator(); i; ++i)
	{
		TerrainMeshComponent->MeshSectionIndices.Add(sectionIndex);
		TerrainMeshComponent->MeshSectionIDs.Add(sectionIndex, *i);
		TerrainMeshComponent->IsGridSectionMeshed.Add(sectionIndex, false);
		sectionIndex++;
	}
}

// Called when the game starts or when spawned
void AProceduralTerrain::BeginPlay()
{
	Super::BeginPlay();

	for (auto i = TerrainMeshComponent->MeshSectionIndices.CreateConstIterator(); i; ++i)
	{
		const int32 &sectionIndex = *i;
		if (!TerrainMeshComponent->IsGridSectionMeshed[sectionIndex])
		{
			TSharedPtr<TArray<FVector>> VertexBufferPtr;
			TSharedPtr<TArray<int32>> PolygonBufferPtr;
			TSharedPtr<TArray<FVector>> NormalBufferPtr;
			TSharedPtr<TArray<FVector2D>> UVMapBufferPtr;
			TSharedPtr<TArray<FColor>> VertexColorsBufferPtr;
			TSharedPtr<TArray<FProcMeshTangent>> TangentsBufferPtr;
			FVector ActiveWorldStart;
			FVector ActiveWorldEnd;
			FVector StartLocation;
			VdbHandle->MeshGrid(TerrainMeshComponent->MeshSectionIDs[sectionIndex],
				                VertexBufferPtr,
				                PolygonBufferPtr,
				                NormalBufferPtr,
				                UVMapBufferPtr,
				                VertexColorsBufferPtr,
				                TangentsBufferPtr,
				                ActiveWorldStart,
				                ActiveWorldEnd,
				                StartLocation);
			TerrainMeshComponent->CreateTerrainMeshSection(
				sectionIndex,
				bCreateCollision,
				*VertexBufferPtr,
				*PolygonBufferPtr,
				*UVMapBufferPtr,
				*NormalBufferPtr,
				*VertexColorsBufferPtr,
				*TangentsBufferPtr);

			ACharacter* Character = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
			FVector PlayerLocation;
			if (Character)
			{
				PlayerLocation = Character->GetActorLocation();
			}
			FVector TerrainScale = TerrainMeshComponent->GetComponentScale();
			StartLocation = TerrainScale*(PlayerLocation - StartLocation);
			TerrainMeshComponent->SetWorldLocation(PlayerLocation);
			TerrainMeshComponent->SetRelativeLocationAndRotation(StartLocation, FRotator::ZeroRotator);
			TerrainMeshComponent->SetMeshSectionVisible(sectionIndex, true);
			TerrainMeshComponent->IsGridSectionMeshed[sectionIndex] = true;
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

void AProceduralTerrain::OnOverlapBegin(class AActor* OtherActor, class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	AFirstPersonCPPCharacter * PlayerActor = Cast<AFirstPersonCPPCharacter>(OtherActor);
	if (PlayerActor != nullptr)
	{
		static FIntVector PrevVoxelCoord;
		FIntVector VoxelCoord;
		VdbHandle->GetVoxelCoord(TerrainMeshComponent->MeshSectionIDs[0], SweepResult.Location, VoxelCoord);
		if (PrevVoxelCoord != VoxelCoord)
		{
			PrevVoxelCoord = VoxelCoord;
			UE_LOG(LogFlying, Display, TEXT("OnOverlapBegin %s"), *VoxelCoord.ToString());
		}
	}
}

void AProceduralTerrain::OnOverlapEnd(class AActor* OtherActor, class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	AFirstPersonCPPCharacter * PlayerActor = Cast<AFirstPersonCPPCharacter>(OtherActor);
	if (PlayerActor != nullptr)
	{

	}
}