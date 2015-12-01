#include "OpenVDBModule.h"
#include "libovdb.h"
#include <string>
#include <vector>

void FOpenVDBModule::StartupModule()
{
	if (OvdbInitialize())
	{
		//TODO: Handle Ovdb errors
	}
	return;
}

bool FOpenVDBModule::LoadVdbFile(FString vdbFilename, FString gridName)
{
	//std::string filename = "C:/Users/zach/Documents/Unreal Projects/obtuse-tanuki/CosmicSafari/ThirdParty/Build/x64/Release/vdbs/noise_w3008_h3008_l3008_t16_s1_t1.vdb";
	//std::string filename = "C:/Users/zach/Documents/Unreal Projects/obtuse-tanuki/CosmicSafari/ThirdParty/Build/x64/Release/vdbs/noise_w288_h288_l288_t16_s1_t1.vdb";
	//std::string filename = "C:/Users/zach/Documents/Unreal Projects/obtuse-tanuki/CosmicSafari/ThirdParty/Build/x64/Release/vdbs/noise_w100_h100_l100_t10_s1_t0.vdb";
	//std::string filename = "C:/Users/zach/Documents/Unreal Projects/obtuse-tanuki/CosmicSafari/ThirdParty/Build/x64/Debug/vdbs/noise_w288_h288_l288_t16_s1_t0.vdb";
	//std::string filename = "C:/Users/zach/Documents/Unreal Projects/obtuse-tanuki/CosmicSafari/ThirdParty/Build/x64/Release/vdbs/noise_w512_h512_l512_t8_s1_t1.vdb";

	std::wstring wfname = *vdbFilename;
	std::wstring wgname = *gridName;
	std::string fname = std::string(wfname.begin(), wfname.end());
	std::string gname = std::string(wgname.begin(), wgname.end());
	if (OvdbLoadVdb(fname, gname))
	{
		//TODO: Handle Ovdb errors
		return false;
	}
	return true;
}
bool FOpenVDBModule::GetVDBGeometry(double isovalue, double adaptivity, TQueue<FVector> &Vertices, TQueue<uint32_t> &TriangleIndices)
{
	//TODO: Sanity check to see if a VDB file has been loaded

	if (OvdbVolumeToMesh(isovalue, adaptivity))
	{
		//TODO: Handle Ovdb errors
		return false;
	}

	FVector vertex;
	while (OvdbGetNextMeshPoint(vertex.X, vertex.Y, vertex.Z))
	{
		//Vertices are taken from the back of the Ovdb vector, so enqueue them to preserve the order
		Vertices.Enqueue(vertex);
	}

	uint32_t vertexIndex = 0;
	while (OvdbGetNextMeshTriangle(vertexIndex))
	{
		TriangleIndices.Enqueue(vertexIndex);
	}

	return true;
}

void FOpenVDBModule::ShutdownModule()
{
	OvdbUninitialize();
	return;
}

IMPLEMENT_GAME_MODULE(FOpenVDBModule, OpenVDBModule);