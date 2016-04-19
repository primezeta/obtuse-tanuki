// Fill out your copyright notice in the Description page of Project Settings.

#include "OpenVDBModule.h"

////The standard OpenVDB tree configuration in which leaf nodes contain ValueType
//template<typename ValueType>
//using TreeType = openvdb::tree::Tree4<ValueType, 5, 4, 3>::Type;

VdbRegistryType FOpenVDBModule::VdbRegistry;

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
	if (!FilePath.IsEmpty() && FOpenVDBModule::IsAvailable())
	{
		FOpenVDBModule::Get().RegisterVdb(FilePath, EnableGridStats, EnableDelayLoad);
		TSharedPtr<VdbHandlePrivateType> VdbPrivatePtr = FOpenVDBModule::VdbRegistry.FindChecked(FilePath);
		try
		{
			VdbPrivatePtr->InsertFileMeta<openvdb::math::ScaleMap>(VdbHandlePrivateType::MetaName_RegionScale(), openvdb::math::ScaleMap(openvdb::Vec3d((double)RegionVoxelCount.X, (double)RegionVoxelCount.Y, (double)RegionVoxelCount.Z)));
		}
		catch (const openvdb::Exception &e)
		{
			UE_LOG(LogOpenVDBModule, Error, TEXT("UVdbHandle OpenVDB exception: %s"), UTF8_TO_TCHAR(e.what()));
		}
		catch (const std::exception& e)
		{
			UE_LOG(LogOpenVDBModule, Error, TEXT("UVdbHandle exception: %s"), UTF8_TO_TCHAR(e.what()));
		}
		catch (const std::string& e)
		{
			UE_LOG(LogOpenVDBModule, Error, TEXT("UVdbHandle exception: %s"), UTF8_TO_TCHAR(e.c_str()));
		}
		catch (...)
		{
			UE_LOG(LogOpenVDBModule, Fatal, TEXT("UVdbHandle unexpected exception"));
		}
	}
#endif
}

void UVdbHandle::BeginDestroy()
{
	//Queue up the file to be written because From UObject.h BeginDestroy() is:
	//"Called before destroying the object. This is called immediately upon deciding to destroy the object, to allow the object to begin an asynchronous cleanup process."
#if WITH_ENGINE
	if (!FilePath.IsEmpty() && FOpenVDBModule::IsAvailable())
	{
		TSharedPtr<VdbHandlePrivateType> VdbPrivatePtr = FOpenVDBModule::VdbRegistry.FindChecked(FilePath);
		try
		{
			VdbPrivatePtr->WriteChanges();
		}
		catch (const openvdb::Exception &e)
		{
			UE_LOG(LogOpenVDBModule, Error, TEXT("UVdbHandle OpenVDB exception: %s"), UTF8_TO_TCHAR(e.what()));
		}
		catch (const std::exception& e)
		{
			UE_LOG(LogOpenVDBModule, Error, TEXT("UVdbHandle exception: %s"), UTF8_TO_TCHAR(e.what()));
		}
		catch (const std::string& e)
		{
			UE_LOG(LogOpenVDBModule, Error, TEXT("UVdbHandle exception: %s"), UTF8_TO_TCHAR(e.c_str()));
		}
		catch (...)
		{
			UE_LOG(LogOpenVDBModule, Fatal, TEXT("UVdbHandle unexpected exception"));
		}
	}
#endif
	Super::BeginDestroy();
}

FString UVdbHandle::AddGrid(const FString &gridName, const FIntVector &worldIndex, FIntVector &indexStart, FIntVector &indexEnd)
{
	FString gridID;
	if (FOpenVDBModule::IsAvailable())
	{
		TSharedPtr<VdbHandlePrivateType> VdbPrivatePtr = FOpenVDBModule::VdbRegistry.FindChecked(FilePath);
		try
		{
			openvdb::TypedMetadata<openvdb::math::ScaleMap>::Ptr regionSizeMetaValue = VdbPrivatePtr->GetFileMetaValue<openvdb::math::ScaleMap>(VdbHandlePrivateType::MetaName_RegionScale());
			check(regionSizeMetaValue != nullptr);

			openvdb::Vec3d regionStart = regionSizeMetaValue->value().applyInverseMap(openvdb::Vec3d((double)worldIndex.X, (double)worldIndex.Y, (double)worldIndex.Z));
			openvdb::Coord endIndexCoord = openvdb::Coord((int32)regionStart.x() + 1, (int32)regionStart.y() + 1, (int32)regionStart.z() + 1);
			openvdb::Vec3d regionEnd = regionSizeMetaValue->value().applyMap(openvdb::Vec3d((double)endIndexCoord.x(), (double)endIndexCoord.y(), (double)endIndexCoord.z()));
			indexStart = FIntVector(regionStart.x(), regionStart.y(), regionStart.z());
			indexEnd = FIntVector(regionEnd.x(), regionEnd.y(), regionEnd.z());
			gridID = indexStart.ToString() + TEXT(",") + indexEnd.ToString();
			VdbPrivatePtr->AddGrid<TreeType>(gridID);
		}
		catch (const openvdb::Exception &e)
		{
			UE_LOG(LogOpenVDBModule, Error, TEXT("UVdbHandle OpenVDB exception: %s"), UTF8_TO_TCHAR(e.what()));
		}
		catch (const std::exception& e)
		{
			UE_LOG(LogOpenVDBModule, Error, TEXT("UVdbHandle exception: %s"), UTF8_TO_TCHAR(e.what()));
		}
		catch (const std::string& e)
		{
			UE_LOG(LogOpenVDBModule, Error, TEXT("UVdbHandle exception: %s"), UTF8_TO_TCHAR(e.c_str()));
		}
		catch (...)
		{
			UE_LOG(LogOpenVDBModule, Fatal, TEXT("UVdbHandle unexpected exception"));
		}
	}
	return gridID;
}

TArray<FString> UVdbHandle::GetAllGridIDs()
{
	TArray<FString> GridIDs;
	if (FOpenVDBModule::IsAvailable())
	{
		TSharedPtr<VdbHandlePrivateType> VdbPrivatePtr = FOpenVDBModule::VdbRegistry.FindChecked(FilePath);
		try
		{
			VdbPrivatePtr->GetAllGridIDs(GridIDs);
		}
		catch (const openvdb::Exception &e)
		{
			UE_LOG(LogOpenVDBModule, Error, TEXT("UVdbHandle OpenVDB exception: %s"), UTF8_TO_TCHAR(e.what()));
		}
		catch (const std::exception& e)
		{
			UE_LOG(LogOpenVDBModule, Error, TEXT("UVdbHandle exception: %s"), UTF8_TO_TCHAR(e.what()));
		}
		catch (const std::string& e)
		{
			UE_LOG(LogOpenVDBModule, Error, TEXT("UVdbHandle exception: %s"), UTF8_TO_TCHAR(e.c_str()));
		}
		catch (...)
		{
			UE_LOG(LogOpenVDBModule, Fatal, TEXT("UVdbHandle unexpected exception"));
		}
	}
	return GridIDs;
}

void UVdbHandle::RemoveGrid(const FString &gridID)
{
	if (!FOpenVDBModule::IsAvailable())
	{
		return;
	}
	TSharedPtr<VdbHandlePrivateType> VdbPrivatePtr = FOpenVDBModule::VdbRegistry.FindChecked(FilePath);
	try
	{
		VdbPrivatePtr->RemoveFileMeta(gridID);
		VdbPrivatePtr->RemoveGridFromGridVec(gridID);
	}
	catch (const openvdb::Exception &e)
	{
		UE_LOG(LogOpenVDBModule, Error, TEXT("UVdbHandle OpenVDB exception: %s"), UTF8_TO_TCHAR(e.what()));
	}
	catch (const std::exception& e)
	{
		UE_LOG(LogOpenVDBModule, Error, TEXT("UVdbHandle exception: %s"), UTF8_TO_TCHAR(e.what()));
	}
	catch (const std::string& e)
	{
		UE_LOG(LogOpenVDBModule, Error, TEXT("UVdbHandle exception: %s"), UTF8_TO_TCHAR(e.c_str()));
	}
	catch (...)
	{
		UE_LOG(LogOpenVDBModule, Fatal, TEXT("UVdbHandle unexpected exception"));
	}
}

void UVdbHandle::SetRegionScale(const FIntVector &regionScale)
{
	if (!FOpenVDBModule::IsAvailable())
	{
		return;
	}
	TSharedPtr<VdbHandlePrivateType> VdbPrivatePtr = FOpenVDBModule::VdbRegistry.FindChecked(FilePath);
	try
	{
		VdbPrivatePtr->InsertFileMeta(VdbHandlePrivateType::MetaName_RegionScale(), openvdb::math::ScaleMap(openvdb::Vec3d((double)regionScale.X, (double)regionScale.Y, (double)regionScale.Z)));
	}
	catch (const openvdb::Exception &e)
	{
		UE_LOG(LogOpenVDBModule, Error, TEXT("UVdbHandle OpenVDB exception: %s"), UTF8_TO_TCHAR(e.what()));
	}
	catch (const std::exception& e)
	{
		UE_LOG(LogOpenVDBModule, Error, TEXT("UVdbHandle exception: %s"), UTF8_TO_TCHAR(e.what()));
	}
	catch (const std::string& e)
	{
		UE_LOG(LogOpenVDBModule, Error, TEXT("UVdbHandle exception: %s"), UTF8_TO_TCHAR(e.c_str()));
	}
	catch (...)
	{
		UE_LOG(LogOpenVDBModule, Fatal, TEXT("UVdbHandle unexpected exception"));
	}
}

void UVdbHandle::ReadGridTreeIndex(const FString &gridID, const FIntVector &startFill, const FIntVector &endFill, FIntVector &activeStart, FIntVector &activeEnd)
{
	if (!FOpenVDBModule::IsAvailable())
	{
		return;
	}
	TSharedPtr<VdbHandlePrivateType> VdbPrivatePtr = FOpenVDBModule::VdbRegistry.FindChecked(FilePath);
	try
	{
		VdbHandlePrivateType::GridTypePtr GridPtr = VdbPrivatePtr->ReadGridTree<TreeType>(gridID, activeStart, activeEnd);
		VdbPrivatePtr->FillGrid_PerlinDensity<TreeType>(gridID, startFill, endFill, PerlinFrequency, PerlinLacunarity, PerlinPersistence, PerlinOctaveCount, activeStart, activeEnd);
	}
	catch (const openvdb::Exception &e)
	{
		UE_LOG(LogOpenVDBModule, Error, TEXT("UVdbHandle OpenVDB exception: %s"), UTF8_TO_TCHAR(e.what()));
	}
	catch (const std::exception& e)
	{
		UE_LOG(LogOpenVDBModule, Error, TEXT("UVdbHandle exception: %s"), UTF8_TO_TCHAR(e.what()));
	}
	catch (const std::string& e)
	{
		UE_LOG(LogOpenVDBModule, Error, TEXT("UVdbHandle exception: %s"), UTF8_TO_TCHAR(e.c_str()));
	}
	catch (...)
	{
		UE_LOG(LogOpenVDBModule, Fatal, TEXT("UVdbHandle unexpected exception"));
	}
}

//TODO
//void UVdbHandle::ReadGridTreeWorld(const FString &gridID, FVector &activeStart, FVector &activeEnd) //TODO
//{
//	TSharedPtr<VdbHandlePrivateType> VdbPrivatePtr = FOpenVDBModule::VdbRegistry.FindChecked(FilePath);
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
	if (!FOpenVDBModule::IsAvailable())
	{
		return;
	}
	TSharedPtr<VdbHandlePrivateType> VdbPrivatePtr = FOpenVDBModule::VdbRegistry.FindChecked(FilePath);
	try
	{
		VdbPrivatePtr->MeshRegion<TreeType, Vdb::GridOps::IndexTreeType>(gridID, surfaceValue, vertexBuffer, polygonBuffer, normalBuffer);
	}
	catch (const openvdb::Exception &e)
	{
		UE_LOG(LogOpenVDBModule, Error, TEXT("UVdbHandle OpenVDB exception: %s"), UTF8_TO_TCHAR(e.what()));
	}
	catch (const std::exception& e)
	{
		UE_LOG(LogOpenVDBModule, Error, TEXT("UVdbHandle exception: %s"), UTF8_TO_TCHAR(e.what()));
	}
	catch (const std::string& e)
	{
		UE_LOG(LogOpenVDBModule, Error, TEXT("UVdbHandle exception: %s"), UTF8_TO_TCHAR(e.c_str()));
	}
	catch (...)
	{
		UE_LOG(LogOpenVDBModule, Fatal, TEXT("UVdbHandle unexpected exception"));
	}
}

void UVdbHandle::ReadGridIndexBounds(const FString &gridID, FIntVector &indexStart, FIntVector &indexEnd)
{
	if (!FOpenVDBModule::IsAvailable())
	{
		return;
	}
	TSharedPtr<VdbHandlePrivateType> VdbPrivatePtr = FOpenVDBModule::VdbRegistry.FindChecked(FilePath);
	try
	{
		VdbPrivatePtr->ReadGridIndexBounds<TreeType>(gridID, indexStart, indexEnd);
	}
	catch (const openvdb::Exception &e)
	{
		UE_LOG(LogOpenVDBModule, Error, TEXT("UVdbHandle OpenVDB exception: %s"), UTF8_TO_TCHAR(e.what()));
	}
	catch (const std::exception& e)
	{
		UE_LOG(LogOpenVDBModule, Error, TEXT("UVdbHandle exception: %s"), UTF8_TO_TCHAR(e.what()));
	}
	catch (const std::string& e)
	{
		UE_LOG(LogOpenVDBModule, Error, TEXT("UVdbHandle exception: %s"), UTF8_TO_TCHAR(e.c_str()));
	}
	catch (...)
	{
		UE_LOG(LogOpenVDBModule, Fatal, TEXT("UVdbHandle unexpected exception"));
	}
}

FIntVector UVdbHandle::GetRegionIndex(const FVector &worldLocation)
{
	openvdb::Vec3d regionIndex;
	if (FOpenVDBModule::IsAvailable())
	{
		TSharedPtr<VdbHandlePrivateType> VdbPrivatePtr = FOpenVDBModule::VdbRegistry.FindChecked(FilePath);
		try
		{
			openvdb::TypedMetadata<openvdb::math::ScaleMap>::Ptr regionSizeMetaValue = VdbPrivatePtr->GetFileMetaValue<openvdb::math::ScaleMap>(VdbHandlePrivateType::MetaName_RegionScale());
			check(regionSizeMetaValue != nullptr);
			regionIndex = regionSizeMetaValue->value().applyMap(openvdb::Vec3d(worldLocation.X, worldLocation.Y, worldLocation.Z));
		}
		catch (const openvdb::Exception &e)
		{
			UE_LOG(LogOpenVDBModule, Error, TEXT("UVdbHandle OpenVDB exception: %s"), UTF8_TO_TCHAR(e.what()));
		}
		catch (const std::exception& e)
		{
			UE_LOG(LogOpenVDBModule, Error, TEXT("UVdbHandle exception: %s"), UTF8_TO_TCHAR(e.what()));
		}
		catch (const std::string& e)
		{
			UE_LOG(LogOpenVDBModule, Error, TEXT("UVdbHandle exception: %s"), UTF8_TO_TCHAR(e.c_str()));
		}
		catch (...)
		{
			UE_LOG(LogOpenVDBModule, Fatal, TEXT("UVdbHandle unexpected exception"));
		}
	}
	return FIntVector((int)regionIndex.x(), (int)regionIndex.y(), (int)regionIndex.z());
}