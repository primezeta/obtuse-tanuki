#pragma once
#include "EngineMinimal.h"
#include "VDBInterface.generated.h"

UINTERFACE(Blueprintable)
class UVDBInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class IVDBInterface
{
	GENERATED_IINTERFACE_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "VDB Interface")
	virtual void Initialize(const FString &path, bool enableDelayLoad, bool enableGridStats) = 0;

	UFUNCTION(BlueprintCallable, Category = "VDB Interface")
	virtual FString AddGrid(const FString &gridName, const FIntVector &indexStart, const FIntVector &indexEnd) = 0;

	UFUNCTION(BlueprintCallable, Category = "VDB Interface")
	virtual void RemoveGrid(const FString &gridID) = 0;

	UFUNCTION(BlueprintCallable, Category = "VDB Interface")
	virtual void ReadGridTree(const FString &gridID, FIntVector &indexStart, FIntVector &indexEnd) = 0;

	UFUNCTION(BlueprintCallable, Category = "VDB Interface")
	virtual void MeshGrid(const FString &gridID, float surfaceValue, TArray<FVector> &vertexBuffer, TArray<int32> &polygonBuffer, TArray<FVector> &normalBuffer) = 0;

	UFUNCTION(BlueprintCallable, Category = "VDB Interface")
	virtual void ReadGridIndexBounds(const FString &gridID, FIntVector &indexStart, FIntVector &indexEnd) = 0;

	UFUNCTION(BlueprintCallable, Category = "VDB Interface")
	virtual SIZE_T ReadGridCount() = 0;

	UFUNCTION(BlueprintCallable, Category = "VDB Interface")
	virtual void PopulateGridDensity_Perlin(const FString &gridID, double frequency, double lacunarity, double persistence, int octaveCount) = 0;
};