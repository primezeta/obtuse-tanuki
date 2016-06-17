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
		PolygonBuffer.Empty();
		NormalBuffer.Empty();
		UVMapBuffer.Empty();
		VertexColorBuffer.Empty();
		TangentBuffer.Empty();
	}

	VertexBufferType VertexBuffer;
	PolygonBufferType PolygonBuffer;
	NormalBufferType NormalBuffer;
	UVMapBufferType UVMapBuffer;
	VertexColorBufferType VertexColorBuffer;
	TangentBufferType TangentBuffer;
};