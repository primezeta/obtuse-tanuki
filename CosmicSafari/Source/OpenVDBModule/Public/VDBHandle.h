// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/ActorComponent.h"
#include "VdbInterface.h"
#include "VoxelData.h"
#include "ProceduralTerrainMeshComponent.h"
#include "VdbHandle.generated.h"

UCLASS()
class OPENVDBMODULE_API UVdbHandle : public UActorComponent //, public IVdbInterface TODO
{
	GENERATED_BODY()

public:
	UVdbHandle(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VDB Handle", Meta = (ToolTip = "Mesh algorithm"))
		EMeshType MeshMethod;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VDB Handle", Meta = (ToolTip = "Path to voxel database"))
		FString FilePath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VDB Handle", Meta = (ToolTip = "Enable delayed loading of grids"))
		bool EnableDelayLoad;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VDB Handle", Meta = (ToolTip = "Enable grid stats metadata"))
		bool EnableGridStats;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VDB Handle", Meta = (ToolTip = "Name of volume"))
		FString WorldName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VDB Handle", Meta = (ToolTip = "Perlin noise random generator seed"))
		int32 PerlinSeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VDB Handle", Meta = (ToolTip = "Perlin noise frequency of first octave"))
		float PerlinFrequency;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VDB Handle", Meta = (ToolTip = "Perlin noise frequency multiplier each successive octave"))
		float PerlinLacunarity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VDB Handle", Meta = (ToolTip = "Perlin noise amplitude multiplier each successive octave"))
		float PerlinPersistence;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VDB Handle", Meta = (ToolTip = "Perlin noise number of octaves"))
		int32 PerlinOctaveCount;

	virtual void InitializeComponent() override;
	virtual void BeginDestroy() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UFUNCTION(BlueprintCallable, Category="VDB Handle")
		virtual FString AddGrid(const FString &gridName, const FIntVector &regionIndex, const FVector &voxelSize, TArray<FGridMeshBuffers> &meshBuffers);
	UFUNCTION(BlueprintCallable, Category = "VDB Handle")
		virtual void RemoveGrid(const FString &gridID);
	UFUNCTION(BlueprintCallable, Category = "VDB Handle")
		virtual void SetRegionScale(const FIntVector &regionScale);
	UFUNCTION(BlueprintCallable, Category = "VDB Handle")
		virtual void ReadGridTree(const FString &gridID, TArray<TEnumAsByte<EVoxelType>> &sectionMaterialIDs);
	UFUNCTION(BlueprintCallable, Category = "VDB Handle")
		virtual void GetVoxelCoord(const FString &gridID, const FVector &worldLocation, FIntVector &outVoxelCoord);
	UFUNCTION(BlueprintCallable, Category = "VDB Handle")
		virtual void MeshGrid(const FString &gridID);
	UFUNCTION(BlueprintCallable, Category = "VDB Handle")
		void GetGridDimensions(const FString &gridID, FVector &worldStart, FVector &worldEnd, FVector &firstActive);

	TArray<FString> GetAllGridIDs();
	FIntVector GetRegionIndex(const FVector &worldLocation);
	void WriteAllGrids();

private:
	bool isRegistered;
};