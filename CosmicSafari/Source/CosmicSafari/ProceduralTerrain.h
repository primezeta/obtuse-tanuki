// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/Actor.h"
#include "OpenVDBModule.h"
#include "ProceduralTerrainMeshComponent.h"
#include "ProceduralTerrain.generated.h"

UCLASS()
class COSMICSAFARI_API AProceduralTerrain : public AActor
{
	GENERATED_BODY()
	
public:
	// Sets default values for this actor's properties
	AProceduralTerrain();

	virtual void PostInitializeComponents() override;

	// Called when the game starts
	virtual void BeginPlay() override;

	// Called every frame
	virtual void Tick(float DeltaSeconds) override;

	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& e) override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Procedural Terrain")
	UVDBHandle * VDBHandle;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Procedural Terrain")
	UProceduralTerrainMeshComponent * TerrainMeshComponent;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Procedural Terrain")
	UMaterial * TerrainMaterial;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Procedural Terrain")
	FString VolumeName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Procedural Terrain")
	float MeshSurfaceValue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Procedural Terrain")
	float scaleXYZ;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Procedural Terrain")
	float frequency;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Procedural Terrain")
	float lacunarity;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Procedural Terrain")
	float persistence;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Procedural Terrain")
	int32 octaveCount;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Procedural Terrain")
	FIntVector MapBoundsStart; //TODO: Error check dim ranges since internally to openvdb they are unsigned

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Procedural Terrain")
	FIntVector MapBoundsEnd; //TODO: Error check dim ranges since internally to openvdb they are unsigned

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Procedural Terrain")
	FIntVector RegionCount;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Procedural Terrain")
	int32 HeightMapRange;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Procedural Terrain")
	bool bCreateCollision;

private:
	static FOpenVDBModule * OpenVDBModule;
	static void InitializeOpenVDBModule();

	//TArray<int32> MeshSectionIndices;
	//TMap<int32, Vdb::HandleType> VDBHandles;
	//TMap<int32, FString> MeshSectionIDs;
	//TMap<int32, bool> IsGridSectionMeshed;
	//TMap<int32, TArray<FVector>> MeshSectionVertices;
	//TMap<int32, TArray<int32>> MeshSectionPolygons;
	//TMap<int32, TArray<FVector2D>> MeshSectionUVMap;
	//TMap<int32, TArray<FVector>> MeshSectionNormals;
	//TMap<int32, TArray<FColor>> MeshSectionVertexColors;
	//TMap<int32, TArray<FProcMeshTangent>> MeshSectionTangents;
	//UMaterialInstanceDynamic * TerrainDynamicMaterial;
};