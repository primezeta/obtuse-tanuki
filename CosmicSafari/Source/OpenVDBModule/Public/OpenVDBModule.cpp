#include "OpenVDBModule.h"
#include "libovdb.h"
#include <string>
#include <vector>

void FOpenVDBModule::StartupModule()
{
	std::string errorMsg = "";
	std::string filename = "C:/Users/zach/Desktop/mygrids.vdb";
	if (OvdbInitialize(errorMsg, filename))
	{
		std::string s = errorMsg; //just for a breakpoint
	}

	return;
}

void FOpenVDBModule::ShutdownModule()
{
	OvdbUninitialize();
	return;
}

IMPLEMENT_GAME_MODULE(FOpenVDBModule, OpenVDBModule);