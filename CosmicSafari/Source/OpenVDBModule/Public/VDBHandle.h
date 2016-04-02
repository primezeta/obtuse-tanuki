#pragma once
#include "EngineMinimal.h"
#include "VDBInterface.h"
#include "VDBHandle.generated.h"

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

UCLASS(MinimalAPI)
class UVDBHandle : public UObject, public IVDBInterface
{
	GENERATED_BODY()

public:
	static UVDBHandle const * RegisterVDB(const FString &path, const FString &worldName, bool enableDelayLoad, bool enableGridStats);

	UPROPERTY(EditAnywhere)
	FString FilePath;

	UPROPERTY(EditAnywhere)
	bool EnableDelayLoad;

	UPROPERTY(EditAnywhere)
	bool EnableGridStats;

	UPROPERTY(EditAnywhere)
	FString WorldName;

	UVDBHandle(const FObjectInitializer& ObjectInitializer);
	virtual void PostInitProperties() override;
	virtual void BeginDestroy() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UFUNCTION(BlueprintCallable, Category = "VDB Handle")
	virtual FString AddGrid(const FString &gridName, const FIntVector &indexStart, const FIntVector &indexEnd) override;
	UFUNCTION(BlueprintCallable, Category = "VDB Handle")
	virtual void RemoveGrid(const FString &gridID) override;
	UFUNCTION(BlueprintCallable, Category = "VDB Handle")
	virtual void ReadGridTree(const FString &gridID, FIntVector &indexStart, FIntVector &indexEnd) override;
	UFUNCTION(BlueprintCallable, Category = "VDB Handle")
	virtual void MeshGrid(const FString &gridID, float surfaceValue, TArray<FVector> &vertexBuffer, TArray<int32> &polygonBuffer, TArray<FVector> &normalBuffer) override;
	UFUNCTION(BlueprintCallable, Category = "VDB Handle")
	virtual void ReadGridIndexBounds(const FString &gridID, FIntVector &indexStart, FIntVector &indexEnd) override;
	UFUNCTION(BlueprintCallable, Category = "VDB Handle")
	virtual int32 ReadGridCount() override;
	UFUNCTION(BlueprintCallable, Category = "VDB Handle")
	virtual void PopulateGridDensity_Perlin(const FString &gridID, float frequency, float lacunarity, float persistence, int32 octaveCount) override;
};