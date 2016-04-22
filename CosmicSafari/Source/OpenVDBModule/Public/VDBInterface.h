// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Object.h"
#include "ProceduralMeshComponent.h"
#include "VdbInterface.generated.h"

UINTERFACE(Blueprintable)
class OPENVDBMODULE_API UVdbInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class IVdbInterface
{
	GENERATED_IINTERFACE_BODY()

public:
	virtual FString AddGrid(const FString &gridName, const FVector &worldLocation, const FVector &voxelSize) = 0;
	virtual void RemoveGrid(const FString &gridID) = 0;
	virtual void SetRegionScale(const FIntVector &regionScale) = 0;
	virtual void ReadGridTreeIndex(const FString &gridID, FIntVector &startFill, FIntVector &endFill, FIntVector &activeStart, FIntVector &activeEnd) = 0;
	//TODO
	//virtual void ReadGridTreeWorld(const FString &gridID, FVector &activeStart, FVector &activeEnd) = 0;
	virtual void MeshGrid(const FString &gridID,
						  float surfaceValue,
						  TSharedPtr<TArray<FVector>> &OutVertexBufferPtr,
						  TSharedPtr<TArray<int32>> &OutPolygonBufferPtr,
						  TSharedPtr<TArray<FVector>> &OutNormalBufferPtr,
						  TSharedPtr<TArray<FVector2D>> &OutUVMapBufferPtr,
						  TSharedPtr<TArray<FColor>> &OutVertexColorsBufferPtr,
						  TSharedPtr<TArray<FProcMeshTangent>> &OutTangentsBufferPtr) = 0;
};