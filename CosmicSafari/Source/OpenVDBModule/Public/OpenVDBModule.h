#ifndef __OPENVDBMODULE_H__
#define __OPENVDBMODULE_H__

#include "libovdb.h"
//#include "LibNoise/module/perlin.h"
#include "EngineMinimal.h"

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

	UFUNCTION(BlueprintCallable)
	void InitializeVDB(const FString &vdbFilename, const FString &gridID);

	UFUNCTION(BlueprintCallable)
	FString CreateRegion(const FString &gridID, const FIntVector &start, const FIntVector &end);

	UFUNCTION(BlueprintCallable)
	void LoadRegion(const FString &regionID);

	UFUNCTION(BlueprintCallable)
	void FillRegionWithPerlinDensity(const FString &regionID, double scaleXYZ, double frequency, double lacunarity, double persistence, int32 octaveCount);

	UFUNCTION(BlueprintCallable)
	void GenerateMesh(const FString &regionID, float surfaceValue);

	UFUNCTION(BlueprintCallable)
	void GetMeshGeometry(const FString &regionID, TArray<FVector> &Vertices, TArray<int32> &TriangleIndices, TArray<FVector> &Normals);

private:
	IOvdb * OvdbInterface;
};

DECLARE_LOG_CATEGORY_EXTERN(LogOpenVDBModule, Log, All);

#endif