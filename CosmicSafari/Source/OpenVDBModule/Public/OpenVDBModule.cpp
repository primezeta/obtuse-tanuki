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

uint32 FOpenVDBModule::ReadVDBFile(FString vdbFilename, FString gridName)
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

uint32 FOpenVDBModule::CreateDynamicVdb(float surfaceValue, int32 dimX, int32 dimY, int32 dimZ, float &isovalue)
{
	uint32 gridID = UINT32_MAX;
	if (OvdbCreateLibNoiseVolume("noise", surfaceValue, (uint32)dimX, (uint32)dimY, (uint32)dimZ, gridID, isovalue)) //TODO: Range check dims since internally they are unsigned
	{
		UE_LOG(LogOpenVDBModule, Fatal, TEXT("Failed to create dynamic vdb!"));
	}
	return gridID;
}

bool FOpenVDBModule::CreateMesh(uint32 gridID, float isovalue)
{
	//TODO: Handle Ovdb errors
	return OvdbVolumeToMesh(gridID, MESHING_NAIVE, isovalue) == 0;
}

bool FOpenVDBModule::CreateGreedyMesh(uint32 gridID, float isovalue)
{
	//TODO: Handle Ovdb errors
	return OvdbVolumeToMesh(gridID, MESHING_GREEDY, isovalue) == 0;
}

bool FOpenVDBModule::GetMeshGeometry(uint32 gridID, TArray<FVector> &Vertices, TArray<int32> &TriangleIndices, TArray<FVector> &Normals)
{
	FVector vertex;
	while (OvdbYieldNextMeshPoint(gridID, vertex.X, vertex.Y, vertex.Z))
	{
		Vertices.Add(vertex);
	}

	uint32 triangleIndices[3];
	while (OvdbYieldNextMeshPolygon(gridID, triangleIndices[0], triangleIndices[1], triangleIndices[2]))
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

	FVector normal;
	while (OvdbYieldNextMeshNormal(gridID, normal.X, normal.Y, normal.Z))
	{
		Normals.Add(normal);
	}
	return true; //TODO: Handle errors
}

bool FOpenVDBModule::GetVDBMesh(uint32 gridID, float isovalue, TArray<FVector> &Vertices, TArray<int32> &TriangleIndices, TArray<FVector> &Normals)
{
	if (CreateMesh(gridID, isovalue))
	{
		if (GetMeshGeometry(gridID, Vertices, TriangleIndices, Normals))
		{
			return true;
		}
	}
	return false;
}

bool FOpenVDBModule::GetVDBGreedyMesh(uint32 gridID, float isovalue, TArray<FVector> &Vertices, TArray<int32> &TriangleIndices, TArray<FVector> &Normals)
{
	if (CreateGreedyMesh(gridID, isovalue))
	{
		if (GetMeshGeometry(gridID, Vertices, TriangleIndices, Normals))
		{
			return true;
		}
	}
	return false;
}

void FOpenVDBModule::ShutdownModule()
{
	OvdbUninitialize();
	return;
}

IMPLEMENT_GAME_MODULE(FOpenVDBModule, OpenVDBModule);