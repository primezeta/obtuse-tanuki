// Fill out your copyright notice in the Description page of Project Settings.

#include "CosmicSafari.h"
#include <fstream>
#include <sstream>
#include "ProceduralTerrainMeshComponent.h"

void UProceduralTerrainMeshComponent::CreateTerrainMeshSection(int32 SectionIndex, bool bCreateCollision,
	                                                           const TArray<FVector>& Vertices, const TArray<int32>& Triangles,
	                                                           const TArray<FVector2D> &UV0, const TArray<FVector> &Normals,
	                                                           const TArray<FColor> &VertexColors, const TArray<FProcMeshTangent> &Tangents)
{
	Super::CreateMeshSection(SectionIndex, Vertices, Triangles, Normals, UV0, VertexColors, Tangents, bCreateCollision);
}