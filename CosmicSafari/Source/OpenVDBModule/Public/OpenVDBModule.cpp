#include "OpenVDBModule.h"
#include "libovdb.h"
#include <string>

DEFINE_LOG_CATEGORY(LogOpenVDBModule)

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
bool FOpenVDBModule::GetVDBGeometry(double isovalue, double adaptivity, TArray<FVector> &Vertices, TArray<int32> &TriangleIndices)
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
		Vertices.Insert(vertex, 0);
	}

	uint32 triangleIndex[3];
	while (OvdbGetNextMeshTriangle(triangleIndex[0], triangleIndex[1], triangleIndex[2]))
	{
		for (int i = 0; i < 3; i++)
		{
			int32 testIndex = (int32)triangleIndex[i];
			if (testIndex < 0)
			{
				UE_LOG(LogOpenVDBModule, Fatal, TEXT("Triangle index is too large!"));
			}
			else
			{
				TriangleIndices.Add(testIndex);
			}
		}
	}

	return true;
}

void FOpenVDBModule::ShutdownModule()
{
	OvdbUninitialize();
	return;
}

IMPLEMENT_GAME_MODULE(FOpenVDBModule, OpenVDBModule);