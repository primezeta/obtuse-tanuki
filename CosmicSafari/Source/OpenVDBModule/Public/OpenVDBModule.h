#ifndef __OPENVDBMODULE_H__
#define __OPENVDBMODULE_H__

#include "EngineMinimal.h"

class OPENVDBMODULE_API FOpenVDBModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static inline FOpenVDBModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FOpenVDBModule>("OpenVDBModule");
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("OpenVDBModule");
	}

	FString CreateDynamicVdb(float surfaceValue, const FIntVector &boundsStart, const FIntVector &boundsEnd, int32 range, float &isoValue);
	FString ReadVDBFile(FString vdbFilename, FString gridName);
	bool GetVDBMesh(const FString &gridID, float isoValue, TArray<FVector> &Vertices, TArray<int32> &TriangleIndices, TArray<FVector> &Normals);
	bool GetVDBGreedyMesh(const FString &gridID, float isoValue, TArray<FVector> &Vertices, TArray<int32> &TriangleIndices, TArray<FVector> &Normals);

private:	
	bool CreateMesh(const FString &gridID, float isoValue);
	bool CreateGreedyMesh(const FString &gridID, float isoValue);
	bool GetMeshGeometry(const FString &gridID, TArray<FVector> &Vertices, TArray<int32> &TriangleIndices, TArray<FVector> &Normals);

	TMap<FString, FString> MeshRegions;
};

DECLARE_LOG_CATEGORY_EXTERN(LogOpenVDBModule, Log, All);

#endif