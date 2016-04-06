// Fill out your copyright notice in the Description page of Project Settings.

#include "OpenVDBModule.h"
#include "VdbHandle.h"
#include "VdbHandlePrivate.h"

////The standard OpenVDB tree configuration in which leaf nodes contain ValueType
//template<typename ValueType>
//using TreeType = openvdb::tree::Tree4<ValueType, 5, 4, 3>::Type;

extern VdbRegistryType VdbRegistry;

UVdbHandle const * UVdbHandle::RegisterNewVdb(const FObjectInitializer& ObjectInitializer, UObject * parent, const FString &path, const FString &worldName, bool enableDelayLoad, bool enableGridStats)
{
	UVdbHandle * VdbHandle = ObjectInitializer.CreateDefaultSubobject<UVdbHandle>(parent, TEXT("VDB Handle"));
	VdbHandle->FilePath = path;
	VdbHandle->WorldName = worldName;
	VdbHandle->EnableDelayLoad = enableDelayLoad;
	VdbHandle->EnableGridStats = enableGridStats;
	UVdbHandle::RegisterVdb(VdbHandle);
	return VdbHandle;
}

void UVdbHandle::RegisterVdb(UVdbHandle const * VdbHandle)
{
	check(!VdbHandle->FilePath.IsEmpty());
	TSharedPtr<VdbHandlePrivateType> VdbPrivatePtr;
	if (!VdbRegistry.Contains(VdbHandle->FilePath))
	{
		VdbPrivatePtr = TSharedPtr<VdbHandlePrivateType>(new VdbHandlePrivateType(VdbHandle));
		VdbRegistry.Add(VdbHandle->FilePath, VdbPrivatePtr);
	}
	else
	{
		VdbPrivatePtr = VdbRegistry.FindChecked(VdbHandle->FilePath);
		if (VdbPrivatePtr->VdbHandle->EnableDelayLoad != VdbHandle->EnableDelayLoad ||
			VdbPrivatePtr->VdbHandle->EnableGridStats != VdbHandle->EnableGridStats)
		{
			VdbPrivatePtr->WriteChanges();
			VdbPrivatePtr = TSharedPtr<VdbHandlePrivateType>(new VdbHandlePrivateType(VdbHandle));
			VdbRegistry[VdbHandle->FilePath] = VdbPrivatePtr;
		}
	}
}

UVdbHandle::UVdbHandle(const FObjectInitializer& ObjectInitializer)
{
	FilePath = "";
	EnableDelayLoad = true;
	EnableGridStats = true;
	WorldName = "";
	PerlinFrequency = 2.01f;
	PerlinLacunarity = 2.0f;
	PerlinPersistence = 0.5f;
	PerlinOctaveCount = 8;
}

void UVdbHandle::InitVdb()
{
#if WITH_ENGINE
	if (!FilePath.IsEmpty())
	{
		RegisterVdb(this);
		TSharedPtr<VdbHandlePrivateType> VdbPrivatePtr = VdbRegistry.FindChecked(FilePath);
		openvdb::TypedMetadata<openvdb::math::ScaleMap>::Ptr regionSizeMetaValue = VdbPrivatePtr->GetFileMetaValue<openvdb::math::ScaleMap>(VdbHandlePrivateType::MetaName_RegionSize());
		if (regionSizeMetaValue == nullptr)
		{
			VdbPrivatePtr->InsertFileMeta<openvdb::math::ScaleMap>(VdbHandlePrivateType::MetaName_RegionSize(), openvdb::math::ScaleMap(openvdb::Vec3d(1.0)));
			VdbPrivatePtr->WriteChanges();
		}
	}
#endif
}

void UVdbHandle::FinishVdb()
{
#if WITH_ENGINE
	if (!FilePath.IsEmpty())
	{
		TSharedPtr<VdbHandlePrivateType> VdbPrivatePtr = VdbRegistry.FindChecked(FilePath);
		VdbPrivatePtr->WriteChangesAsync();
	}
#endif
}

void UVdbHandle::PostInitProperties()
{
	Super::PostInitProperties();
	InitVdb();
}

void UVdbHandle::BeginDestroy()
{
	//Queue up the file to be written because From UObject.h BeginDestroy() is:
	//"Called before destroying the object. This is called immediately upon deciding to destroy the object, to allow the object to begin an asynchronous cleanup process."
	FinishVdb();
	Super::BeginDestroy();
}

FString UVdbHandle::AddGrid(const FString &gridName, const FIntVector &regionIndex, FIntVector &indexStart, FIntVector &indexEnd)
{
	TSharedPtr<VdbHandlePrivateType> VdbPrivatePtr = VdbRegistry.FindChecked(FilePath);
	openvdb::TypedMetadata<openvdb::math::ScaleMap>::Ptr regionSizeMetaValue = VdbPrivatePtr->GetFileMetaValue<openvdb::math::ScaleMap>(VdbHandlePrivateType::MetaName_RegionSize());
	check(regionSizeMetaValue != nullptr);
	openvdb::Vec3d region = regionSizeMetaValue->value().applyMap(openvdb::Vec3d(regionIndex.X, regionIndex.Y, regionIndex.Z));
	openvdb::Coord regionStart = openvdb::Coord(region.x(), region.y(), region.z());
	openvdb::Coord regionEnd = openvdb::Coord(regionStart.x()*2 - 1, regionStart.y()*2 - 1, regionStart.z()*2 - 1);
	const Vdb::Metadata::RegionMetadata regionMetaValue(WorldName, gridName, openvdb::CoordBBox(regionStart, regionEnd));
	FString gridID = regionIndex.ToString();
	VdbPrivatePtr->InsertFileMeta<Vdb::Metadata::RegionMetadata>(gridID, regionMetaValue);

	indexStart = FIntVector(regionStart.x(), regionStart.y(), regionStart.z());
	indexEnd = FIntVector(regionEnd.x(), regionEnd.y(), regionEnd.z());
	return gridID;
}

TArray<FString> UVdbHandle::GetAllGridIDs()
{
	TSharedPtr<VdbHandlePrivateType> VdbPrivatePtr = VdbRegistry.FindChecked(FilePath);
	TArray<FString> GridIDs;
	for (auto i = VdbPrivatePtr->FileMetaPtr->beginMeta(); i != VdbPrivatePtr->FileMetaPtr->endMeta(); ++i)
	{
		if (i->second->typeName() == openvdb::TypedMetadata<Vdb::Metadata::RegionMetadata>::staticTypeName())
		{
			GridIDs.Add(UTF8_TO_TCHAR(i->first.c_str()));
		}
	}
	return GridIDs;
}

void UVdbHandle::RemoveGrid(const FString &gridID)
{
	TSharedPtr<VdbHandlePrivateType> VdbPrivatePtr = VdbRegistry.FindChecked(FilePath);
	VdbPrivatePtr->RemoveFileMeta(gridID);
	VdbPrivatePtr->RemoveGridFromGridVec(gridID);
}

void UVdbHandle::SetRegionSize(const FIntVector &regionSize)
{
	TSharedPtr<VdbHandlePrivateType> VdbPrivatePtr = VdbRegistry.FindChecked(FilePath);
	VdbPrivatePtr->InsertFileMeta(VdbHandlePrivateType::MetaName_RegionSize(), openvdb::math::ScaleMap(openvdb::Vec3d((double)regionSize.X, (double)regionSize.Y, (double)regionSize.Z)));
}

void UVdbHandle::ReadGridTreeIndex(const FString &gridID, FIntVector &activeStart, FIntVector &activeEnd)
{
	TSharedPtr<VdbHandlePrivateType> VdbPrivatePtr = VdbRegistry.FindChecked(FilePath);
	VdbHandlePrivateType::GridTypePtr GridPtr = VdbPrivatePtr->ReadGridTree<TreeType, Vdb::Metadata::RegionMetadata>(gridID, activeStart, activeEnd);
	if (GridPtr != nullptr)
	{
		openvdb::TypedMetadata<openvdb::CoordBBox>::Ptr bboxMinPtr = VdbPrivatePtr->GetGridMetaValue<openvdb::CoordBBox>(gridID, openvdb::GridBase::META_FILE_BBOX_MIN);
		openvdb::TypedMetadata<openvdb::CoordBBox>::Ptr bboxMaxPtr = VdbPrivatePtr->GetGridMetaValue<openvdb::CoordBBox>(gridID, openvdb::GridBase::META_FILE_BBOX_MAX);
		check(bboxMinPtr != nullptr);
		check(bboxMaxPtr != nullptr);
		FIntVector startFill(bboxMinPtr->value().min().x(), bboxMinPtr->value().min().y(), bboxMinPtr->value().min().z());
		FIntVector endFill(bboxMaxPtr->value().min().x(), bboxMaxPtr->value().min().y(), bboxMaxPtr->value().min().z());
		VdbPrivatePtr->FillGrid_PerlinDensity<TreeType, Vdb::Metadata::RegionMetadata>(gridID, startFill, endFill, PerlinFrequency, PerlinLacunarity, PerlinPersistence, PerlinOctaveCount, activeStart, activeEnd);
		VdbPrivatePtr->WriteChangesAsync();
	}
}

//TODO
//void UVdbHandle::ReadGridTreeWorld(const FString &gridID, FVector &activeStart, FVector &activeEnd) //TODO
//{
//	TSharedPtr<VdbHandlePrivateType> VdbPrivatePtr = VdbRegistry.FindChecked(FilePath);
//	VdbHandlePrivateType::GridTypePtr GridPtr = VdbPrivatePtr->ReadGridTree<TreeType, Vdb::Metadata::RegionMetadata>(gridID, activeStart, activeEnd);
//	if (!GridPtr.IsValid())
//	{
//		TSharedPtr<openvdb::CoordBBox> bboxMinPtr = VdbPrivatePtr->GetGridMetaValue<openvdb::CoordBBox>(gridID, openvdb::GridBase::META_FILE_BBOX_MIN);
//		check(bboxMinPtr.IsValid());
//		TSharedPtr<openvdb::CoordBBox> bboxMaxPtr = VdbPrivatePtr->GetGridMetaValue<openvdb::CoordBBox>(gridID, openvdb::GridBase::META_FILE_BBOX_MAX);
//		check(bboxMaxPtr.IsValid());
//		FIntVector startFill(bboxMinPtr->min().x(), bboxMinPtr->min().y(), bboxMinPtr->min().z());
//		FIntVector endFill(bboxMaxPtr->min().x(), bboxMaxPtr->min().y(), bboxMaxPtr->min().z());
//		FIntVector activeIndexStart;
//		FIntVector activeIndexEnd;
//		VdbPrivatePtr->FillGrid_PerlinDensity<TreeType, Vdb::Metadata::RegionMetadata>(gridID, startFill, endFill, PerlinFrequency, PerlinLacunarity, PerlinPersistence, PerlinOctaveCount, activeIndexStart, activeIndexEnd);
//		VdbPrivatePtr->WriteChangesAsync();
//	}
//}

void UVdbHandle::MeshGrid(const FString &gridID, float surfaceValue, TArray<FVector> &vertexBuffer, TArray<int32> &polygonBuffer, TArray<FVector> &normalBuffer)
{
	TSharedPtr<VdbHandlePrivateType> VdbPrivatePtr = VdbRegistry.FindChecked(FilePath);
	VdbPrivatePtr->MeshRegion<TreeType, Vdb::Metadata::RegionMetadata>(gridID, surfaceValue, vertexBuffer, polygonBuffer, normalBuffer);
}

void UVdbHandle::ReadGridIndexBounds(const FString &gridID, FIntVector &indexStart, FIntVector &indexEnd)
{
	TSharedPtr<VdbHandlePrivateType> VdbPrivatePtr = VdbRegistry.FindChecked(FilePath);
	VdbPrivatePtr->ReadGridIndexBounds<TreeType, Vdb::Metadata::RegionMetadata>(gridID, indexStart, indexEnd);
}

int32 UVdbHandle::ReadGridCount()
{
	int32 count = 0;
	TSharedPtr<VdbHandlePrivateType> VdbPrivatePtr = VdbRegistry.FindChecked(FilePath);
	for (auto i = VdbPrivatePtr->FileMetaPtr->beginMeta(); i != VdbPrivatePtr->FileMetaPtr->endMeta(); ++i)
	{
		if (i->second->typeName() == openvdb::TypedMetadata<Vdb::Metadata::RegionMetadata>::staticTypeName())
		{
			count++;
		}
	}
	return count;
}

FIntVector UVdbHandle::GetRegionIndex(const FVector &worldLocation)
{
	TSharedPtr<VdbHandlePrivateType> VdbPrivatePtr = VdbRegistry.FindChecked(FilePath);
	openvdb::TypedMetadata<openvdb::math::ScaleMap>::Ptr regionSizeMetaValue = VdbPrivatePtr->GetFileMetaValue<openvdb::math::ScaleMap>(VdbHandlePrivateType::MetaName_RegionSize());
	check(regionSizeMetaValue != nullptr);
	openvdb::Vec3d regionIndex = regionSizeMetaValue->value().applyInverseMap(openvdb::Vec3d(worldLocation.X, worldLocation.Y, worldLocation.Z));
	return FIntVector((int)regionIndex.x(), (int)regionIndex.y(), (int)regionIndex.z());
}