#include "OpenVDBModule.h"
#include "libovdb.h"

DEFINE_LOG_CATEGORY(LogOpenVDBModule)

void FOpenVDBModule::InitializeVDB(const FString &vdbFilename, const FString &gridID)
{
	OvdbInterface = GetIOvdbInstance(TCHAR_TO_UTF8(*vdbFilename));
	if (!OvdbInterface)
	{
		UE_LOG(LogOpenVDBModule, Fatal, TEXT("Failed to open OVDB interface!"));
	}
	if (OvdbInterface->InitializeGrid(TCHAR_TO_UTF8(*gridID)))
	{
		UE_LOG(LogOpenVDBModule, Fatal, TEXT("Failed to open vdb file!"));
	}
}

FString FOpenVDBModule::CreateRegion(const FString &gridID, const FIntVector &start, const FIntVector &end)
{
	//Set the region name as the region bounds coordinates
	const static int maxNameLen = 256;
	char nameCStr[maxNameLen];	
	if (OvdbInterface->DefineRegion(start.X, start.Y, start.Z, end.X, end.Y, end.Z, nameCStr, maxNameLen))
	{
		UE_LOG(LogOpenVDBModule, Fatal, TEXT("Failed to create region!"));
	}
	return FString::Printf(TEXT("%s"), UTF8_TO_TCHAR(nameCStr));
}

void FOpenVDBModule::LoadRegion(const FString &regionID)
{
	if (OvdbInterface->LoadRegion(TCHAR_TO_UTF8(*regionID)))
	{
		UE_LOG(LogOpenVDBModule, Fatal, TEXT("Failed to load region!"));
	}
}

void FOpenVDBModule::FillRegionWithPerlinDensity(const FString &regionID, double scaleXYZ, double frequency, double lacunarity, double persistence, int32 octaveCount)
{
	if (OvdbInterface->PopulateRegionDensityPerlin(TCHAR_TO_UTF8(*regionID), scaleXYZ, frequency, lacunarity, persistence, octaveCount))
	{
		UE_LOG(LogOpenVDBModule, Fatal, TEXT("Failed to generate Perlin density grid!"));
	}
}

void FOpenVDBModule::GenerateMesh(const FString &regionID, float surfaceValue)
{
	if (OvdbInterface->MeshRegion(TCHAR_TO_UTF8(*regionID), surfaceValue))
	{
		UE_LOG(LogOpenVDBModule, Fatal, TEXT("Failed to create dynamic vdb!"));
	}
}

void FOpenVDBModule::GetMeshGeometry(const FString &regionID, TArray<FVector> &Vertices, TArray<int32> &TriangleIndices, TArray<FVector> &Normals)
{
	FVector vertex;
	while (OvdbInterface->YieldVertex(TCHAR_TO_UTF8(*regionID), vertex.X, vertex.Y, vertex.Z))
	{
		Vertices.Add(vertex);
	}

	uint32 triangleIndices[3];
	while (OvdbInterface->YieldPolygon(TCHAR_TO_UTF8(*regionID), triangleIndices[0], triangleIndices[1], triangleIndices[2]))
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
	while (OvdbInterface->YieldNormal(TCHAR_TO_UTF8(*regionID), normal.X, normal.Y, normal.Z))
	{
		Normals.Add(normal);
	}
}

IMPLEMENT_GAME_MODULE(FOpenVDBModule, OpenVDBModule);