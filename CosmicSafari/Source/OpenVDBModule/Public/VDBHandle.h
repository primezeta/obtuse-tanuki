#pragma once
#include "EngineMinimal.h"
#include "VDBInterface.h"
#include "VDBHandle.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogVDBHandle, Log, All)

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
class UVDBHandle : public UObject, public IVDBInterface
{
	GENERATED_BODY()

public:
	UVDBHandle(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere)
	FString FilePath;

	UPROPERTY(EditAnywhere)
	bool EnableDelayLoad;

	UPROPERTY(EditAnywhere)
	bool EnableGridStats;

	UPROPERTY(EditAnywhere)
	FString WorldName;

	virtual void Initialize(const FString &path, bool enableDelayLoad, bool enableGridStats) override;
	virtual FString AddGrid(const FString &gridName, const FIntVector &indexStart, const FIntVector &indexEnd) override;
	virtual void RemoveGrid(const FString &gridID) override;
	virtual void ReadGridTree(const FString &gridID, FIntVector &indexStart, FIntVector &indexEnd) override;
	virtual void MeshGrid(const FString &gridID, float surfaceValue, TArray<FVector> &vertexBuffer, TArray<int32> &polygonBuffer, TArray<FVector> &normalBuffer) override;
	virtual void ReadGridIndexBounds(const FString &gridID, FIntVector &indexStart, FIntVector &indexEnd) override;
	virtual SIZE_T ReadGridCount() override;
	virtual void PopulateGridDensity_Perlin(const FString &gridID, double frequency, double lacunarity, double persistence, int octaveCount) override;
};