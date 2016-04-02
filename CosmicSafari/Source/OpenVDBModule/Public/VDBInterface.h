#pragma once
#include "EngineMinimal.h"
#include "VDBInterface.generated.h"

UINTERFACE(Blueprintable)
class OPENVDBMODULE_API UVDBInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class IVDBInterface
{
	GENERATED_IINTERFACE_BODY()

public:
	virtual FString AddGrid(const FString &gridName, const FIntVector &indexStart, const FIntVector &indexEnd) = 0;
	virtual void RemoveGrid(const FString &gridID) = 0;
	virtual void ReadGridTree(const FString &gridID, FIntVector &indexStart, FIntVector &indexEnd) = 0;
	virtual void MeshGrid(const FString &gridID, float surfaceValue, TArray<FVector> &vertexBuffer, TArray<int32> &polygonBuffer, TArray<FVector> &normalBuffer) = 0;
	virtual void ReadGridIndexBounds(const FString &gridID, FIntVector &indexStart, FIntVector &indexEnd) = 0;
	virtual int32 ReadGridCount() = 0;
	virtual void PopulateGridDensity_Perlin(const FString &gridID, float frequency, float lacunarity, float persistence, int32 octaveCount) = 0;
};