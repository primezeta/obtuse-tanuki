#ifndef __OPENVDBMODULE_H__
#define __OPENVDBMODULE_H__

#include "EngineMinimal.h"
#include "VdbHandle.h"

class OPENVDBMODULE_API FOpenVDBModule : public IModuleInterface
{
public:
	static inline FOpenVDBModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FOpenVDBModule>("OpenVDBModule");
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("OpenVDBModule");
	}

	static bool RegisterVdb(const FString &vdbName, const FString &vdbFilepath, bool enableGridStats, bool enableDelayLoad);
	static void UnregisterVdb(const FString &vdbName);
	static FString AddGrid(const FString &vdbName, const FString &gridName, const FIntVector &regionIndex, const FVector &voxelSize, TArray<FProcMeshSection> &sectionBuffers);
	static void ReadGridTree(const FString &vdbName, const FString &gridID, FIntVector &startIndex, FIntVector &endIndex);
	static void FillTreePerlin(const FString &vdbName, const FString &gridID, FIntVector &startFill, FIntVector &endFill, int32 seed, float frequency, float lacunarity, float persistence, int32 octaveCount, bool threaded = true);
	static bool ExtractIsoSurface(const FString &vdbName, const FString &gridID, EMeshType MeshMethod, TArray<TEnumAsByte<EVoxelType>> &sectionMaterialIDs, FBox &gridDimensions, FVector &initialLocation, bool threaded = true);
	static void MeshGrid(const FString &vdbName, const FString &gridID, EMeshType MeshMethod);
	static TArray<FString> GetAllGridIDs(const FString &vdbName);
	static void RemoveGrid(const FString &vdbName, const FString &gridID);
	static void SetRegionScale(const FString &vdbName, const FIntVector &regionScale);
	static void GetVoxelCoord(const FString &vdbName, const FString &gridID, const FVector &worldLocation, FIntVector &outVoxelCoord);
	static bool GetGridDimensions(const FString &vdbName, const FString &gridID, FVector &startLocation);
	static bool GetGridDimensions(const FString &vdbName, const FString &gridID, FBox &worldBounds);
	static bool GetGridDimensions(const FString &vdbName, const FString &gridID, FBox &worldBounds, FVector &startLocation);
	static FIntVector GetRegionIndex(const FString &vdbName, const FVector &worldLocation);
	static void WriteAllGrids(const FString &vdbName);
};

#endif