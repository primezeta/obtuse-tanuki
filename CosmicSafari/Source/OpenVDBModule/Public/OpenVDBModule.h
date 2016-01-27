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

	uint32 CreateDynamicVdb(float surfaceValue, int32 dimX, int32 dimY, int32 dimZ, float &isovalue);
	uint32 ReadVDBFile(FString vdbFilename, FString gridName);
	bool GetVDBMesh(uint32 gridID, float isovalue, TArray<FVector> &Vertices, TArray<int32> &TriangleIndices, TArray<FVector> &Normals);
	bool GetVDBGreedyMesh(uint32 gridID, float isovalue, TArray<FVector> &Vertices, TArray<int32> &TriangleIndices, TArray<FVector> &Normals);

private:	
	bool CreateMesh(uint32 gridID, float surfaceValue);
	bool CreateGreedyMesh(uint32 gridID, float surfaceValue);	
	bool GetMeshGeometry(uint32 gridID, TArray<FVector> &Vertices, TArray<int32> &TriangleIndices, TArray<FVector> &Normals);
};

DECLARE_LOG_CATEGORY_EXTERN(LogOpenVDBModule, Log, All);

#endif