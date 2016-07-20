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
		FString MeshName;
	UPROPERTY()
		FString MeshID;
	UPROPERTY()
		EGridState RegionState;
	UPROPERTY()
		int32 IsStateStarted[NUM_GRID_STATES];
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
		FVector StartLocation;
	UPROPERTY()
		APlayerStart * RegionStart;
	UPROPERTY()
		int32 SectionCount;
	UPROPERTY()
		FIntVector RegionIndex;
	UPROPERTY()
		FVector VoxelSize;
	UPROPERTY()
		TArray<TEnumAsByte<EVoxelType>> SectionMaterialIDs;
	UPROPERTY()
		FBox SectionBounds;
	UPROPERTY()
		FIntVector StartIndex;
	UPROPERTY()
		FIntVector EndIndex;
	UPROPERTY()
		int32 NumStatesRemaining;
	UPROPERTY()
		bool IsQueued;
	UVdbHandle * VdbHandle;

	UProceduralTerrainMeshComponent(const FObjectInitializer& ObjectInitializer);
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

	void AddGrid(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent);
	void ReadGridTree(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent);
	void FillTreeValues(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent);
	void ExtractIsoSurface(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent);
	void RemoveGrid(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent);
	void MeshGrid(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent);
	void FinishAllSections(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent);
	void FinishSection(int32 SectionIndex, bool isVisible);
};