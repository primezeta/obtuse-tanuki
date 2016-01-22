// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "ProceduralMeshComponent.h"
#include "ProceduralTerrainMeshComponent.generated.h"

/**
 * 
 */
UCLASS()
class COSMICSAFARI_API UProceduralTerrainMeshComponent : public UProceduralMeshComponent
{
	GENERATED_BODY()

public:
	void CreateTerrainMeshSection(int32 SectionIndex, bool bCreateCollision,
		const TArray<FVector>& Vertices, const TArray<int32>& Triangles,
		const TArray<FVector2D> &UV0, const TArray<FVector> &Normals,
		const TArray<FColor> &VertexColors, const TArray<FProcMeshTangent> &Tangents);
	
};
