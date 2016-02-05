// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/Actor.h"
#include "OpenVDBModule/Public/OpenVDBModule.h"
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

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ProceduralTerrain")
	UProceduralTerrainMeshComponent * TerrainMeshComponent;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ProceduralTerrain")
	UMaterial * TerrainMaterial;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ProceduralTerrain")
	FString VolumeName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ProceduralTerrain")
	float MeshSurfaceValue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ProceduralTerrain")
	FIntVector MapBoundsStart; //TODO: Error check dim ranges since internally to openvdb they are unsigned

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ProceduralTerrain")
	FIntVector MapBoundsEnd; //TODO: Error check dim ranges since internally to openvdb they are unsigned

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ProceduralTerrain")
	FIntVector RegionCount;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ProceduralTerrain")
	int32 HeightMapRange;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ProceduralTerrain")
	bool bCreateCollision;

private:
	static FOpenVDBModule * openVDBModule;
	static void InitializeOpenVDBModule();

	void InitializeMeshSections();
	FString GridID;
	float GridIsoValue;
	TArray<int32> MeshSectionIndices;
	TMap<int32, bool> IsGridSectionMeshed;
	TMap<int32, FIntVector> MeshSectionStart;
	TMap<int32, FIntVector> MeshSectionEnd;
	TMap<int32, TArray<FVector>> MeshSectionVertices;
	TMap<int32, TArray<int32>> MeshSectionTriangleIndices;
	TMap<int32, TArray<FVector2D>> MeshSectionUVMap;
	TMap<int32, TArray<FVector>> MeshSectionNormals;
	TMap<int32, TArray<FColor>> MeshSectionVertexColors;
	TMap<int32, TArray<FProcMeshTangent>> MeshSectionTangents;
	UMaterialInstanceDynamic * TerrainDynamicMaterial;
};