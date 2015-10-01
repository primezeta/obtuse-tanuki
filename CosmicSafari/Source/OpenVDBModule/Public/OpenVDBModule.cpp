#include "OpenVDBModule.h"
#include "libovdb.h"

void FOpenVDBModule::StartupModule()
{
	OvdbInitialize();
}

void FOpenVDBModule::ShutdownModule()
{
	OvdbUninitialize();
}

IMPLEMENT_GAME_MODULE(FOpenVDBModule, OpenVDBModule);