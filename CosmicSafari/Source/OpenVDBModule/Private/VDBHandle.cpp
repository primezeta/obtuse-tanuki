#include "OpenVDBModule.h"
#include "VDBHandlePrivate.h"

////The standard OpenVDB tree configuration in which leaf nodes contain ValueType
//template<typename ValueType>
//using TreeType = openvdb::tree::Tree4<ValueType, 5, 4, 3>::Type;

DEFINE_LOG_CATEGORY(LogVDBHandle)

extern VDBRegistryType VDBRegistry;

UVDBHandle const * UVDBHandle::RegisterVDB(const FString &path, const FString &worldName, bool enableDelayLoad, bool enableGridStats)
{
	UVDBHandle const * VDBHandle;
	TSharedPtr<VDBHandlePrivateType> VDBPrivatePtr;
	if (!VDBRegistry.Contains(path))
	{
		VDBHandle = NewObject<UVDBHandle>();
		VDBPrivatePtr = TSharedPtr<VDBHandlePrivateType>(new VDBHandlePrivateType(VDBHandle));
		VDBRegistry.Add(path, VDBPrivatePtr);
	}
	else
	{
		VDBPrivatePtr = VDBRegistry.FindChecked(path);
		VDBHandle = VDBPrivatePtr->VDBHandle;
		if (VDBHandle->FilePath != path ||
			VDBHandle->EnableDelayLoad != enableDelayLoad ||
			VDBHandle->EnableGridStats != enableGridStats)
		{
			VDBPrivatePtr->WriteChanges();
			VDBHandle = NewObject<UVDBHandle>();
			VDBRegistry.Add(path, TSharedPtr<VDBHandlePrivateType>(new VDBHandlePrivateType(VDBHandle)));
		}
	}
	VDBPrivatePtr->InsertFileMeta<std::string>("WorldName", TCHAR_TO_UTF8(*worldName));
	return VDBHandle;
}

UVDBHandle::UVDBHandle(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UVDBHandle::PostInitProperties()
{
	Super::PostInitProperties();
	RegisterVDB(FilePath, WorldName, EnableDelayLoad, EnableGridStats);
}

void UVDBHandle::BeginDestroy()
{
	//Queue up the file to be written because From UObject.h BeginDestroy() is:
	//"Called before destroying the object. This is called immediately upon deciding to destroy the object, to allow the object to begin an asynchronous cleanup process."
	TSharedPtr<VDBHandlePrivateType> VDBPrivatePtr = VDBRegistry.FindChecked(FilePath);
	VDBPrivatePtr->WriteChangesAsync();
	Super::BeginDestroy();
}

FString UVDBHandle::AddGrid(const FString &gridName, const FIntVector &indexStart, const FIntVector &indexEnd)
{
	TSharedPtr<VDBHandlePrivateType> VDBPrivatePtr = VDBRegistry.FindChecked(FilePath);
	const Vdb::Metadata::RegionMetadata regionMetaValue(WorldName, gridName, openvdb::CoordBBox(openvdb::Coord(indexStart.X, indexStart.Y, indexStart.Z), openvdb::Coord(indexEnd.X, indexEnd.Y, indexEnd.Z)));
	VDBPrivatePtr->InsertFileMeta<Vdb::Metadata::RegionMetadata>(regionMetaValue.ID(), regionMetaValue);
	return regionMetaValue.ID();
}

void UVDBHandle::RemoveGrid(const FString &gridID)
{
	TSharedPtr<VDBHandlePrivateType> VDBPrivatePtr = VDBRegistry.FindChecked(FilePath);
	VDBPrivatePtr->RemoveFileMeta(gridID);
	VDBPrivatePtr->RemoveGridFromGridVec(gridID);
}

void UVDBHandle::ReadGridTree(const FString &gridID, FIntVector &indexStart, FIntVector &indexEnd)
{
	TSharedPtr<VDBHandlePrivateType> VDBPrivatePtr = VDBRegistry.FindChecked(FilePath);
	VDBPrivatePtr->ReadGridTree<TreeType, Vdb::Metadata::RegionMetadata>(gridID, indexStart, indexEnd);
}

void UVDBHandle::MeshGrid(const FString &gridID, float surfaceValue, TArray<FVector> &vertexBuffer, TArray<int32> &polygonBuffer, TArray<FVector> &normalBuffer)
{
	TSharedPtr<VDBHandlePrivateType> VDBPrivatePtr = VDBRegistry.FindChecked(FilePath);
	VDBPrivatePtr->MeshRegion<TreeType, Vdb::Metadata::RegionMetadata>(gridID, surfaceValue, vertexBuffer, polygonBuffer, normalBuffer);
}

void UVDBHandle::ReadGridIndexBounds(const FString &gridID, FIntVector &indexStart, FIntVector &indexEnd)
{
	TSharedPtr<VDBHandlePrivateType> VDBPrivatePtr = VDBRegistry.FindChecked(FilePath);
	VDBPrivatePtr->ReadGridIndexBounds<TreeType, Vdb::Metadata::RegionMetadata>(gridID, indexStart, indexEnd);
}

int32 UVDBHandle::ReadGridCount()
{
	TSharedPtr<VDBHandlePrivateType> VDBPrivatePtr = VDBRegistry.FindChecked(FilePath);
	return 0;//TODO
}

void UVDBHandle::PopulateGridDensity_Perlin(const FString &gridID, float frequency, float lacunarity, float persistence, int32 octaveCount)
{
	TSharedPtr<VDBHandlePrivateType> VDBPrivatePtr = VDBRegistry.FindChecked(FilePath);
	VDBPrivatePtr->FillGrid_PerlinDensity<TreeType, Vdb::Metadata::RegionMetadata>(gridID, frequency, lacunarity, persistence, octaveCount);
}