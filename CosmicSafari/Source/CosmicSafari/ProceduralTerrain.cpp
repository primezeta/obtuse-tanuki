// Fill out your copyright notice in the Description page of Project Settings.

#include "CosmicSafari.h"
#include "OpenVDBModule/Public/OpenVDBModule.h"
#include "ProceduralTerrain.h"

// Sets default values
AProceduralTerrain::AProceduralTerrain()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	this->SetActorScale3D(FVector(10.0f, 10.0f, 10.0f));
	//Just to give something to see, setup a basic sphere as the root component. The procedural terrain mesh will be attached to this sphere
	USphereComponent* SphereComponent = CreateDefaultSubobject<USphereComponent>(TEXT("RootComponent"));
	this->RootComponent = SphereComponent;
	SphereComponent->InitSphereRadius(0.000f);
	SphereComponent->SetCollisionProfileName(TEXT("Pawn")); //Don't know what the collision profile name does. Got this from a tutorial

	//Visual mesh of the sphere
	UStaticMeshComponent * SphereVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AProceduralTerrain.RootComponent.Mesh"));
	SphereVisual->AttachTo(RootComponent);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereVisualAsset(TEXT("StaticMesh'/Engine/EngineMeshes/Sphere.Sphere'"));

	if (SphereVisualAsset.Succeeded())
	{
		SphereVisual->SetStaticMesh(SphereVisualAsset.Object);
		SphereVisual->SetRelativeLocation(FVector(0.0f));
		SphereVisual->SetWorldScale3D(FVector(0.1f));
	}

	TerrainMeshComponent = CreateDefaultSubobject<UProceduralTerrainMeshComponent>(TEXT("GeneratedTerrain"));
	TerrainMeshComponent->AttachTo(SphereComponent);
	TerrainMeshComponent->SetRelativeLocation(FVector(0.0f));
	TerrainMeshComponent->SetWorldScale3D(FVector(1.0f));

	//static ConstructorHelpers::FObjectFinder<UMaterial> TerrainMaterialObject(TEXT("Material'/Engine/EngineMaterials/DefaultDeferredDecalMaterial.DefaultDeferredDecalMaterial'"));
	//if (TerrainMaterialObject.Succeeded())
	//{
	//	TerrainMaterial = (UMaterial*)TerrainMaterialObject.Object;
	//	TerrainDynamicMaterial = UMaterialInstanceDynamic::Create(TerrainMaterial, this);
	//	TerrainMeshComponent->SetMaterial(0, TerrainDynamicMaterial);
	//}
}

// Called when the game starts or when spawned
void AProceduralTerrain::BeginPlay()
{
	Super::BeginPlay();

	//C:/Users/zach/Documents/Unreal Projects/obtuse-tanuki/CosmicSafari/ThirdParty/Build/x64/Release/vdbs/noise_w3008_h3008_l3008_t16_s1_t1.vdb;
	//C:/Users/zach/Documents/Unreal Projects/obtuse-tanuki/CosmicSafari/ThirdParty/Build/x64/Release/vdbs/noise_w288_h288_l288_t16_s1_t1.vdb;
	//C:/Users/zach/Documents/Unreal Projects/obtuse-tanuki/CosmicSafari/ThirdParty/Build/x64/Release/vdbs/noise_w100_h100_l100_t10_s1_t0.vdb;
	//C:/Users/zach/Documents/Unreal Projects/obtuse-tanuki/CosmicSafari/ThirdParty/Build/x64/Debug/vdbs/noise_w288_h288_l288_t16_s1_t0.vdb;
	//C:/Users/zach/Documents/Unreal Projects/obtuse-tanuki/CosmicSafari/ThirdParty/Build/x64/Release/vdbs/noise_w512_h512_l512_t8_s1_t1.vdb;
	FString vdbFilename = TEXT("C:/Users/zach/Documents/Unreal Projects/obtuse-tanuki/CosmicSafari/ThirdParty/Build/x64/Release/vdbs/noise-X200-Y200-Z10_scale1.vdb");
	FString gridName = TEXT("noise");
	if (!LoadVdbFile(vdbFilename, gridName))
	{
		UE_LOG(LogFlying, Warning, TEXT("%s %s %s %s"), TEXT("Failed to load"), *vdbFilename, TEXT(", grid"), *gridName);
	}
	else
	{
		int32 meshSectionIndex = 0;
		TerrainMeshComponent->CreateTerrainMeshSection(meshSectionIndex, MeshSectionVertices, MeshSectionTriangleIndices);
		TerrainMeshComponent->SetMeshSectionVisible(meshSectionIndex, true);
	}
}

// Called every frame
void AProceduralTerrain::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

bool AProceduralTerrain::LoadVdbFile(const FString &vdbFilename, const FString &gridName)
{
	//Note: Blueprints do not currently support double
	static FOpenVDBModule * openVDBModule = nullptr;

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
	
	static bool isLoaded = false;
	//Read the file once
	if (!isLoaded)
	{
		isLoaded = openVDBModule->LoadVdbFile(vdbFilename, gridName);
		if (!isLoaded)
		{
			UE_LOG(LogFlying, Warning, TEXT("VDB file %s %s"), *vdbFilename, TEXT("unable to load"));
		}
	}
	MeshIsovalue = 0.0f;
	MeshAdaptivity = 0.0f;
	return isLoaded && openVDBModule->GetVDBGeometry((double)MeshIsovalue, (double)MeshAdaptivity, MeshSectionVertices, MeshSectionTriangleIndices);
}