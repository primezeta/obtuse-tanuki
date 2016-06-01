// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "ProceduralMeshComponent.h"
#include "ProceduralTerrainMeshComponent.generated.h"

typedef TArray<FVector> VertexBufferType;
typedef TArray<int32> PolygonBufferType;
typedef TArray<FVector> NormalBufferType;
typedef TArray<FVector2D> UVMapBufferType;
typedef TArray<FColor> VertexColorBufferType;
typedef TArray<FProcMeshTangent> TangentBufferType;

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
	
	FString MeshName;
	FString MeshID;
	bool IsGridSectionMeshed;
	bool CreateCollision;
	int32 SectionIndex;
};
