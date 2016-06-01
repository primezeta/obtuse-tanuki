#ifndef __OPENVDBMODULE_H__
#define __OPENVDBMODULE_H__

#include "EngineMinimal.h"
#include "VdbHandle.h"

class OPENVDBMODULE_API FOpenVDBModule : public IModuleInterface
{
public:
	static inline FOpenVDBModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FOpenVDBModule>("OpenVDBModule");
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("OpenVDBModule");
	}

	static void RegisterVdb(UVdbHandle const * VdbHandle);
	static void UnregisterVdb(UVdbHandle const * VdbHandle);
	static FString AddGrid(UVdbHandle const * VdbHandle, const FString &gridName, const FIntVector &regionIndex, const FVector &voxelSize);
	static TArray<FString> GetAllGridIDs(UVdbHandle const * VdbHandle);
	static void RemoveGrid(UVdbHandle const * VdbHandle, const FString &gridID);
	static void SetRegionScale(UVdbHandle const * VdbHandle, const FIntVector &regionScale);
	static void ReadGridTree(UVdbHandle const * VdbHandle, const FString &gridID, EMeshType MeshMethod, FIntVector &startFill, FIntVector &endFill);
	static void GetVoxelCoord(UVdbHandle const * VdbHandle, const FString &gridID, const FVector &worldLocation, FIntVector &outVoxelCoord);
	static void MeshGrid(UVdbHandle const * VdbHandle,
		const FString &gridID,
		const FVector &Location,
		TSharedPtr<VertexBufferType>& VertexBuffer,
		TSharedPtr<PolygonBufferType>& PolygonBuffer,
		TSharedPtr<NormalBufferType>& NormalBuffer,
		TSharedPtr<UVMapBufferType>& UVMapBuffer,
		TSharedPtr<VertexColorBufferType>& VertexColorBuffer,
		TSharedPtr<TangentBufferType>& TangentBuffer,
		EMeshType MeshMethod,
		FVector &worldStart,
		FVector &worldEnd,
		FVector &startLocation);
	static FIntVector GetRegionIndex(UVdbHandle const * VdbHandle, const FVector &worldLocation);
	static void WriteAllGrids(UVdbHandle const * VdbHandle);
};

#endif