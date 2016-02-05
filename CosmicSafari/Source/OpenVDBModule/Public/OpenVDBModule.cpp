#include "OpenVDBModule.h"
#include "libovdb.h"

DEFINE_LOG_CATEGORY(LogOpenVDBModule)

using namespace ovdb;
using namespace ovdb::meshing;

void FOpenVDBModule::StartupModule()
{
	if (OvdbInitialize())
	{
		//TODO: Handle Ovdb errors
	}
	return;
}

FString FOpenVDBModule::ReadVDBFile(FString vdbFilename, FString gridName)
{
	std::wstring wfname = *vdbFilename;
	std::wstring wgname = *gridName;
	std::string fname = std::string(wfname.begin(), wfname.end());
	std::string gname = std::string(wgname.begin(), wgname.end());

	IDType gridID = INVALID_GRID_ID;
	if (OvdbReadVdb(fname, gname, gridID))
	{
		UE_LOG(LogOpenVDBModule, Fatal, TEXT("Failed to read vdb file!"));
	}
	return gridID.data();
}

FString FOpenVDBModule::CreateDynamicVdb(float surfaceValue, const FIntVector &boundsStart, const FIntVector &boundsEnd, int32 range, float &isoValue)
{
	VolumeDimensions volumeDimensions(boundsStart.X, boundsEnd.X, boundsStart.Y, boundsEnd.Y, boundsStart.Z, boundsEnd.Z);
	IDType gridID = OvdbCreateLibNoiseVolume("noise", surfaceValue, volumeDimensions, (uint32)range, isoValue); //TODO: Range check dims since internally they are unsigned;
	if (gridID == INVALID_GRID_ID)
	{
		UE_LOG(LogOpenVDBModule, Fatal, TEXT("Failed to create dynamic vdb! (invalid grid ID)"));
	}
	return gridID.data();
}

FString FOpenVDBModule::CreateGridMeshRegion(const FString &gridID, int32 regionIndex, float isoValue, TArray<FVector> &Vertices, TArray<int32> &TriangleIndices, TArray<FVector> &Normals)
{
	FString regionID = FString::Printf(TEXT("%s:%d"), *gridID, regionIndex);
	if (OvdbVolumeToMesh(gridID.GetCharArray().GetData(), regionID.GetCharArray().GetData(), ovdb::meshing::MESHING_NAIVE, isoValue) != 0)
	{
		regionID = FString(INVALID_GRID_ID.data());
	}
	return regionID;
}

bool FOpenVDBModule::GetMeshGeometry(const FString &regionID, TArray<FVector> &Vertices, TArray<int32> &TriangleIndices, TArray<FVector> &Normals)
{
	FVector vertex;
	while (OvdbYieldNextMeshPoint(regionID.GetCharArray().GetData(), vertex.X, vertex.Y, vertex.Z))
	{
		Vertices.Add(vertex);
	}

	uint32 triangleIndices[3];
	while (OvdbYieldNextMeshPolygon(regionID.GetCharArray().GetData(), triangleIndices[0], triangleIndices[1], triangleIndices[2]))
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
	while (OvdbYieldNextMeshNormal(regionID.GetCharArray().GetData(), normal.X, normal.Y, normal.Z))
	{
		Normals.Add(normal);
	}
	return true; //TODO: Handle errors
}

void FOpenVDBModule::ShutdownModule()
{
	OvdbUninitialize();
	return;
}

IMPLEMENT_GAME_MODULE(FOpenVDBModule, OpenVDBModule);