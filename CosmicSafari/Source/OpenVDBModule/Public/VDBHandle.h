// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/ActorComponent.h"
#include "ProceduralMeshComponent.h"
#include "VoxelData.h"
#include "VdbHandle.generated.h"

/**
*
*/
UCLASS()
class OPENVDBMODULE_API UVdbHandle : public UActorComponent
{
	GENERATED_BODY()

public:
	UVdbHandle(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration", Meta = (ToolTip = "Mesh algorithm for this grid"))
		EMeshType MeshMethod;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration", Meta = (ToolTip = "Path to the grid database"))
		FString FilePath;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration", Meta = (ToolTip = "Enable loading tree data seperately"))
		bool EnableDelayLoad;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration", Meta = (ToolTip = "Enable grid stats metadata"))
		bool EnableGridStats;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration", Meta = (ToolTip = "World that contains this grid"))
		FString WorldName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration", Meta = (ToolTip = "Perlin noise random seed"))
		int32 PerlinSeed;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration", Meta = (ToolTip = "Perlin noise starting frequency"))
		float PerlinFrequency;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration", Meta = (ToolTip = "Perlin noise frequency multiplier per octave"))
		float PerlinLacunarity;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration", Meta = (ToolTip = "Perlin noise amplitutde multiplier per octave"))
		float PerlinPersistence;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration", Meta = (ToolTip = "Perlin noise number of octacves"))
		int32 PerlinOctaveCount;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration", Meta = (ToolTip = "Whether or not to run grid operations multithreaded"))
		bool ThreadedGridOps;

	virtual void InitializeComponent() override;
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction);
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void BeginDestroy() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	void RunVoxelDatabaseTask(const FString &ThreadName, TFunction<void(void)> &&Task);

	UFUNCTION(BlueprintCallable, Category="VDB")
		FString AddGrid(const FString &gridName, const FIntVector &regionIndex, const FVector &voxelSize, TArray<FProcMeshSection> &sectionBuffers);
	UFUNCTION(BlueprintCallable, Category = "VDB")
		void ReadGridTree(const FString &gridID, FIntVector &startIndex, FIntVector &endIndex);
	UFUNCTION(BlueprintCallable, Category = "VDB")
		bool FillTreePerlin(const FString &gridID, FIntVector &startFill, FIntVector &endFill);
	UFUNCTION(BlueprintCallable, Category = "VDB")
		bool ExtractIsoSurface(const FString &gridID, TArray<TEnumAsByte<EVoxelType>> &sectionMaterialIDs, FBox &gridDimensions, FVector &initialLocation);
	UFUNCTION(BlueprintCallable, Category = "VDB")
		void MeshGrid(const FString &gridID);
	UFUNCTION(BlueprintCallable, Category = "VDB")
		void RemoveGrid(const FString &gridID);
	UFUNCTION(BlueprintCallable, Category = "VDB")
		bool SetRegionScale(const FIntVector &regionScale);
	UFUNCTION(BlueprintCallable, Category = "VDB")
		void GetVoxelCoord(const FString &gridID, const FVector &worldLocation, FIntVector &outVoxelCoord);

	//TODO: Make ufunctions for getting grid dimensions
	bool GetGridDimensions(const FString &gridID, FVector &startLocation);
	bool GetGridDimensions(const FString &gridID, FBox &worldBounds);
	bool GetGridDimensions(const FString &gridID, FBox &worldBounds, FVector &startLocation);

	TArray<FString> GetAllGridIDs();
	FIntVector GetRegionIndex(const FVector &worldLocation);
	void WriteAllGrids();
	const FString VdbName;

private:
	bool IsOpen;
	void OpenVoxelDatabaseGuard();
	void CloseVoxelDatabaseGuard();
};