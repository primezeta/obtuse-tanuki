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
	PerlinSeed = 0;
	PerlinFrequency = 2.01f;
	PerlinLacunarity = 2.0f;
	PerlinPersistence = 0.5f;
	PerlinOctaveCount = 8;
}

void UVdbHandle::InitVdb(TArray<TArray<FVector>> &VertexBuffers, TArray<TArray<int32>> &PolygonBuffers, TArray<TArray<FVector>> &NormalBuffers)
{
#if WITH_ENGINE
	if (!FilePath.IsEmpty() && FOpenVDBModule::IsAvailable())
	{
		FOpenVDBModule::Get().RegisterVdb(FilePath, EnableGridStats, EnableDelayLoad, VertexBuffers, PolygonBuffers, NormalBuffers);
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

FString UVdbHandle::AddGrid(const FString &gridName, const FVector &worldLocation, const FVector &voxelSize)
{
	FString gridID;
	if (FOpenVDBModule::IsAvailable())
	{
		TSharedPtr<VdbHandlePrivateType> VdbPrivatePtr = FOpenVDBModule::VdbRegistry.FindChecked(FilePath);
		try
		{
			TSharedPtr<openvdb::TypedMetadata<openvdb::math::ScaleMap>> regionSizeMetaValue = VdbPrivatePtr->GetFileMetaValue<openvdb::math::ScaleMap>(VdbHandlePrivateType::MetaName_RegionScale());
			check(regionSizeMetaValue.IsValid());

			const openvdb::Vec3d regionStart = regionSizeMetaValue->value().applyInverseMap(openvdb::Vec3d((double)worldLocation.X, (double)worldLocation.Y, (double)worldLocation.Z));
			const openvdb::Coord endIndexCoord = openvdb::Coord((int32)regionStart.x() + 1, (int32)regionStart.y() + 1, (int32)regionStart.z() + 1);
			const openvdb::Vec3d regionEnd = regionSizeMetaValue->value().applyMap(openvdb::Vec3d((double)endIndexCoord.x(), (double)endIndexCoord.y(), (double)endIndexCoord.z()));
			const FIntVector indexStart = FIntVector(regionStart.x(), regionStart.y(), regionStart.z());
			const FIntVector indexEnd = FIntVector(regionEnd.x(), regionEnd.y(), regionEnd.z());
			
			gridID = indexStart.ToString() + TEXT(",") + indexEnd.ToString();
			VdbPrivatePtr->AddGrid<TreeType>(gridID, indexStart, indexEnd, voxelSize);
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
		if (regionScale.X > 0 && regionScale.Y > 0 && regionScale.Z > 0)
		{
			VdbPrivatePtr->InsertFileMeta(VdbHandlePrivateType::MetaName_RegionScale(), openvdb::math::ScaleMap(openvdb::Vec3d((double)regionScale.X, (double)regionScale.Y, (double)regionScale.Z)));
		}
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

void UVdbHandle::ReadGridTreeIndex(const FString &gridID, FIntVector &startFill, FIntVector &endFill, FIntVector &activeStart, FIntVector &activeEnd)
{
	if (!FOpenVDBModule::IsAvailable())
	{
		return;
	}
	TSharedPtr<VdbHandlePrivateType> VdbPrivatePtr = FOpenVDBModule::VdbRegistry.FindChecked(FilePath);
	try
	{
		VdbHandlePrivateType::GridTypePtr GridPtr = VdbPrivatePtr->ReadGridTree<TreeType>(gridID, startFill, endFill);
		UE_LOG(LogOpenVDBModule, Display, TEXT("Pre Perlin op: %s has %d active voxels"), *gridID, GridPtr->activeVoxelCount());
		VdbPrivatePtr->FillGrid_PerlinDensity<TreeType>(gridID, startFill, endFill, PerlinSeed, PerlinFrequency, PerlinLacunarity, PerlinPersistence, PerlinOctaveCount, activeStart, activeEnd);
		UE_LOG(LogOpenVDBModule, Display, TEXT("Post Perlin op: %s has %d active voxels"), *gridID, GridPtr->activeVoxelCount());
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

void UVdbHandle::MeshGrid(const FString &gridID, float surfaceValue)
{
	if (!FOpenVDBModule::IsAvailable())
	{
		return;
	}
	TSharedPtr<VdbHandlePrivateType> VdbPrivatePtr = FOpenVDBModule::VdbRegistry.FindChecked(FilePath);
	try
	{
		VdbHandlePrivateType::GridTypePtr GridPtr = VdbPrivatePtr->GetGridPtr<TreeType>(gridID);
		openvdb::CoordBBox bbox = GridPtr->evalActiveVoxelBoundingBox();
		UE_LOG(LogOpenVDBModule, Display, TEXT("Pre mesh op: %s has %d active voxels with bbox %d,%d,%d %d,%d,%d"), *gridID, GridPtr->activeVoxelCount(), bbox.min().x(), bbox.min().y(), bbox.min().z(), bbox.max().x(), bbox.max().y(), bbox.max().z());
		VdbPrivatePtr->MeshRegion<TreeType, Vdb::GridOps::IndexTreeType>(gridID, surfaceValue);
		bbox = GridPtr->evalActiveVoxelBoundingBox();
		UE_LOG(LogOpenVDBModule, Display, TEXT("Post mesh op: %s has %d active voxels with bbox %d,%d,%d %d,%d,%d"), *gridID, GridPtr->activeVoxelCount(), bbox.min().x(), bbox.min().y(), bbox.min().z(), bbox.max().x(), bbox.max().y(), bbox.max().z());
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
			TSharedPtr<openvdb::TypedMetadata<openvdb::math::ScaleMap>> regionSizeMetaValue = VdbPrivatePtr->GetFileMetaValue<openvdb::math::ScaleMap>(VdbHandlePrivateType::MetaName_RegionScale());
			check(regionSizeMetaValue.IsValid());
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

void UVdbHandle::WriteAllGrids()
{
	if (FOpenVDBModule::IsAvailable())
	{
		for (auto i = FOpenVDBModule::VdbRegistry.CreateIterator(); i; ++i)
		{
			i.Value()->WriteChanges();
		}
	}
}