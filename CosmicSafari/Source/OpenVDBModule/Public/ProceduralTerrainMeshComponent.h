// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "ProceduralMeshComponent.h"
#include "ProceduralTerrainMeshComponent.generated.h"

typedef TArray<TArray<FVector>> VertexBufferType;
typedef TArray<TArray<int32>> PolygonBufferType;
typedef TArray<TArray<FVector>> NormalBufferType;
typedef TArray<TArray<FVector2D>> UVMapBufferType;
typedef TArray<TArray<FColor>> VertexColorBufferType;
typedef TArray<TArray<FProcMeshTangent>> TangentBufferType;

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

	void CreateTerrainMeshSection(int32 SectionIndex);
	void UpdateTerrainMeshSection(int32 SectionIndex);
	
	FString MeshName;
	FString MeshID;
	bool IsGridSectionMeshed;
	bool CreateCollision;
	TSharedPtr<VertexBufferType> VertexBufferPtrs;
	TSharedPtr<PolygonBufferType> PolygonBufferPtrs;
	TSharedPtr<NormalBufferType> NormalBufferPtrs;
	TSharedPtr<UVMapBufferType> UVMapBufferPtrs;
	TSharedPtr<VertexColorBufferType> VertexColorsBufferPtrs;
	TSharedPtr<TangentBufferType> TangentsBufferPtrs;
};
