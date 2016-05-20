#pragma once
#include "EngineMinimal.h"
#include "VoxelData.generated.h"

USTRUCT()
struct FVoxelData
{
	GENERATED_USTRUCT_BODY()

	typedef float DataType;
	typedef int32 MaterialType;

	UPROPERTY()
		float Data;
	UPROPERTY()
		int32 MaterialID;

	FVoxelData()
	{
		Data = (float)0;
		MaterialID = (int32)-1;
	}

	FVoxelData(const float &data, const int32 &materialID)
		: Data(data), MaterialID(materialID)
	{
	}
};