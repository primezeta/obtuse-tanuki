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

	bool LoadVdbFile(FString vdbFilename, FString gridName);
	bool GetVDBGeometry(double isovalue, double adaptivity, TArray<FVector> &Vertices, TArray<int32> &TriangleIndices);
};

DECLARE_LOG_CATEGORY_EXTERN(LogOpenVDBModule, Log, All);

#endif