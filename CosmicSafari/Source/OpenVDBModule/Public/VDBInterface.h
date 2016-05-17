// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Object.h"
#include "ProceduralMeshComponent.h"
#include "VdbInterface.generated.h"

UENUM(BlueprintType)		//"BlueprintType" is essential to include
enum class EMeshType : uint8
{
	MESH_TYPE_CUBES 		 UMETA(DisplayName = "Mesh as basic cubes"),
	MESH_TYPE_MARCHING_CUBES UMETA(DisplayName = "Mesh with marching cubes"),
};

UINTERFACE(Blueprintable)
class OPENVDBMODULE_API UVdbInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class IVdbInterface
{
	GENERATED_IINTERFACE_BODY()

public:
	virtual FString AddGrid(const FString &gridName, const FIntVector &regionIndex, const FVector &voxelSize) = 0;
	virtual void RemoveGrid(const FString &gridID) = 0;
	virtual void SetRegionScale(const FIntVector &regionScale) = 0;
	virtual void ReadGridTree(const FString &gridID, FIntVector &startFill, FIntVector &endFill) = 0;
	virtual void GetVoxelCoord(const FString &gridID, const FVector &worldLocation, FIntVector &outVoxelCoord) = 0;
	virtual void MeshGrid(const FString &gridID,
		TSharedPtr<TArray<FVector>> &OutVertexBufferPtr,
		TSharedPtr<TArray<int32>> &OutPolygonBufferPtr,
		TSharedPtr<TArray<FVector>> &OutNormalBufferPtr,
		TSharedPtr<TArray<FVector2D>> &OutUVMapBufferPtr,
		TSharedPtr<TArray<FColor>> &OutVertexColorsBufferPtr,
		TSharedPtr<TArray<FProcMeshTangent>> &OutTangentsBufferPtr,
		FVector &worldStart,
		FVector &worldEnd,
		FVector &firstActive) = 0;
};