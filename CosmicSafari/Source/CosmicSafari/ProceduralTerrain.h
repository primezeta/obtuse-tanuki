// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/Actor.h"
#include "ProceduralTerrainMeshComponent.h"
#include "ProceduralTerrain.generated.h"

UCLASS()
class COSMICSAFARI_API AProceduralTerrain : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AProceduralTerrain();

	// Called when the game starts
	virtual void BeginPlay() override;

	// Called every frame
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ProceduralTerrain")
	float MeshSurfaceValue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ProceduralTerrain")
	UProceduralTerrainMeshComponent * TerrainMeshComponent;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ProceduralTerrain")
	UMaterial * TerrainMaterial;

private:
	TArray<FVector> MeshSectionVertices;
	TArray<int32> MeshSectionTriangleIndices;
	TArray<FVector2D> MeshSectionUVMap;
	TArray<FVector> MeshSectionNormals;
	TArray<FColor> MeshSectionVertexColors;
	TArray<FProcMeshTangent> MeshSectionTangents;
	UMaterialInstanceDynamic * TerrainDynamicMaterial;
		
};