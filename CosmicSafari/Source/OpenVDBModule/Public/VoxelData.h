#pragma once
#include "EngineMinimal.h"
#include "VoxelData.generated.h"

UENUM(BlueprintType)
enum class EVoxelType : uint8
{
	VOXEL_NONE		UMETA(DisplayName = "None"),
	VOXEL_WATER		UMETA(DisplayName = "Water"),
	VOXEL_ROCK		UMETA(DisplayName = "Rock"),
	VOXEL_DIRT		UMETA(DisplayName = "Dirt"),
	VOXEL_GRASS		UMETA(DisplayName = "Grass"),
	VOXEL_SNOW		UMETA(DisplayName = "Snow"),
};
const static int32 VOXEL_TYPE_COUNT = (int32)EVoxelType::VOXEL_SNOW;

USTRUCT()
struct FVoxelData
{
	GENERATED_USTRUCT_BODY()

	typedef float DataType;

	UPROPERTY()
		float Data;
	UPROPERTY()
		EVoxelType VoxelType;

	FVoxelData()
	{
		Data = (float)0;
		VoxelType = EVoxelType::VOXEL_NONE;
	}

	FVoxelData(const float &data, const EVoxelType &voxelType)
		: Data(data), VoxelType(voxelType)
	{
	}

	static int32 NumMaterials() { return VOXEL_TYPE_COUNT; }
};