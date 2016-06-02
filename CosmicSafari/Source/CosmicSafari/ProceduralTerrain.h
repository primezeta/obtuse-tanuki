// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/PlayerStart.h"
#include "VDBHandle.h"
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

	UPROPERTY(BlueprintReadOnly, Category = "VDB Handle", Meta = (ToolTip = "Terrain mesh component of each grid"))
		TArray<UProceduralTerrainMeshComponent*> TerrainMeshComponents;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Procedural Terrain")
		FString VolumeName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Procedural Terrain")
		bool bCreateCollision;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Meta = (ToolTip = "Voxels per dimension of a meshable terrain region"))
		FIntVector RegionDimensions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Meta = (ToolTip = "Dimensions of a single voxel"))
		FVector VoxelSize;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Meta = (ToolTip = "Material to apply to each terrain component"))
		UMaterial * Material;

	UFUNCTION()
		void AddTerrainComponent(const FString &name, const FIntVector &gridIndex);
};