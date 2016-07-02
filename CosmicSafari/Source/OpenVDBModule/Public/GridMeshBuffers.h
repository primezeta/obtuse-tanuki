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

	UPROPERTY()
		TArray<FVector> VertexBuffer;
	UPROPERTY()
		TArray<FVector> NormalBuffer;
	UPROPERTY()
		TArray<FColor> VertexColorBuffer;
	UPROPERTY()
		TArray<FProcMeshTangent> TangentBuffer;
	UPROPERTY()
		TArray<int32> PolygonBuffer;
	UPROPERTY()
		TArray<FVector2D> UVMapBuffer;
	UPROPERTY()
		TArray<int32> NumTris;
};