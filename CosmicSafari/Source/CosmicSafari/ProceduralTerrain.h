// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/SceneComponent.h"
#include "ProceduralTerrain.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class COSMICSAFARI_API UProceduralTerrain : public USceneComponent
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	UProceduralTerrain();

	// Called when the game starts
	virtual void BeginPlay() override;

	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ProceduralTerrain")
	float MeshIsovalue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "ProceduralTerrain")
	float MeshAdaptivity;

	UFUNCTION(BlueprintCallable, Category = "ProceduralTerrain")
	bool LoadVdbFile(FString vdbFilename, FString gridName);

	UFUNCTION(BlueprintCallable, Category = "ProceduralTerrain")
	bool GetNextMeshVertex(FVector &vertex);

	UFUNCTION(BlueprintCallable, Category = "ProceduralTerrain")
	bool GetNextTriangleIndex(int32 & index);

private:
	TQueue<FVector> Vertices;
	TQueue<uint32_t> TriangleIndices;
};