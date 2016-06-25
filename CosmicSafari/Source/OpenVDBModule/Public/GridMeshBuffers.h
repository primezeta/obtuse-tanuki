#pragma once
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

	void ClearBuffers()
	{
		VertexBuffer.Empty();
		NormalBuffer.Empty();
		VertexColorBuffer.Empty();
		TangentBuffer.Empty();
		for (int32 i = 0; i < FVoxelData::VOXEL_TYPE_COUNT; ++i)
		{
			PolygonBuffer[i].Empty();
			UVMapBuffer[i].Empty();
		}
	}

	VertexBufferType VertexBuffer;
	NormalBufferType NormalBuffer;
	VertexColorBufferType VertexColorBuffer;
	TangentBufferType TangentBuffer;
	PolygonBufferType PolygonBuffer[FVoxelData::VOXEL_TYPE_COUNT];
	UVMapBufferType UVMapBuffer[FVoxelData::VOXEL_TYPE_COUNT];
};
typedef TMap<FString, FGridMeshBuffers> GridMeshBuffersType;