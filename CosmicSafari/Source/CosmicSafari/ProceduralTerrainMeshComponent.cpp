// Fill out your copyright notice in the Description page of Project Settings.

#include "CosmicSafari.h"
#include "ProceduralTerrainMeshComponent.h"

void UProceduralTerrainMeshComponent::CreateTerrainMeshSection(int32 SectionIndex, const TArray<FVector>& Vertices, const TArray<int32>& Triangles)
{
	TArray<FVector> Normals;
	TArray<FVector2D>UV0;
	TArray<FColor> VertexColors;
	TArray<FProcMeshTangent> Tangents;
	bool bCreateCollision = false;
	Super::CreateMeshSection(SectionIndex, Vertices, Triangles, Normals, UV0, VertexColors, Tangents, bCreateCollision);
}