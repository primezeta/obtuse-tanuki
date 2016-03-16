#include "OpenVDBModule.h"
#include "libovdb.h"

DEFINE_LOG_CATEGORY(LogOpenVDBModule)

FString FOpenVDBModule::InitializeVDB(const FString &vdbFilename, const FString &gridID, const FVector &scale, const FIntVector &dimensions)
{
	OvdbInterface = IOvdb::GetIOvdbInstance(TCHAR_TO_UTF8(*vdbFilename));
	if (!OvdbInterface)
	{
		UE_LOG(LogOpenVDBModule, Fatal, TEXT("Failed to create VDB interface!"));
	}	
	if (OvdbInterface->DefineGrid(TCHAR_TO_UTF8(*gridID), scale.X, scale.Y, scale.Z, 0, 0, 0, dimensions.X, dimensions.Y, dimensions.Z))
	{
		UE_LOG(LogOpenVDBModule, Fatal, TEXT("Failed to create define grid!"));
	}
	GridID = gridID;

	static size_t maxStrLen = 256;
	size_t regionsCount = OvdbInterface->ReadMetaRegionCount();
	if (regionsCount > 0)
	{
		//First reserve space for UTF8 c-style string names
		TArray<TArray<char*>> strs;
		strs.SetNum(regionsCount);
		for (int i = 0; i < regionsCount; ++i)
		{
			strs[i].SetNum(maxStrLen);
		}

		//Read region IDs into the reserved string buffers
		if (OvdbInterface->ReadMetaRegionIDs(strs.GetData()->GetData(), regionsCount, strs.GetAllocatedSize() / regionsCount) < 1)
		{
			UE_LOG(LogOpenVDBModule, Fatal, TEXT("Failed to read regions!"));
		}

		//Finally convert region IDs to FString
		for (TArray<TArray<char*>>::TIterator i = strs.CreateIterator(); i; ++i)
		{
			FString temp = FString(UTF8_TO_TCHAR(i->GetData()));
			RegionIDs.Add(FString::Printf(TEXT("%s"), *temp));
		}
	}
	return GridID;
}

FString FOpenVDBModule::CreateRegion(const FString &gridName, const FString &regionName, const FIntVector &start, const FIntVector &end)
{
	char regionIDStr[256];
	if(OvdbInterface->DefineRegion(TCHAR_TO_UTF8(*gridName), TCHAR_TO_UTF8(*regionName), start.X, start.Y, start.Z, end.X, end.Y, end.Z, regionIDStr, 256))
	{
		UE_LOG(LogOpenVDBModule, Fatal, TEXT("Failed to define the region!"));
	}
	if (RegionIDs.Find(regionIDStr) == INDEX_NONE)
	{
		RegionIDs.Add(regionIDStr);
	}
	return regionIDStr;
}

void FOpenVDBModule::LoadRegion(const FString &regionID)
{
	if (OvdbInterface->LoadRegion(TCHAR_TO_UTF8(*regionID)))
	{
		UE_LOG(LogOpenVDBModule, Fatal, TEXT("Failed to load region!"));
	}
}

void FOpenVDBModule::FillRegionWithPerlinDensity(const FString &regionID, double frequency, double lacunarity, double persistence, int32 octaveCount)
{
	if (OvdbInterface->PopulateRegionDensityPerlin(TCHAR_TO_UTF8(*regionID), frequency, lacunarity, persistence, octaveCount))
	{
		UE_LOG(LogOpenVDBModule, Fatal, TEXT("Failed to generate Perlin density grid!"));
	}
	if (OvdbInterface->WriteChanges())
	{
		UE_LOG(LogOpenVDBModule, Fatal, TEXT("Failed to write Perlin density to file!"));
	}
}

void FOpenVDBModule::GenerateMesh(const FString &regionID, float surfaceValue)
{
	if (OvdbInterface->MeshRegion(TCHAR_TO_UTF8(*regionID), surfaceValue))
	{
		UE_LOG(LogOpenVDBModule, Fatal, TEXT("Failed to generate mesh!"));
	}
}

void FOpenVDBModule::GetMeshGeometry(const FString &regionID, TArray<FVector> &Vertices, TArray<int32> &TriangleIndices, TArray<FVector> &Normals)
{
	Vertices.Reserve(OvdbInterface->VertexCount(TCHAR_TO_UTF8(*regionID)));
	TriangleIndices.Reserve(3*OvdbInterface->VertexCount(TCHAR_TO_UTF8(*regionID)));
	Normals.Reserve(OvdbInterface->VertexCount(TCHAR_TO_UTF8(*regionID)));

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