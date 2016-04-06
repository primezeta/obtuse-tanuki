// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/Actor.h"
#include "OpenVDBModule.h"
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
	static FOpenVDBModule * OpenVDBModule;
	static void InitializeOpenVDBModule();

	TArray<int32> MeshSectionIndices;
	TMap<int32, FString> MeshSectionIDs;
	TMap<int32, bool> IsGridSectionMeshed;
	TMap<int32, TArray<FVector>> MeshSectionVertices;
	TMap<int32, TArray<int32>> MeshSectionPolygons;
	TMap<int32, TArray<FVector2D>> MeshSectionUVMap;
	TMap<int32, TArray<FVector>> MeshSectionNormals;
	TMap<int32, TArray<FColor>> MeshSectionVertexColors;
	TMap<int32, TArray<FProcMeshTangent>> MeshSectionTangents;
	//UMaterialInstanceDynamic * TerrainDynamicMaterial;
};