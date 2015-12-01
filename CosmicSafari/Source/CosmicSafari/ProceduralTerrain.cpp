// Fill out your copyright notice in the Description page of Project Settings.

#include "CosmicSafari.h"
#include "OpenVDBModule/Public/OpenVDBModule.h"
#include <string>
#include "ProceduralTerrain.h"

// Sets default values
UProceduralTerrain::UProceduralTerrain()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	bWantsBeginPlay = true;
	PrimaryComponentTick.bCanEverTick = true;
	// ...
}

// Called when the game starts or when spawned
void UProceduralTerrain::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void UProceduralTerrain::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

bool UProceduralTerrain::LoadVdbFile(FString vdbFilename, FString gridName)
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

bool UProceduralTerrain::GetNextMeshVertex(FVector &vertex)
{
	bool empty = Vertices.IsEmpty();
	if (!empty)
	{
		Vertices.Dequeue(vertex);
	}
	return empty;
}

bool UProceduralTerrain::GetNextTriangleIndex(int32 &index)
{
	bool empty = TriangleIndices.IsEmpty();
	if (!empty)
	{
		uint32_t testIndex = 0;
		TriangleIndices.Dequeue(testIndex);
		if (int32(testIndex) < 0)
		{
			UE_LOG(LogFlying, Fatal, TEXT("Triangle index too large!"));
		}
	}
	return empty;
}