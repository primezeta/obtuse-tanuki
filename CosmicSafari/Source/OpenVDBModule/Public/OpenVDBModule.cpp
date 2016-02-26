#include "OpenVDBModule.h"
#include "libovdb.h"

DEFINE_LOG_CATEGORY(LogOpenVDBModule)

void FOpenVDBModule::ReadVDBFile(const FString &vdbFilename, const FString &gridName)
{
	if (OvdbInterface->ReadGrid(*gridName, *vdbFilename))
	{
		UE_LOG(LogOpenVDBModule, Fatal, TEXT("Failed to read vdb file!"));
	}
}

void FOpenVDBModule::CreateDynamicVdb(const FString &gridID, float surfaceValue, const FIntVector &boundsStart, const FIntVector &boundsEnd, int32 range,
	double scaleXYZ, double frequency, double lacunarity, double persistence, int32 octaveCount)
{	
	if (OvdbInterface->CreateLibNoiseGrid(*gridID, boundsEnd.X - boundsStart.X + 1, boundsEnd.Y - boundsStart.Y + 1, range, surfaceValue, scaleXYZ, frequency, lacunarity, persistence, octaveCount))
	{
		UE_LOG(LogOpenVDBModule, Fatal, TEXT("Failed to create dynamic vdb!"));
	}
}

void FOpenVDBModule::CreateGridMeshRegions(const FString &gridID, int32 regionCountX, int32 regionCountY, int32 regionCountZ, TArray<FString> &regionIDs)
{
	int32 rx, ry, rz;
	if (OvdbInterface->MaskRegions(*gridID, regionCountX, regionCountY, regionCountZ, rx, ry, rz))
	{
		UE_LOG(LogOpenVDBModule, Fatal, TEXT("Failed to mask regions!"));
	}
	else
	{
		for (int32 x = 0; x < rx; ++x)
		{
			for (int32 y = 0; y < ry; ++y)
			{
				for (int32 z = 0; z < rz; ++z)
				{
					regionIDs.Add(FString::Printf(TEXT("%d,%d,%d"), x, y, z));
				}
			}
		}
	}
}

void FOpenVDBModule::GetMeshGeometry(const FString &gridID, const FString &meshID, float surfaceValue, TArray<FVector> &Vertices, TArray<int32> &TriangleIndices, TArray<FVector> &Normals)
{
	if (OvdbInterface->RegionToMesh(*gridID, *meshID, IOvdb::PRIMITIVE_CUBES, surfaceValue))
	{
		UE_LOG(LogOpenVDBModule, Fatal, TEXT("Failed to get mesh geometry!"));
	}
	else
	{
		FVector vertex;
		while (OvdbInterface->YieldVertex(*gridID, *meshID, vertex.X, vertex.Y, vertex.Z))
		{
			Vertices.Add(vertex);
		}

		uint32 triangleIndices[3];
		while (OvdbInterface->YieldPolygon(*gridID, *meshID, triangleIndices[0], triangleIndices[1], triangleIndices[2]))
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
		while (OvdbInterface->YieldNormal(*gridID, *meshID, normal.X, normal.Y, normal.Z))
		{
			Normals.Add(normal);
		}
	}
}

IMPLEMENT_GAME_MODULE(FOpenVDBModule, OpenVDBModule);