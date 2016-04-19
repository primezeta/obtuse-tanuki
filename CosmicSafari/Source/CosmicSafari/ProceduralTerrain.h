// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/Actor.h"
#include "VDBHandle.h"
#include "ProceduralTerrainMeshComponent.h"
#include "ProceduralTerrain.generated.h"

UCLASS(Category = "OpenVDB|Procedural Terrain")
class COSMICSAFARI_API AProceduralTerrain : public AActor
{
	GENERATED_UCLASS_BODY()
	
public:
	virtual void PostInitializeComponents() override;

	// Called when the game starts
	virtual void BeginPlay() override;

	// Called every frame
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SimpleDisplay, Category = "OpenVDB|Procedural Terrain")
		UVdbHandle * VdbHandle;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "OpenVDB|Procedural Terrain")
		UProceduralTerrainMeshComponent * TerrainMeshComponent;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "OpenVDB|Procedural Terrain")
		UMaterial * TerrainMaterial;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "OpenVDB|Procedural Terrain")
		FString VolumeName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "OpenVDB|Procedural Terrain")
		float MeshSurfaceValue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "OpenVDB|Procedural Terrain")
		bool bCreateCollision;

private:
	TArray<int32> MeshSectionIndices;
	TMap<int32, FString> MeshSectionIDs;
	TMap<int32, bool> IsGridSectionMeshed;
	TArray<TArray<FVector>> MeshSectionVertices;
	TArray<TArray<int32>> MeshSectionPolygons;
	TArray<TArray<FVector2D>> MeshSectionUVMap;
	TArray<TArray<FVector>> MeshSectionNormals;
	TArray<TArray<FColor>> MeshSectionVertexColors;
	TArray<TArray<FProcMeshTangent>> MeshSectionTangents;
	//UMaterialInstanceDynamic * TerrainDynamicMaterial;
};