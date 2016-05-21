// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/ActorComponent.h"
#include "VdbInterface.h"
#include "VoxelData.h"
#include "ProceduralTerrainMeshComponent.h"
#include "VdbHandle.generated.h"

//USTRUCT()
//struct FScaleTransformStruct
//{
//	GENERATED_USTRUCT_BODY()
//
//	UPROPERTY(EditAnywhere, Category = "Scale Transform Struct")
//	FVector Scale;
//};
//
//USTRUCT()
//struct FTranslateTransformStruct
//{
//	GENERATED_USTRUCT_BODY()
//
//	UPROPERTY(EditAnywhere, Category = "Translate Transform Struct")
//	FVector Translation;
//};
//
//USTRUCT()
//struct FScaleTranslateTransformStruct
//{
//	GENERATED_USTRUCT_BODY()
//
//	UPROPERTY(EditAnywhere, Category = "Scale Translate Transform Struct")
//	FVector Scale;
//	UPROPERTY(EditAnywhere, Category = "Scale Translate Transform Struct")
//	FVector Translation;
//};
//
//USTRUCT()
//struct FUniformScaleTransformStruct
//{
//	GENERATED_USTRUCT_BODY()
//
//	UPROPERTY(EditAnywhere, Category = "Uniform Scale Transform Struct")
//	double UniformScale;
//};
//
//USTRUCT()
//struct FUniformScaleTranslateTransformStruct
//{
//	GENERATED_USTRUCT_BODY()
//
//	UPROPERTY(EditAnywhere, Category = "Uniform Scale Translate Transform Struct")
//	double UniformScale;
//	UPROPERTY(EditAnywhere, Category = "Uniform Scale Translate Transform Struct")
//	FVector Translation;
//};
//
//USTRUCT()
//struct FAffineTransformStruct
//{
//	GENERATED_USTRUCT_BODY()
//
//	UPROPERTY(EditAnywhere, Category = "Affine Transform Struct")
//	FVector MatrixColumnA;
//	UPROPERTY(EditAnywhere, Category = "Affine Transform Struct")
//	FVector MatrixColumnB;
//	UPROPERTY(EditAnywhere, Category = "Affine Transform Struct")
//	FVector MatrixColumnC;
//};
//
//USTRUCT()
//struct FUnitaryTransformStruct
//{
//	GENERATED_USTRUCT_BODY()
//
//	UPROPERTY(EditAnywhere, Category = "Unitary Transform Struct")
//	FVector Axis;
//	UPROPERTY(EditAnywhere, Category = "Unitary Transform Struct")
//	double Radians;
//};
//
//USTRUCT()
//struct FNonlinearFrustumTransformStruct
//{
//	GENERATED_USTRUCT_BODY()
//
//	UPROPERTY(EditAnywhere, Category = "Nonlinear Frustum Transform Struct")
//	FVector Position;
//	UPROPERTY(EditAnywhere, Category = "Nonlinear Frustum Transform Struct")
//	FVector Size;
//	UPROPERTY(EditAnywhere, Category = "Nonlinear Frustum Transform Struct")
//	double Taper;
//	UPROPERTY(EditAnywhere, Category = "Nonlinear Frustum Transform Struct")
//	double Depth;
//};

UCLASS()
class OPENVDBMODULE_API UVdbHandle : public UActorComponent, public IVdbInterface
{
	GENERATED_BODY()

public:
	UVdbHandle(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VDB Handle", Meta = (ToolTip = "Mesh algorithm"))
		EMeshType MeshMethod;

	UPROPERTY(BlueprintReadOnly, Category = "VDB Handle", Meta = (ToolTip = "Terrain mesh component of each grid"))
		TArray<UProceduralTerrainMeshComponent*> TerrainMeshComponents;

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
		virtual FString AddGrid(const FString &gridName, const FIntVector &regionIndex, const FVector &voxelSize, bool bCreateCollision) override;
	UFUNCTION(BlueprintCallable, Category = "VDB Handle")
		virtual void RemoveGrid(const FString &gridID) override;
	UFUNCTION(BlueprintCallable, Category = "VDB Handle")
		virtual void SetRegionScale(const FIntVector &regionScale) override;
	UFUNCTION(BlueprintCallable, Category = "VDB Handle")
		virtual void ReadGridTrees() override;
	UFUNCTION(BlueprintCallable, Category = "VDB Handle")
		virtual void GetVoxelCoord(const FString &gridID, const FVector &worldLocation, FIntVector &outVoxelCoord) override;
	UFUNCTION(BlueprintCallable, Category = "VDB Handle")
		virtual void MeshGrids(FVector &worldStart,
			FVector &worldEnd,
			TArray<FVector> &startLocations) override;

	TArray<FString> GetAllGridIDs();
	FIntVector GetRegionIndex(const FVector &worldLocation);
	void WriteAllGrids();
};