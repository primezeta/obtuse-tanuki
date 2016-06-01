// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Object.h"
#include "ProceduralTerrainMeshComponent.h"
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
	virtual FString AddGrid(const FString &gridName, const FIntVector &regionIndex, const FVector &voxelSize) = 0;
	virtual void RemoveGrid(const FString &gridID) = 0;
	virtual void SetRegionScale(const FIntVector &regionScale) = 0;
	virtual void ReadGridTree(const FString &gridID) = 0;
	virtual void GetVoxelCoord(const FString &gridID, const FVector &worldLocation, FIntVector &outVoxelCoord) = 0;
	virtual void MeshGrid(const FString &gridID,
		const FVector &playerLocation,
		TSharedPtr<VertexBufferType> &VertexBuffer,
		TSharedPtr<PolygonBufferType> &PolygonBuffer,
		TSharedPtr<NormalBufferType> &NormalBuffer,
		TSharedPtr<UVMapBufferType> &UVMapBuffer,
		TSharedPtr<VertexColorBufferType> &VertexColorBuffer,
		TSharedPtr<TangentBufferType> &TangentBuffer,
		FVector &worldStart,
		FVector &worldEnd,
		FVector &startLocation) = 0;
};