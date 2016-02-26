#ifndef __OPENVDBMODULE_H__
#define __OPENVDBMODULE_H__

#include "libovdb.h"
//#include "LibNoise/module/perlin.h"
#include "EngineMinimal.h"

class OPENVDBMODULE_API FOpenVDBModule : public IModuleInterface
{
public:
	
	FOpenVDBModule() : OvdbInterface(GetIOvdbInstance()) {}

	static inline FOpenVDBModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FOpenVDBModule>("OpenVDBModule");
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("OpenVDBModule");
	}

	UFUNCTION(BlueprintCallable)
	void ReadVDBFile(const FString &vdbFilename, const FString &gridName);

	UFUNCTION(BlueprintCallable)
	void CreateDynamicVdb(const FString &gridID, float surfaceValue, const FIntVector &boundsStart, const FIntVector &boundsEnd, int32 range, double scaleXYZ, double frequency, double lacunarity, double persistence, int32 octaveCount);

	UFUNCTION(BlueprintCallable)
	void FOpenVDBModule::CreateGridMeshRegions(const FString &gridID, int32 regionCountX, int32 regionCountY, int32 regionCountZ, TArray<FString> &regionIDs);

	UFUNCTION(BlueprintCallable)
	void GetMeshGeometry(const FString &gridID, const FString &meshID, float surfaceValue, TArray<FVector> &Vertices, TArray<int32> &TriangleIndices, TArray<FVector> &Normals);

private:
	IOvdb * OvdbInterface;
};

DECLARE_LOG_CATEGORY_EXTERN(LogOpenVDBModule, Log, All);

#endif