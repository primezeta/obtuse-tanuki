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
		bool IsGridSectionMeshed;
	UPROPERTY()
		bool CreateCollision;
	UPROPERTY()
		TMap<int32, EVoxelType> MeshTypes;
	UPROPERTY()
		FVector StartLocation;
	UPROPERTY()
		int32 SectionCount;

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
	UFUNCTION(BlueprintCallable, Category = "VDB Handle")
		FBox GetGridDimensions();

private:
	UVdbHandle * VdbHandle;
};
