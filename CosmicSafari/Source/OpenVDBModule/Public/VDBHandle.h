// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/ActorComponent.h"
#include "VdbInterface.h"
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
	static UVdbHandle const * RegisterNewVdb(const FObjectInitializer& ObjectInitializer, UObject * parent, const FString &path, const FString &worldName, bool enableDelayLoad, bool enableGridStats);
	static void RegisterVdb(UVdbHandle const * VdbHandle);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Meta = (ToolTip = "Path to voxel database"))
		FString FilePath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Meta = (ToolTip = "Enable delayed loading of grids"))
		bool EnableDelayLoad;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Meta = (ToolTip = "Enable grid stats metadata"))
		bool EnableGridStats;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Meta = (ToolTip = "Name of volume"))
		FString WorldName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Meta = (ToolTip = "Voxels per dimension of a region"))
		FIntVector RegionVoxelCount;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Meta = (ToolTip = "Perlin noise source frequency"))
		float PerlinFrequency;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Meta = (ToolTip = "Perlin noise source lacunarity"))
		float PerlinLacunarity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Meta = (ToolTip = "Perlin noise source persistence"))
		float PerlinPersistence;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Meta = (ToolTip = "Perlin noise source number of octaves"))
		int32 PerlinOctaveCount;

	UVdbHandle(const FObjectInitializer& ObjectInitializer);

	virtual void PostInitProperties() override;
	virtual void BeginDestroy() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UFUNCTION(BlueprintCallable, Category="VDB Handle")
		virtual FString AddGrid(const FString &gridName, const FIntVector &worldIndex, FIntVector &indexStart, FIntVector &indexEnd) override;
	UFUNCTION(BlueprintCallable, Category = "VDB Handle")
		virtual void RemoveGrid(const FString &gridID) override;
	UFUNCTION(BlueprintCallable, Category = "VDB Handle")
		virtual void SetRegionSize(const FIntVector &regionSize) override;
	UFUNCTION(BlueprintCallable, Category = "VDB Handle")
		virtual void ReadGridTreeIndex(const FString &gridID, FIntVector &activeStart, FIntVector &activeEnd) override;
	//TODO
	//UFUNCTION(BlueprintCallable, Category = "VDB Handle")
	//	virtual void ReadGridTreeWorld(const FString &gridID, FVector &activeStart, FVector &activeEnd) override;
	UFUNCTION(BlueprintCallable, Category = "VDB Handle")
		virtual void MeshGrid(const FString &gridID, float surfaceValue, TArray<FVector> &vertexBuffer, TArray<int32> &polygonBuffer, TArray<FVector> &normalBuffer) override;
	UFUNCTION(BlueprintCallable, Category = "VDB Handle")
		virtual void ReadGridIndexBounds(const FString &gridID, FIntVector &indexStart, FIntVector &indexEnd) override;
	UFUNCTION(BlueprintCallable, Category = "VDB Handle")
		virtual int32 ReadGridCount() override;

	TArray<FString> GetAllGridIDs();
	FIntVector GetRegionIndex(const FVector &worldLocation);
	void InitVdb();

private:
	void FinishVdb();
};