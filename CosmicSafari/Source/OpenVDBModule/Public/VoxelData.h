#pragma once
#include "EngineMinimal.h"
#include "VoxelData.generated.h"

//TODO: Replace TEnumAsByte usage because according to https://docs.unrealengine.com/latest/INT/Programming/Development/CodingStandard/index.html
//don't have to use TEnumAsByte if enum class is a uint8.
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

//Interesting:
//ENUM_CLASS_FLAGS(EnumType)

UENUM(BlueprintType)		//"BlueprintType" is essential to include
enum class EGridState : uint8
{
	GRID_STATE_NULL     			UMETA(DisplayName = "Null"),				//Grid is in an undefined state
	GRID_STATE_EMPTY				UMETA(DisplayName = "Empty"),				//Grid is defined but without meta or tree data
	GRID_STATE_DATA_DESYNC			UMETA(DisplayName = "DataDesync"),			//Grid data value(s) were changed
	GRID_STATE_ACTIVE_STATES_DESYNC	UMETA(DisplayName = "ActiveStatesDesync"),	//Grid active state(s) were changed
	GRID_STATE_CLEAN				UMETA(DisplayName = "Clean"),				//Grid is meshed and should be rendered
	GRID_STATE_RENDERED				UMETA(DisplayName = "Rendered"),			//Grid is meshed and rendered and needs collision calculated (if configured to do so)
	GRID_STATE_COMPLETE				UMETA(DisplayName = "Complete"),			//All steps complete
};

const static int32 NUM_GRID_STATES = (int32)EGridState::GRID_STATE_COMPLETE;

/**
 *
 */
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