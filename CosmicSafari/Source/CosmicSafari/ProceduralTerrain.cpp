// Fill out your copyright notice in the Description page of Project Settings.

#include "CosmicSafari.h"
#include "OpenVDBModule/Public/OpenVDBModule.h"
#include "ProceduralTerrain.h"

// Sets default values
AProceduralTerrain::AProceduralTerrain()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void AProceduralTerrain::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void AProceduralTerrain::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

bool AProceduralTerrain::LoadVdbFile(FString vdbFilename, FString gridName)
{
	//Note: Blueprints do not currently support double
	static FOpenVDBModule * openVDBModule = nullptr;

	if (openVDBModule == nullptr)
	{
		openVDBModule = &FOpenVDBModule::Get();
		if (!openVDBModule->IsAvailable())
		{
			UE_LOG(LogFlying, Warning, TEXT("Failed to load OpenVDBModule!"));
			return false;
		}
		openVDBModule->StartupModule();
	}
	
	static bool isLoaded = false;
	//Read the file once
	if (!isLoaded)
	{
		isLoaded = openVDBModule->LoadVdbFile(vdbFilename, gridName);
	}

	bool geometryLoaded = false;
	if (isLoaded)
	{
		geometryLoaded = openVDBModule->GetVDBGeometry((double)MeshIsovalue, (double)MeshAdaptivity, Vertices, TriangleIndices);
	}
	else
	{
		UE_LOG(LogFlying, Warning, TEXT("%s %s %s %s"), TEXT("Failed to load"), *vdbFilename, TEXT(", grid"), *gridName);
	}
	return geometryLoaded;
}

bool AProceduralTerrain::GetNextMeshVertex(FVector &vertex)
{
	if (!Vertices.IsEmpty())
	{
		Vertices.Dequeue(vertex);
	}
	return !Vertices.IsEmpty();
}

bool AProceduralTerrain::GetNextTriangleIndex(int32 &index)
{
	if (!TriangleIndices.IsEmpty())
	{
		uint32_t testIndex = 0;
		TriangleIndices.Dequeue(testIndex);
		if (int32(testIndex) < 0)
		{
			UE_LOG(LogFlying, Fatal, TEXT("Triangle index too large!"));
		}
		index = (int32)testIndex;
	}
	return !TriangleIndices.IsEmpty();
}