#pragma once
#include "EngineMinimal.h"
#include "VDBInterface.h"

DECLARE_LOG_CATEGORY_EXTERN(LogOpenVDBModule, Log, All);

USTRUCT()
struct FScaleTransformStruct
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = "Scale Transform Struct")
	FVector Scale;
};

USTRUCT()
struct FTranslateTransformStruct
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = "Translate Transform Struct")
	FVector Translation;
};

USTRUCT()
struct FScaleTranslateTransformStruct
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = "Scale Translate Transform Struct")
	FVector Scale;
	UPROPERTY(EditAnywhere, Category = "Scale Translate Transform Struct")
	FVector Translation;
};

USTRUCT()
struct FUniformScaleTransformStruct
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = "Uniform Scale Transform Struct")
	double UniformScale;
};

USTRUCT()
struct FUniformScaleTranslateTransformStruct
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = "Uniform Scale Translate Transform Struct")
	double UniformScale;
	UPROPERTY(EditAnywhere, Category = "Uniform Scale Translate Transform Struct")
	FVector Translation;
};

USTRUCT()
struct FAffineTransformStruct
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = "Affine Transform Struct")
	FVector MatrixColumnA;
	UPROPERTY(EditAnywhere, Category = "Affine Transform Struct")
	FVector MatrixColumnB;
	UPROPERTY(EditAnywhere, Category = "Affine Transform Struct")
	FVector MatrixColumnC;
};

USTRUCT()
struct FUnitaryTransformStruct
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = "Unitary Transform Struct")
	FVector Axis;
	UPROPERTY(EditAnywhere, Category = "Unitary Transform Struct")
	double Radians;
};

USTRUCT()
struct FNonlinearFrustumTransformStruct
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = "Nonlinear Frustum Transform Struct")
	FVector Position;
	UPROPERTY(EditAnywhere, Category = "Nonlinear Frustum Transform Struct")
	FVector Size;
	UPROPERTY(EditAnywhere, Category = "Nonlinear Frustum Transform Struct")
	double Taper;
	UPROPERTY(EditAnywhere, Category = "Nonlinear Frustum Transform Struct")
	double Depth;
};

UCLASS()
class UVdbHandle : public UObject, public IVDBInterface
{
	GENERATED_UCLASS_BODY()

public:
	UVdbHandle(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere)
	FString FilePath;

	UPROPERTY(EditAnywhere)
	bool EnableDelayLoad;

	UPROPERTY(EditAnywhere)
	bool EnableGridStats;

	UPROPERTY(EditAnywhere)
	FString WorldName;

	void Initialize_Implementation(const FString &path, bool enableDelayLoad, bool enableGridStats) override;
	FString AddGrid_Implementation(const FString &gridName, const FIntVector &indexStart, const FIntVector &indexEnd) override;
	void RemoveGrid_Implementation(const FString &gridID) override;
	void ReadGridTree_Implementation(const FString &gridID, const FIntVector &indexStart, const FIntVector &indexEnd, TArray<FVector> &vertexBuffer, TArray<int32> &polygonBuffer, TArray<FVector> &normalBuffer) override;
	void MeshGrid_Implementation(const FString &gridID, float surfaceValue) override;
	void ReadGridIndexBounds_Implementation(const FString &gridID, FIntVector &indexStart, FIntVector &indexEnd) override;
	SIZE_T ReadGridCount_Implementation() override;
	void PopulateGridDensity_Perlin_Implementation(const FString &gridID, double frequency, double lacunarity, double persistence, int octaveCount) override;
};