// Fill out your copyright notice in the Description page of Project Settings.

#include "OpenVDBModule.h"

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

void UVdbHandle::InitVdb()
{
#if WITH_ENGINE
	if (!FilePath.IsEmpty() && FOpenVDBModule::IsAvailable())
	{
		FOpenVDBModule::Get().RegisterVdb(FilePath, EnableGridStats, EnableDelayLoad);
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
			VdbPrivatePtr->AddGrid(gridID, indexStart, indexEnd, voxelSize);
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

void UVdbHandle::ReadGridTree(const FString &gridID, const float &surfaceValue, FIntVector &startFill, FIntVector &endFill, FIntVector &indexStart, FIntVector &indexEnd, FVector &worldStart, FVector &worldEnd, FVector &startLocation)
{
	if (!FOpenVDBModule::IsAvailable())
	{
		return;
	}
	TSharedPtr<VdbHandlePrivateType> VdbPrivatePtr = FOpenVDBModule::VdbRegistry.FindChecked(FilePath);
	try
	{
		VdbHandlePrivateType::GridTypePtr GridPtr = VdbPrivatePtr->ReadGridTree(gridID, startFill, endFill, startLocation);
		VdbPrivatePtr->FillGrid_PerlinDensity(gridID, startFill, endFill, PerlinSeed, PerlinFrequency, PerlinLacunarity, PerlinPersistence, PerlinOctaveCount, surfaceValue, indexStart, indexEnd, worldStart, worldEnd);
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

void UVdbHandle::GetVoxelCoord(const FString &gridID, const FVector &worldLocation, FIntVector &outVoxelCoord)
{
	if (!FOpenVDBModule::IsAvailable())
	{
		return;
	}
	TSharedPtr<VdbHandlePrivateType> VdbPrivatePtr = FOpenVDBModule::VdbRegistry.FindChecked(FilePath);
	try
	{
		VdbPrivatePtr->GetIndexCoord(gridID, worldLocation, outVoxelCoord);
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

void UVdbHandle::MeshGrid(const FString &gridID,
						  TSharedPtr<TArray<FVector>> &OutVertexBufferPtr,
						  TSharedPtr<TArray<int32>> &OutPolygonBufferPtr,
						  TSharedPtr<TArray<FVector>> &OutNormalBufferPtr,
						  TSharedPtr<TArray<FVector2D>> &OutUVMapBufferPtr,
						  TSharedPtr<TArray<FColor>> &OutVertexColorsBufferPtr,
						  TSharedPtr<TArray<FProcMeshTangent>> &OutTangentsBufferPtr)
{
	if (!FOpenVDBModule::IsAvailable())
	{
		return;
	}
	TSharedPtr<VdbHandlePrivateType> VdbPrivatePtr = FOpenVDBModule::VdbRegistry.FindChecked(FilePath);
	try
	{
		VdbHandlePrivateType::GridTypePtr GridPtr = VdbPrivatePtr->GetGridPtrChecked(gridID);
		VdbPrivatePtr->MeshRegion(gridID);
		VdbPrivatePtr->GetGridSectionBuffers(gridID, OutVertexBufferPtr, OutPolygonBufferPtr, OutNormalBufferPtr, OutUVMapBufferPtr, OutVertexColorsBufferPtr, OutTangentsBufferPtr);
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