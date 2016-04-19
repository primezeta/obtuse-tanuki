#ifndef __OPENVDBMODULE_H__
#define __OPENVDBMODULE_H__

#include "EngineMinimal.h"
#include "VdbHandle.h"
#include "VDBHandlePrivate.h"

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

	void RegisterVdb(const FString &FilePath, bool EnableGridStats, bool EnableDelayLoad);
	void UnregisterVdb(const FString &FilePath);

	static VdbRegistryType VdbRegistry;
};

#endif