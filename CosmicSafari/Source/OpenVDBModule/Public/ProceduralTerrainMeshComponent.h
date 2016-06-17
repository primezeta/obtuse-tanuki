// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "ProceduralMeshComponent.h"
#include "GridMeshBuffers.h"
#include "VoxelData.h"
#include "ProceduralTerrainMeshComponent.generated.h"

UENUM(BlueprintType)		//"BlueprintType" is essential to include
enum class EMeshType : uint8
{
	MESH_TYPE_CUBES 		 UMETA(DisplayName = "Mesh as basic cubes"),
	MESH_TYPE_MARCHING_CUBES UMETA(DisplayName = "Mesh with marching cubes"),
};

/**
 * 
 */
UCLASS()
class OPENVDBMODULE_API UProceduralTerrainMeshComponent : public UProceduralMeshComponent
{
	GENERATED_BODY()

public:
	UProceduralTerrainMeshComponent(const FObjectInitializer& ObjectInitializer);
	
	UPROPERTY()
		FString MeshID;
	UPROPERTY()
		bool IsGridSectionMeshed;
	UPROPERTY()
		bool CreateCollision;
	UPROPERTY()
		TMap<int32, EVoxelType> MeshTypes;
	UPROPERTY()
		FVector StartLocation;
	UPROPERTY()
		int32 SectionCount;
};
