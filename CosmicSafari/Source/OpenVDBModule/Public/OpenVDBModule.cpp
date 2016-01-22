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

uint32 FOpenVDBModule::LoadVdbFile(FString vdbFilename, FString gridName)
{
	std::wstring wfname = *vdbFilename;
	std::wstring wgname = *gridName;
	std::string fname = std::string(wfname.begin(), wfname.end());
	std::string gname = std::string(wgname.begin(), wgname.end());

	uint32 gridID = UINT_MAX;
	if (OvdbReadVdb(fname, gname, gridID))
	{
		//TODO: Handle Ovdb errors
		return false;
	}
	return gridID;
}
bool FOpenVDBModule::GetVDBGeometry(FString vdbFilename, FString gridName, float surfaceValue, TArray<FVector> &Vertices, TArray<int32> &TriangleIndices)
{
	//TODO: Sanity check to see if a VDB file has been loaded
	uint32 id = LoadVdbFile(vdbFilename, gridName);
	if (OvdbVolumeToMesh(id, MESHING_NAIVE, surfaceValue))
	{
		//TODO: Handle Ovdb errors
		return false;
	}

	FVector vertex;
	while (OvdbYieldNextMeshPoint(id, vertex.X, vertex.Y, vertex.Z))
	{
		Vertices.Add(vertex);
	}

	uint32 triangleIndices[3];
	while (OvdbYieldNextMeshPolygon(id, triangleIndices[0], triangleIndices[1], triangleIndices[2]))
	{
		for (int i = 0; i < 3; i++)
		{
			int32 testIndex = (int32)triangleIndices[i];
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