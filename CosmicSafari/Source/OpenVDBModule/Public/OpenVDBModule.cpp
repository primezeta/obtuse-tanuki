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
	static IDType internalID; //Workaround for UE4 tarray TBB race condition during deallocation
	std::wstring wfname = *vdbFilename;
	std::wstring wgname = *gridName;
	std::string fname = std::string(wfname.begin(), wfname.end());
	std::string gname = std::string(wgname.begin(), wgname.end());

	internalID = INVALID_GRID_ID;
	if (OvdbReadVdb(fname, gname, internalID))
	{
		UE_LOG(LogOpenVDBModule, Fatal, TEXT("Failed to read vdb file!"));
	}
	//UE4 seems to internally use TBB (threads) to handle string memory and a race condition can cause a crash, so do the following to successfully copy the string
	return FString(FString::Printf(TEXT("%s"), internalID.c_str()));
}

FString FOpenVDBModule::CreateDynamicVdb(float surfaceValue, const FIntVector &boundsStart, const FIntVector &boundsEnd, int32 range, float &isoValue)
{
	static IDType internalID; //Workaround for UE4 tarray TBB race condition during deallocation
	VolumeDimensions volumeDimensions(boundsStart.X, boundsEnd.Y, boundsStart.Z, boundsEnd.X, boundsStart.Y, range);
	internalID = OvdbCreateLibNoiseVolume("noise", surfaceValue, volumeDimensions, isoValue); //TODO: Range check dims since internally they are unsigned;
	if (internalID == INVALID_GRID_ID)
	{
		UE_LOG(LogOpenVDBModule, Fatal, TEXT("Failed to create dynamic vdb! (invalid grid ID)"));
	}
	//UE4 seems to internally use TBB (threads) to handle string memory and a race condition can cause a crash, so do the following to successfully copy the string
	return FString(FString::Printf(TEXT("%s"), internalID.c_str()));
}

FString FOpenVDBModule::CreateGridMeshRegion(const FString &gridID, int32 regionIndex, ovdb::meshing::VolumeDimensions &dims, float isoValue, TArray<FVector> &Vertices, TArray<int32> &TriangleIndices, TArray<FVector> &Normals)
{
	static IDType internalID; //Workaround for UE4 tarray TBB race condition during deallocation
	FString id = FString::Printf(TEXT("%d"), regionIndex);
	ovdb::meshing::IDType gid = gridID.GetCharArray().GetData();
	ovdb::meshing::IDType rid = id.GetCharArray().GetData();
	if (OvdbVolumeToMesh(gid, rid, dims, ovdb::meshing::MESHING_NAIVE, isoValue) != 0)
	{
		//UE4 seems to internally use TBB (threads) to handle string memory and a race condition can cause a crash, so do the following to successfully copy the string
		internalID = INVALID_GRID_ID;
		id = FString(FString::Printf(TEXT("%s"), internalID.c_str()));
	}
	GetMeshGeometry(id, Vertices, TriangleIndices, Normals);
	return id;
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