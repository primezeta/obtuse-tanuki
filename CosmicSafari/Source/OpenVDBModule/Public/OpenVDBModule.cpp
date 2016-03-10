#include "OpenVDBModule.h"
#include "libovdb.h"

DEFINE_LOG_CATEGORY(LogOpenVDBModule)

void FOpenVDBModule::InitializeVDB(const FString &vdbFilename, const FString &gridID)
{
	OvdbInterface = IOvdb::GetIOvdbInstance(TCHAR_TO_UTF8(*vdbFilename), TCHAR_TO_UTF8(*gridID));
	if (!OvdbInterface)
	{
		UE_LOG(LogOpenVDBModule, Fatal, TEXT("Failed to create VDB interface!"));
	}
	
	GridID = gridID;
	static size_t maxStrLen = 256;
	size_t regionsCount = OvdbInterface->ReadMetaRegionCount(TCHAR_TO_UTF8(*gridID));
	TArray<TArray<char*>> strs;
	strs.SetNum(regionsCount);

	for (int i = 0; i < regionsCount; ++i)
	{
		strs[i].SetNum(maxStrLen);
	}
	
	if (OvdbInterface->ReadMetaRegionIDs(strs.GetData()->GetData(), regionsCount, strs.GetAllocatedSize()/regionsCount) < 1)
	{
		UE_LOG(LogOpenVDBModule, Fatal, TEXT("Failed to read regions!"));
	}

	for (TArray<TArray<char*>>::TIterator i = strs.CreateIterator(); i; ++i)
	{
		FString temp = FString(UTF8_TO_TCHAR(i->GetData()));
		RegionIDs.Add(FString::Printf(TEXT("%s"), *temp));
	}
}

void FOpenVDBModule::CreateRegion(const FString &gridID, const FString &regionID, const FIntVector &start, const FIntVector &end)
{
	//Set the region name as the region bounds coordinates
	if (OvdbInterface->DefineRegion(TCHAR_TO_UTF8(*gridID), TCHAR_TO_UTF8(*regionID), start.X, start.Y, start.Z, end.X, end.Y, end.Z, true))
	{
		UE_LOG(LogOpenVDBModule, Fatal, TEXT("Failed to create region!"));
	}
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
	double vx, vy, vz;
	while (OvdbInterface->YieldVertex(TCHAR_TO_UTF8(*regionID), vx, vy, vz))
	{
		Vertices.Add(FVector((float)vx, (float)vy, (float)vz));
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

	double nx, ny, nz;
	while (OvdbInterface->YieldNormal(TCHAR_TO_UTF8(*regionID), nx, ny, nz))
	{
		Normals.Add(FVector((float)nx, (float)ny, (float)nz));
	}
}

IMPLEMENT_GAME_MODULE(FOpenVDBModule, OpenVDBModule);