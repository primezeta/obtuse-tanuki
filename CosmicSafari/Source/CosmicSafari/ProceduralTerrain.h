// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/PlayerStart.h"
#include "ProceduralTerrainMeshComponent.h"
#include "ProceduralTerrain.generated.h"

DECLARE_DELEGATE_OneParam(FEarliestGridState, EGridState);

UCLASS(Category = "Procedural Terrain")
class COSMICSAFARI_API AProceduralTerrain : public AActor
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

	UPROPERTY(BlueprintReadOnly, Category = "Voxel database configuration", Meta=(DisplayName="Voxel Database", ToolTip="Configure VBD properties"))
		UVdbHandle * VdbHandle;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Procedural Terrain", Meta = (ToolTip = "Terrain volume name"))
		FString VolumeName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Procedural Terrain", Meta = (ToolTip = "Enable or disable dynamic mesh collision calculation"))
		bool bCreateCollision;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Terrain", Meta = (ToolTip = "Voxels per dimension of a meshable terrain region"))
		FIntVector RegionDimensions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Terrain", Meta = (ToolTip = "Dimensions of a single voxel"))
		FVector VoxelSize;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Terrain", Meta = (ToolTip = "Material to apply according to material ID"))
		TArray<UMaterial*> MeshMaterials;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Procedural Terrain", Meta = (ToolTip = "Name of the region on which the player will start play"))
		FString StartRegion;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Procedural Terrain", Meta = (ToolTip = "Number of regions padding the start region (x-axis)"))
		int32 RegionRadiusX;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Procedural Terrain", Meta = (ToolTip = "Number of regions padding the start region (y-axis)"))
		int32 RegionRadiusY;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Procedural Terrain", Meta = (ToolTip = "Number of regions padding the start region (z-axis)"))
		int32 RegionRadiusZ;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Procedural Terrain", Meta = (ToolTip = "Number of mesh creation steps remaining"))
		int32 NumberRegionsComplete;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Procedural Terrain", Meta = (ToolTip = "Number of mesh creation steps remaining"))
		int32 NumberMeshingStatesRemaining;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Procedural Terrain", Meta = (ToolTip = "Percent (0.0-100.0) mesh creation that is completed among all mesh sections"))
		float PercentMeshingComplete;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Procedural Terrain", Meta = (ToolTip = "Terrain mesh components per grid region"))
		TArray<UProceduralTerrainMeshComponent*> TerrainMeshComponents;

	UFUNCTION()
		FString AddTerrainComponent(const FIntVector &gridIndex);

	UFUNCTION()
		UProceduralTerrainMeshComponent * GetTerrainComponent(const FIntVector &gridIndex);

	EGridState OldestGridState;

private:
	TQueue<UProceduralTerrainMeshComponent*, EQueueMode::Mpsc> DirtyGridRegions;
	int32 NumberTotalGridStates;
	//Terrain mesh per grid region that has a mesh section per voxel type (i.e. per material)
	TSharedPtr<FGridMeshingThread> GridMeshingThread;
	int32 EnqueueOrFinishSection(UProceduralTerrainMeshComponent *terrainMeshComponentPtr);
};