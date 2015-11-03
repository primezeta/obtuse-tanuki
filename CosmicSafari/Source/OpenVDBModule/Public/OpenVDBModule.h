#ifndef __OPENVDBMODULE_H__
#define __OPENVDBMODULE_H__

#include "EngineMinimal.h"
//#include "Engine/"

class FOpenVDBModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	
};

#endif