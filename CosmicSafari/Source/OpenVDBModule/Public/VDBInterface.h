// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Object.h"
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
	virtual FString AddGrid(const FString &gridName, const FIntVector &worldIndex, FIntVector &indexStart, FIntVector &indexEnd) = 0;
	virtual void RemoveGrid(const FString &gridID) = 0;
	virtual void SetRegionSize(const FIntVector &regionSize) = 0;
	virtual void ReadGridTreeIndex(const FString &gridID, FIntVector &activeStart, FIntVector &activeEnd) = 0;
	//TODO
	//virtual void ReadGridTreeWorld(const FString &gridID, FVector &activeStart, FVector &activeEnd) = 0;
	virtual void MeshGrid(const FString &gridID, float surfaceValue, TArray<FVector> &vertexBuffer, TArray<int32> &polygonBuffer, TArray<FVector> &normalBuffer) = 0;
	virtual void ReadGridIndexBounds(const FString &gridID, FIntVector &indexStart, FIntVector &indexEnd) = 0;
	virtual int32 ReadGridCount() = 0;
};