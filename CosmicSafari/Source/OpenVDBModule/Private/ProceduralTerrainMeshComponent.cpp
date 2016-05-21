// Fill out your copyright notice in the Description page of Project Settings.

#include "OpenVDBModule.h"
#include "ProceduralTerrainMeshComponent.h"

UProceduralTerrainMeshComponent::UProceduralTerrainMeshComponent(const FObjectInitializer& ObjectInitializer)
{
	MeshName = TEXT("DefaultMeshName");
}

inline void UProceduralTerrainMeshComponent::CreateTerrainMeshSection()
{
	Super::CreateMeshSection(SectionIndex, *VertexBufferPtr, *PolygonBufferPtr, *NormalBufferPtr, *UVMapBufferPtr, *VertexColorsBufferPtr, *TangentsBufferPtr, CreateCollision);
}

inline void UProceduralTerrainMeshComponent::UpdateTerrainMeshSection()
{
	Super::UpdateMeshSection(SectionIndex, *VertexBufferPtr, *NormalBufferPtr, *UVMapBufferPtr, *VertexColorsBufferPtr, *TangentsBufferPtr);
}