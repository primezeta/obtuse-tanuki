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

	UPROPERTY()
		EMeshType MeshMethod;
	UPROPERTY()
		FString FilePath;
	UPROPERTY()
		bool EnableDelayLoad;
	UPROPERTY()
		bool EnableGridStats;
	UPROPERTY()
		FString WorldName;
	UPROPERTY()
		int32 PerlinSeed;
	UPROPERTY()
		float PerlinFrequency;
	UPROPERTY()
		float PerlinLacunarity;
	UPROPERTY()
		float PerlinPersistence;
	UPROPERTY()
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
		virtual void ReadGridTree(const FString &gridID, TArray<TEnumAsByte<EVoxelType>> &sectionMaterialIDs);
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