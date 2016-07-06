// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/PlayerStart.h"
#include "ProceduralTerrainMeshComponent.h"
#include "ProceduralTerrain.generated.h"

UCLASS(Category = "Procedural Terrain")
class COSMICSAFARI_API AProceduralTerrain : public APlayerStart
{
	GENERATED_BODY()
	
public:
	AProceduralTerrain(const FObjectInitializer& ObjectInitializer);

	virtual void PostInitializeComponents() override;

	// Called when the game starts
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Called every frame
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SimpleDisplay, Category = "Procedural Terrain")
		UVdbHandle * VdbHandle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VDB Handle", Meta = (ToolTip = "Mesh algorithm"))
		EMeshType MeshMethod;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VDB Handle", Meta = (ToolTip = "Path to voxel database"))
		FString FilePath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VDB Handle", Meta = (ToolTip = "Enable delayed loading of grids"))
		bool EnableDelayLoad;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VDB Handle", Meta = (ToolTip = "Enable grid stats metadata"))
		bool EnableGridStats;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VDB Handle", Meta = (ToolTip = "Name of volume"))
		FString WorldName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VDB Handle", Meta = (ToolTip = "Perlin noise random generator seed"))
		int32 PerlinSeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VDB Handle", Meta = (ToolTip = "Perlin noise frequency of first octave"))
		float PerlinFrequency;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VDB Handle", Meta = (ToolTip = "Perlin noise frequency multiplier each successive octave"))
		float PerlinLacunarity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VDB Handle", Meta = (ToolTip = "Perlin noise amplitude multiplier each successive octave"))
		float PerlinPersistence;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VDB Handle", Meta = (ToolTip = "Perlin noise number of octaves"))
		int32 PerlinOctaveCount;

	UPROPERTY(BlueprintReadOnly, Category = "VDB Handle", Meta = (ToolTip = "Terrain mesh component of each grid"))
		TArray<FString> GridRegions;
	//Terrain mesh per grid region that has a mesh section per voxel type (i.e. per material)
	TMap<FString, UProceduralTerrainMeshComponent*> TerrainMeshComponents;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Procedural Terrain")
		FString VolumeName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Procedural Terrain")
		bool bCreateCollision;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Meta = (ToolTip = "Voxels per dimension of a meshable terrain region"))
		FIntVector RegionDimensions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Meta = (ToolTip = "Dimensions of a single voxel"))
		FVector VoxelSize;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Meta = (ToolTip = "Material to apply according to material ID"))
		TArray<UMaterial*> MeshMaterials;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Name of the region on which the player will start play")
		FString StartRegion;

	UFUNCTION()
		FString AddTerrainComponent(const FIntVector &gridIndex);
};