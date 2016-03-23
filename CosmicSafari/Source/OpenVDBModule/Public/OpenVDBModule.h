#ifndef __OPENVDBMODULE_H__
#define __OPENVDBMODULE_H__

#include "EngineMinimal.h"

namespace Vdb
{
	class VdbHandleBase
	{
	public:
		VdbHandleBase(const FString &path, const FGuid &guid, bool enableGridStats) : Path(path), GUID(guid.A, guid.B, guid.C, guid.D), gridStatsIsEnabled(enableGridStats) {}
		const FString Path;
		const FGuid GUID;
		const bool gridStatsIsEnabled;
	};
	typedef TSharedRef<Vdb::VdbHandleBase> HandleType;

	template<typename DataType, typename TransformType>
	class IVdb
	{
	public:
		typedef typename DataType GridDataType;
		typedef typename TransformType GridTransformType;
		virtual Vdb::HandleType CreateVDB(const FString &path, bool enableGridStats) = 0;
		virtual void InitializeGrid(HandleType handle, const FString &gridName, const GridTransformType &xform) = 0;
		virtual FString AddRegionDefinition(HandleType handle, const FString &gridName, const FString &regionName, const FIntVector &indexStart, const FIntVector &indexEnd) = 0;
		virtual FString AddRegion(HandleType handle, const FString &gridName, const FString &regionName, const FIntVector &indexStart, const FIntVector &indexEnd, TArray<FVector> &vertexBuffer, TArray<int32> &polygonBuffer, TArray<FVector> &normalBuffer) = 0;
		virtual void RemoveRegion(HandleType handle, const FString &regionID) = 0;
		virtual void LoadRegion(HandleType handle, const FString &regionID, TArray<FVector> &vertexBuffer, TArray<int32> &polygonBuffer, TArray<FVector> &normalBuffer) = 0;
		virtual void MeshRegion(HandleType handle, const FString &regionID, float surfaceValue) = 0;
		virtual void ReadRegionIndexBounds(HandleType handle, const FString &regionID, FIntVector &indexStart, FIntVector &indexEnd) = 0;
		virtual SIZE_T ReadMetaRegionCount(HandleType handle) = 0;
		virtual void PopulateRegionDensity_Perlin(HandleType handle, const FString &regionID, double frequency, double lacunarity, double persistence, int octaveCount) = 0;
	};

	typedef struct ScaleTransform
	{
		FVector Scale;
	} ScaleTransformType;

	typedef struct TranslateTransform
	{
		FVector Translation;
	} TranslateTransformType;

	typedef struct ScaleTranslateTransform
	{
		ScaleTransform Scale;
		TranslateTransform Translation;
	} ScaleTranslateTransformType;

	typedef struct UniformScaleTransform
	{
		double Scale;
	} UniformScaleTransformType;

	typedef struct UniformScaleTranslateTransform
	{
		UniformScaleTransform Scale;
		TranslateTransform Translation;
	} UniformScaleTranslateTransformType;

	typedef struct AffineTransform
	{
		FVector MatrixColumn[3];
	} AffineTransformType;

	typedef struct UnitaryTransform
	{
		FVector Axis;
		double Radians;
	} UnitaryTransformType;

	typedef struct NonlinearFrustumTransform
	{
		FVector Position;
		FVector Size;
		double Taper;
		double Depth;
	} NonlinearFrustumTransformType;
}

class OPENVDBMODULE_API FOpenVDBModule : public IModuleInterface, public Vdb::IVdb<float, Vdb::UniformScaleTransform>
{
public:

	typedef Vdb::IVdb<float, Vdb::UniformScaleTransform> VDBInterfaceType;
	typedef VDBInterfaceType::GridDataType GridDataType;
	typedef VDBInterfaceType::GridTransformType GridTransformType;

	static inline FOpenVDBModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FOpenVDBModule>("OpenVDBModule");
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("OpenVDBModule");
	}

	//UFUNCTION(BlueprintCallable)
	Vdb::HandleType CreateVDB(const FString &path, bool enableGridStats);
	void InitializeGrid(Vdb::HandleType handle, const FString &gridName, const GridTransformType &xform);
	FString AddRegionDefinition(Vdb::HandleType handle, const FString &gridName, const FString &regionName, const FIntVector &indexStart, const FIntVector &indexEnd);
	FString AddRegion(Vdb::HandleType handle, const FString &gridName, const FString &regionName, const FIntVector &indexStart, const FIntVector &indexEnd, TArray<FVector> &vertexBuffer, TArray<int32> &polygonBuffer, TArray<FVector> &normalBuffer);
	void RemoveRegion(Vdb::HandleType handle, const FString &regionID);
	void LoadRegion(Vdb::HandleType handle, const FString &regionID, TArray<FVector> &vertexBuffer, TArray<int32> &polygonBuffer, TArray<FVector> &normalBuffer);
	void MeshRegion(Vdb::HandleType handle, const FString &regionID, float surfaceValue);
	void ReadRegionIndexBounds(Vdb::HandleType handle, const FString &regionID, FIntVector &indexStart, FIntVector &indexEnd);
	SIZE_T ReadMetaRegionCount(Vdb::HandleType handle);
	void PopulateRegionDensity_Perlin(Vdb::HandleType handle, const FString &regionID, double frequency, double lacunarity, double persistence, int octaveCount);

private:
	TMap<FGuid, Vdb::HandleType> VdbHandles;

};

DECLARE_LOG_CATEGORY_EXTERN(LogOpenVDBModule, Log, All);

#endif