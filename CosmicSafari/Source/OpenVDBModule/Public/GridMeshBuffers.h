#pragma once
#include "VoxelData.h"
#include "GridMeshBuffers.generated.h"

typedef TArray<FVector> VertexBufferType;
typedef TArray<int32> PolygonBufferType;
typedef TArray<FVector> NormalBufferType;
typedef TArray<FVector2D> UVMapBufferType;
typedef TArray<FColor> VertexColorBufferType;
typedef TArray<FProcMeshTangent> TangentBufferType;

USTRUCT()
struct FGridMeshBuffers
{
	GENERATED_USTRUCT_BODY()
	VertexBufferType VertexBuffer;
	NormalBufferType NormalBuffer;
	VertexColorBufferType VertexColorBuffer;
	TangentBufferType TangentBuffer;
	PolygonBufferType PolygonBuffer[FVoxelData::VOXEL_TYPE_COUNT];
	UVMapBufferType UVMapBuffer[FVoxelData::VOXEL_TYPE_COUNT];
};
typedef TMap<FString, FGridMeshBuffers> GridMeshBuffersType;