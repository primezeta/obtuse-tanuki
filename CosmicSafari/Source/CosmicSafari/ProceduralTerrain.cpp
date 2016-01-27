// Fill out your copyright notice in the Description page of Project Settings.

#include "CosmicSafari.h"
#include "OpenVDBModule/Public/OpenVDBModule.h"
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
	TerrainMeshComponent = CreateDefaultSubobject<UProceduralTerrainMeshComponent>(TEXT("GeneratedTerrain"));
	//RootComponent = TerrainMeshComponent;
	TerrainMeshComponent->AttachTo(RootComponent);
	TerrainMeshComponent->SetWorldScale3D(FVector(100.0f, 100.0f, 100.0f));
	TerrainMeshComponent->SetMaterial(0, TerrainMaterial);

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

// Called when the game starts or when spawned
void AProceduralTerrain::BeginPlay()
{
	Super::BeginPlay();

	//C:/Users/zach/Documents/Unreal Projects/obtuse-tanuki/CosmicSafari/ThirdParty/Build/x64/Release/vdbs/noise_w3008_h3008_l3008_t16_s1_t1.vdb;
	//C:/Users/zach/Documents/Unreal Projects/obtuse-tanuki/CosmicSafari/ThirdParty/Build/x64/Release/vdbs/noise_w288_h288_l288_t16_s1_t1.vdb;
	//C:/Users/zach/Documents/Unreal Projects/obtuse-tanuki/CosmicSafari/ThirdParty/Build/x64/Release/vdbs/noise_w100_h100_l100_t10_s1_t0.vdb;
	//C:/Users/zach/Documents/Unreal Projects/obtuse-tanuki/CosmicSafari/ThirdParty/Build/x64/Debug/vdbs/noise_w288_h288_l288_t16_s1_t0.vdb;
	//C:/Users/zach/Documents/Unreal Projects/obtuse-tanuki/CosmicSafari/ThirdParty/Build/x64/Release/vdbs/noise_w512_h512_l512_t8_s1_t1.vdb;
	//FString vdbFilename = TEXT("C:/Users/zach/Documents/Unreal Projects/obtuse-tanuki/CosmicSafari/ThirdParty/Build/x64/Debug/vdbs/noise-X199-Y199-Z10_scale1.vdb");
	//FString gridName = TEXT("noise");
	//if (!openVDBModule->GetVDBGeometry(vdbFilename, gridName, MeshSurfaceValue, MeshSectionVertices, MeshSectionTriangleIndices, MeshSectionNormals))
	//{
	//	UE_LOG(LogFlying, Warning, TEXT("%s %s %s %s"), TEXT("Failed to load"), *vdbFilename, TEXT(", grid"), *gridName);
	//}

	//uint32 FOpenVDBModule::CreateDynamicVdb(float surfaceValue, uint32_t dimX, uint32_t dimY, uint32_t dimZ, float &isovalue)
	float isovalue;
	uint32_t gridID = openVDBModule->CreateDynamicVdb(MeshSurfaceValue, (uint32)MeshDimX, (uint32)MeshDimY, (uint32)MeshDimZ, isovalue); //TODO: Range check dimensions since internally they are unsigned
	if (gridID != UINT32_MAX)
	{
		//uint32 gridID, OvdbMeshMethod method, float surfaceValue, TArray<FVector> &Vertices, TArray<int32> &TriangleIndices, TArray<FVector> &Normals
		if (openVDBModule->GetVDBMesh(gridID, isovalue, MeshSectionVertices, MeshSectionTriangleIndices, MeshSectionNormals))
		{
			int32 meshSectionIndex = 0;
			bool bCreateCollision = false;
			TerrainMeshComponent->CreateTerrainMeshSection(meshSectionIndex, bCreateCollision, MeshSectionVertices, MeshSectionTriangleIndices, MeshSectionUVMap, MeshSectionNormals, MeshSectionVertexColors, MeshSectionTangents);
			TerrainMeshComponent->SetMeshSectionVisible(meshSectionIndex, true);
		}
		else
		{
			UE_LOG(LogFlying, Fatal, TEXT("Failed to load vdb geometry!"));
		}
	}
	else
	{
		UE_LOG(LogFlying, Fatal, TEXT("Dynamic grid is invalid!"));
	}
}

// Called every frame
void AProceduralTerrain::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}