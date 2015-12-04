// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/Actor.h"
#include "ProceduralTerrainMeshComponent.h"
#include "ProceduralTerrain.generated.h"

UCLASS()
class COSMICSAFARI_API AProceduralTerrain : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AProceduralTerrain();

	// Called when the game starts
	virtual void BeginPlay() override;

	// Called every frame
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ProceduralTerrain")
	float MeshIsovalue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ProceduralTerrain")
	float MeshAdaptivity;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ProceduralTerrain")
	UProceduralTerrainMeshComponent * TerrainMeshComponent;

	UFUNCTION(BlueprintCallable, Category = "ProceduralTerrain")
	bool LoadVdbFile(const FString &vdbFilename, const FString &gridName);

private:
	TArray<FVector> MeshSectionVertices;
	TArray<int32> MeshSectionTriangleIndices;
};