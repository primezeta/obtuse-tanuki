#include "OpenVDBModule.h"
#include "libovdb.h"
#include <string>
#include <vector>

void FOpenVDBModule::StartupModule()
{
	//OvdbInitialize("C:/Users/zach/Documents/Unreal Projects/turbo-danger-realm/CosmicSafari/mygrids.vdb");
	//OvdbInitialize("C:\\Users\\zach\\Desktop\\mygrids.vdb");

	std::string errorMsg = "";
	std::string filename = "C:/Users/zach/Desktop/mygrids.vdb";
	if (OvdbInitialize(errorMsg, filename))
	{
		std::string s = errorMsg; //just for a breakpoint
	}
	
	std::vector<float> pxs;
	std::vector<float> pys;
	std::vector<float> pzs;
	std::vector<unsigned int> qxs;
	std::vector<unsigned int> qys;
	std::vector<unsigned int> qzs;
	std::vector<unsigned int> qws;

	if (OvdbVolumeToMesh(errorMsg, pxs, pys, pzs, qxs, qys, qzs, qws))
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