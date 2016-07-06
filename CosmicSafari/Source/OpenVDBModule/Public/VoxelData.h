#pragma once
#include "EngineMinimal.h"
#include "VoxelData.generated.h"

UENUM(BlueprintType)
enum class EVoxelType : uint8
{
	VOXEL_WATER		UMETA(DisplayName = "Water"),
	VOXEL_ROCK		UMETA(DisplayName = "Rock"),
	VOXEL_DIRT		UMETA(DisplayName = "Dirt"),
	VOXEL_GRASS		UMETA(DisplayName = "Grass"),
	VOXEL_SNOW		UMETA(DisplayName = "Snow"),
	VOXEL_NONE		UMETA(DisplayName = "None"), //Always leave NONE as the last
};

UENUM(BlueprintType)		//"BlueprintType" is essential to include
enum class EMeshType : uint8
{
	MESH_TYPE_CUBES 		 UMETA(DisplayName = "Mesh as basic cubes"),
	MESH_TYPE_MARCHING_CUBES UMETA(DisplayName = "Mesh with marching cubes"),
};

UENUM(BlueprintType)		//"BlueprintType" is essential to include
enum class EGridState : uint8
{
	GRID_STATE_INIT 				UMETA(DisplayName = "Init"),
	GRID_STATE_READ_TREE			UMETA(DisplayName = "Read from file"),
	GRID_STATE_FILL_VALUES			UMETA(DisplayName = "Fill values"),
	GRID_STATE_EXTRACT_SURFACE		UMETA(DisplayName = "Extract isosurface"),
	GRID_STATE_MESH					UMETA(DisplayName = "Mesh isosurface"),
	GRID_STATE_READY   			    UMETA(DisplayName = "Ready to render on the game thread"),
	GRID_STATE_FINISHED				UMETA(DisplayName = "Nothing left to do"),
};

const static int32 NUM_GRID_STATES = (int32)EGridState::GRID_STATE_FINISHED;

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

	const static int32 VOXEL_TYPE_COUNT = (int32)EVoxelType::VOXEL_NONE;
};

//One state per actual grid state except GRID_STATE_READY which has a number of states according to number of voxel types (except VOXEL_NONE)
const static int32 NUM_TOTAL_GRID_STATES = (NUM_GRID_STATES - 1) + (FVoxelData::VOXEL_TYPE_COUNT);