// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "ProceduralMeshComponent_Async.h"
#include "VoxelData.h"
#include "VDBHandle.h"
#include "ProceduralTerrainMeshComponent.generated.h"

/**
 * 
 */
UCLASS()
class OPENVDBMODULE_API UProceduralTerrainMeshComponent : public UProceduralMeshComponent_Async
{
	GENERATED_BODY()

public:
	UPROPERTY()
		FString MeshID;
	UPROPERTY()
		EGridState RegionState;
	UPROPERTY()
		bool IsTreeReady;
	UPROPERTY()
		bool IsGridDirty;
	UPROPERTY()
		int32 NumReadySections;
	UPROPERTY()
		int32 IsSectionFinished[FVoxelData::VOXEL_TYPE_COUNT];
	UPROPERTY()
		bool CreateCollision;
	UPROPERTY()
		TMap<int32, EVoxelType> MeshTypes;
	UPROPERTY()
		FVector StartLocation;
	UPROPERTY()
		int32 SectionCount;
	UPROPERTY()
		FBox SectionBounds;
	UPROPERTY()
		FIntVector RegionIndex;
	UPROPERTY()
		FVector VoxelSize;
	UPROPERTY()
		TArray<TEnumAsByte<EVoxelType>> SectionMaterialIDs;
	UPROPERTY()
		FIntVector StartFill;
	UPROPERTY()
		FIntVector EndFill;
	UPROPERTY()
		int32 NumStatesRemaining;
	UPROPERTY()
		bool IsQueued;
	UVdbHandle * VdbHandle;

	UProceduralTerrainMeshComponent(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "Procedural terrain mesh component")
		void AddGrid();
	UFUNCTION(BlueprintCallable, Category = "Procedural terrain mesh component")
		void ReadGridTree();
	UFUNCTION(BlueprintCallable, Category = "Procedural terrain mesh component")
		void FillTreeValues();
	UFUNCTION(BlueprintCallable, Category = "Procedural terrain mesh component")
		void ExtractIsoSurface();
	UFUNCTION(BlueprintCallable, Category = "Procedural terrain mesh component")
		void RemoveGrid();
	UFUNCTION(BlueprintCallable, Category = "Procedural terrain mesh component")
		void MeshGrid();
	UFUNCTION(BlueprintCallable, Category = "Procedural terrain mesh component")
		bool FinishSection(int32 SectionIndex, bool isVisible);
};

struct FGridMeshingThread : public FRunnable
{
	FGridMeshingThread(TQueue<UProceduralTerrainMeshComponent*, EQueueMode::Mpsc> &dirtyGridRegions)
		: DirtyGridRegions(dirtyGridRegions)
	{
	}

	virtual uint32 Run() override
	{
		isRunning = true;
		while (isRunning)
		{
			UProceduralTerrainMeshComponent* terrainMeshComponentPtr = nullptr;
			while (DirtyGridRegions.Dequeue(terrainMeshComponentPtr))
			{
				check(terrainMeshComponentPtr != nullptr);
				UProceduralTerrainMeshComponent &terrainMeshComponent = *terrainMeshComponentPtr;
				terrainMeshComponent.SetComponentTickEnabled(false);
				terrainMeshComponent.AddGrid();
				terrainMeshComponent.NumStatesRemaining--;
				terrainMeshComponent.ReadGridTree();
				terrainMeshComponent.NumStatesRemaining--;
				terrainMeshComponent.FillTreeValues();
				terrainMeshComponent.NumStatesRemaining--;
				terrainMeshComponent.ExtractIsoSurface();
				terrainMeshComponent.NumStatesRemaining--;
				terrainMeshComponent.MeshGrid();
				terrainMeshComponent.NumStatesRemaining--;
				terrainMeshComponent.IsQueued = false;
			}
		}
		return 0;
	}

	virtual void Stop() override
	{
		isRunning = false;
	}

	bool isRunning;
	TQueue<UProceduralTerrainMeshComponent*, EQueueMode::Mpsc> &DirtyGridRegions;
};