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
	UProceduralTerrainMeshComponent * TerrainMeshComponent;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ProceduralTerrain")
	UMaterial * TerrainMaterial;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ProceduralTerrain")
	float MeshSurfaceValue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ProceduralTerrain")
	FIntVector MapBounds; //TODO: Error check dim ranges since internally to openvdb they are unsigned

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ProceduralTerrain")
	FIntVector RegionCount;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ProceduralTerrain")
	int32 LibnoiseRange;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ProceduralTerrain")
	bool bCreateCollision;

	UFUNCTION()
	void CreateGridVolumes();

private:
	//SectionIDs to act as the master map. If it contains the grid ID, then the other maps must contain an item at that ID
	TMap<FString, bool> SectionIDs;
	TMap<FString, int32> MeshSectionIndices;
	TMap<FString, float> SurfaceIsovalues;
	TMap<FString, TArray<FVector>> MeshSectionVertices;
	TMap<FString, TArray<int32>> MeshSectionTriangleIndices;
	TMap<FString, TArray<FVector2D>> MeshSectionUVMap;
	TMap<FString, TArray<FVector>> MeshSectionNormals;
	TMap<FString, TArray<FColor>> MeshSectionVertexColors;
	TMap<FString, TArray<FProcMeshTangent>> MeshSectionTangents;
	UMaterialInstanceDynamic * TerrainDynamicMaterial;
};