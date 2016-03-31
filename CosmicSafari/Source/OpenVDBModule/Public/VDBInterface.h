#pragma once
#include "EngineMinimal.h"

UINTERFACE(Blueprintable)
class UVDBInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class IVDBInterface
{
	GENERATED_IINTERFACE_BODY()

	UFUNCTION(BlueprintCallable, Category = "VDB Interface")
	void Initialize(const FString &path, bool enableDelayLoad, bool enableGridStats);

	UFUNCTION(BlueprintCallable, Category = "VDB Interface")
	FString AddGrid(const FString &gridName, const FIntVector &indexStart, const FIntVector &indexEnd);

	UFUNCTION(BlueprintCallable, Category = "VDB Interface")
	void RemoveGrid(const FString &gridID);

	UFUNCTION(BlueprintCallable, Category = "VDB Interface")
	void ReadGridTree(const FString &gridID, const FIntVector &indexStart, const FIntVector &indexEnd, TArray<FVector> &vertexBuffer, TArray<int32> &polygonBuffer, TArray<FVector> &normalBuffer);

	UFUNCTION(BlueprintCallable, Category = "VDB Interface")
	void MeshGrid(const FString &gridID, float surfaceValue);

	UFUNCTION(BlueprintCallable, Category = "VDB Interface")
	void ReadGridIndexBounds(const FString &gridID, FIntVector &indexStart, FIntVector &indexEnd);

	UFUNCTION(BlueprintCallable, Category = "VDB Interface")
	SIZE_T ReadGridCount();

	UFUNCTION(BlueprintCallable, Category = "VDB Interface")
	void PopulateGridDensity_Perlin(const FString &gridID, double frequency, double lacunarity, double persistence, int octaveCount);
};