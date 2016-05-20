// Fill out your copyright notice in the Description page of Project Settings.

#include "OpenVDBModule.h"
#include "ProceduralTerrainMeshComponent.h"

UProceduralTerrainMeshComponent::UProceduralTerrainMeshComponent(const FObjectInitializer& ObjectInitializer)
{
	MeshName = TEXT("DefaultMeshName");
}

inline void UProceduralTerrainMeshComponent::CreateTerrainMeshSection(int32 SectionIndex)
{
	Super::CreateMeshSection(SectionIndex, (*VertexBufferPtrs)[SectionIndex], (*PolygonBufferPtrs)[SectionIndex], (*NormalBufferPtrs)[SectionIndex], (*UVMapBufferPtrs)[SectionIndex], (*VertexColorsBufferPtrs)[SectionIndex], (*TangentsBufferPtrs)[SectionIndex], CreateCollision);
}

inline void UProceduralTerrainMeshComponent::UpdateTerrainMeshSection(int32 SectionIndex)
{
	Super::UpdateMeshSection(SectionIndex, (*VertexBufferPtrs)[SectionIndex], (*NormalBufferPtrs)[SectionIndex], (*UVMapBufferPtrs)[SectionIndex], (*VertexColorsBufferPtrs)[SectionIndex], (*TangentsBufferPtrs)[SectionIndex]);
}