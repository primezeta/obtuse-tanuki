#include "OpenVDBModule.h"
#include "libovdb.h"

void FOpenVDBModule::StartupModule()
{
	//OvdbInitialize("C:/Users/zach/Documents/Unreal Projects/turbo-danger-realm/CosmicSafari/mygrids.vdb");
	//OvdbInitialize("C:\\Users\\zach\\Desktop\\mygrids.vdb");
	OvdbInitialize("C:/Users/zach/Desktop/mygrids.vdb");
}

void FOpenVDBModule::ShutdownModule()
{
	OvdbUninitialize();
}

IMPLEMENT_GAME_MODULE(FOpenVDBModule, OpenVDBModule);