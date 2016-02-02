#include "OpenVDBModule.h"
#include "libovdb.h"

DEFINE_LOG_CATEGORY(LogOpenVDBModule)

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

	GridIDType gridID = INVALID_GRID_ID;
	if (OvdbReadVdb(fname, gname, gridID))
	{
		UE_LOG(LogOpenVDBModule, Fatal, TEXT("Failed to read vdb file!"));
	}
	return gridID.data();
}

FString FOpenVDBModule::CreateDynamicVdb(float surfaceValue, const FIntVector &boundsStart, const FIntVector &boundsEnd, int32 libnoiseRange, float &isovalue)
{
	VolumeDimensions volumeDimensions(boundsStart.X, boundsEnd.X, boundsStart.Y, boundsEnd.Y, boundsStart.Z, boundsEnd.Z);
	GridIDType gridID = OvdbCreateLibNoiseVolume("noise", surfaceValue, volumeDimensions, (uint32)libnoiseRange, isovalue); //TODO: Range check dims since internally they are unsigned;
	if (gridID == INVALID_GRID_ID)
	{
		UE_LOG(LogOpenVDBModule, Fatal, TEXT("Failed to create dynamic vdb! (invalid grid ID)"));
	}
	return gridID.data();
}

bool FOpenVDBModule::CreateMesh(const FString &gridID, float isovalue)
{
	//TODO: Handle Ovdb errors
	return OvdbVolumeToMesh(gridID.GetCharArray().GetData(), MESHING_NAIVE, isovalue) == 0;
}

bool FOpenVDBModule::CreateGreedyMesh(const FString &gridID, float isovalue)
{
	//TODO: Handle Ovdb errors
	return OvdbVolumeToMesh(gridID.GetCharArray().GetData(), MESHING_GREEDY, isovalue) == 0;
}

bool FOpenVDBModule::GetMeshGeometry(const FString &gridID, TArray<FVector> &Vertices, TArray<int32> &TriangleIndices, TArray<FVector> &Normals)
{
	FVector vertex;
	while (OvdbYieldNextMeshPoint(gridID.GetCharArray().GetData(), vertex.X, vertex.Y, vertex.Z))
	{
		Vertices.Add(vertex);
	}

	uint32 triangleIndices[3];
	while (OvdbYieldNextMeshPolygon(gridID.GetCharArray().GetData(), triangleIndices[0], triangleIndices[1], triangleIndices[2]))
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
	while (OvdbYieldNextMeshNormal(gridID.GetCharArray().GetData(), normal.X, normal.Y, normal.Z))
	{
		Normals.Add(normal);
	}
	return true; //TODO: Handle errors
}

bool FOpenVDBModule::GetVDBMesh(const FString &gridID, float isovalue, TArray<FVector> &Vertices, TArray<int32> &TriangleIndices, TArray<FVector> &Normals)
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

bool FOpenVDBModule::GetVDBGreedyMesh(const FString &gridID, float isovalue, TArray<FVector> &Vertices, TArray<int32> &TriangleIndices, TArray<FVector> &Normals)
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