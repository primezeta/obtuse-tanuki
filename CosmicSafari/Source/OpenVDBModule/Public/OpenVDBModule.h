#ifndef __OPENVDBMODULE_H__
#define __OPENVDBMODULE_H__

#include "EngineMinimal.h"
#include "VDBHandle.h"

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
};

DECLARE_LOG_CATEGORY_EXTERN(LogOpenVDBModule, Log, All)

#endif