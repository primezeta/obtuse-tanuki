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
		bool IsGridDirty;
	UPROPERTY()
		bool IsGridReady;
	UPROPERTY()
		int32 IsSectionReady[FVoxelData::VOXEL_TYPE_COUNT];
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

	UProceduralTerrainMeshComponent(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(Category = "Procedural terrain mesh component")
		void InitMeshComponent(UVdbHandle * vdbHandle);
	UFUNCTION(BlueprintCallable, Category = "Procedural terrain mesh component")
		FString AddGrid(const FIntVector &regionIndex, const FVector &voxelSize);
	UFUNCTION(BlueprintCallable, Category = "Procedural terrain mesh component")
		void RemoveGrid();
	UFUNCTION(BlueprintCallable, Category = "Procedural terrain mesh component")
		void ReadGridTree(TArray<TEnumAsByte<EVoxelType>> &sectionMaterialIDs);
	UFUNCTION(BlueprintCallable, Category = "Procedural terrain mesh component")
		void MeshGrid();

private:
	UVdbHandle * VdbHandle;
};

struct FGridMeshingThread : public FRunnable
{
	FGridMeshingThread(TQueue<FString, EQueueMode::Mpsc> &dirtyGridRegions, TMap<FString, UProceduralTerrainMeshComponent*> &terrainMeshComponents)
		: DirtyGridRegions(dirtyGridRegions), TerrainMeshComponents(terrainMeshComponents)
	{
	}

	virtual uint32 Run() override
	{
		isRunning = true;
		while (isRunning)
		{
			FString regionName;
			while (DirtyGridRegions.Dequeue(regionName))
			{
				check(TerrainMeshComponents.Contains(regionName));
				UProceduralTerrainMeshComponent &terrainMeshComponent = *TerrainMeshComponents[regionName];
				check(!terrainMeshComponent.IsGridReady);
				terrainMeshComponent.SetComponentTickEnabled(false);
				terrainMeshComponent.MeshGrid();
				for (int32 i = 0; i < FVoxelData::VOXEL_TYPE_COUNT; ++i)
				{
					terrainMeshComponent.FinishMeshSection_Async(i, false);
					terrainMeshComponent.IsSectionReady[i] = 1;
				}
				terrainMeshComponent.IsGridReady = true;
			}
		}
		return 0;
	}

	virtual void Stop() override
	{
		isRunning = false;
	}

	bool isRunning;
	TQueue<FString, EQueueMode::Mpsc> &DirtyGridRegions;
	TMap<FString, UProceduralTerrainMeshComponent*> &TerrainMeshComponents;
};