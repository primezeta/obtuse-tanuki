// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/ActorComponent.h"
#include "ProceduralMeshComponent.h"
#include "VoxelData.h"
#include "VdbHandle.generated.h"

UCLASS()
class OPENVDBMODULE_API UVdbHandle : public UActorComponent //, public IVdbInterface TODO
{
	GENERATED_BODY()

public:
	UVdbHandle(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VDB", Meta = (ToolTip = "Mesh algorithm for this grid"))
		EMeshType MeshMethod;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VDB", Meta = (ToolTip = "Path to the grid database"))
		FString FilePath;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VDB", Meta = (ToolTip = "Enable loading tree data seperately"))
		bool EnableDelayLoad;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VDB", Meta = (ToolTip = "Enable grid stats metadata"))
		bool EnableGridStats;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VDB", Meta = (ToolTip = "World that contains this grid"))
		FString WorldName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VDB", Meta = (ToolTip = "Perlin noise random seed"))
		int32 PerlinSeed;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VDB", Meta = (ToolTip = "Perlin noise starting frequency"))
		float PerlinFrequency;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VDB", Meta = (ToolTip = "Perlin noise frequency multiplier per octave"))
		float PerlinLacunarity;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VDB", Meta = (ToolTip = "Perlin noise amplitutde multiplier per octave"))
		float PerlinPersistence;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VDB", Meta = (ToolTip = "Perlin noise number of octacves"))
		int32 PerlinOctaveCount;

	virtual void InitializeComponent() override;
	virtual void BeginDestroy() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UFUNCTION(BlueprintCallable, Category="VDB Handle")
		virtual FString AddGrid(const FString &gridName, const FIntVector &regionIndex, const FVector &voxelSize, TArray<FProcMeshSection> &sectionBuffers);
	UFUNCTION(BlueprintCallable, Category = "VDB Handle")
		virtual void RemoveGrid(const FString &gridID);
	UFUNCTION(BlueprintCallable, Category = "VDB Handle")
		virtual void SetRegionScale(const FIntVector &regionScale);
	UFUNCTION(BlueprintCallable, Category = "VDB Handle")
		virtual void ReadGridTree(const FString &gridID, TArray<TEnumAsByte<EVoxelType>> &sectionMaterialIDs, FVector &initialLocation);
	UFUNCTION(BlueprintCallable, Category = "VDB Handle")
		virtual void GetVoxelCoord(const FString &gridID, const FVector &worldLocation, FIntVector &outVoxelCoord);
	UFUNCTION(BlueprintCallable, Category = "VDB Handle")
		virtual void MeshGrid(const FString &gridID);
	UFUNCTION(BlueprintCallable, Category = "VDB Handle")
		void GetGridDimensions(const FString &gridID, FBox &worldBounds, FVector &firstActive);

	TArray<FString> GetAllGridIDs();
	FIntVector GetRegionIndex(const FVector &worldLocation);
	void WriteAllGrids();

private:
	bool isRegistered;
};