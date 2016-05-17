#include "OpenVDBModule.h"

DEFINE_LOG_CATEGORY(LogOpenVDBModule)
typedef TMap<FString, TSharedPtr<VdbHandlePrivateType>> VdbRegistryType;
static VdbRegistryType VdbRegistry;

void FOpenVDBModule::RegisterVdb(UVdbHandle const * VdbHandle)
{
	const FString VdbObjectName = VdbHandle->GetReadableName();
	try
	{
		if (!VdbRegistry.Contains(VdbObjectName))
		{
			TSharedPtr<VdbHandlePrivateType> VdbHandlePrivatePtr = TSharedPtr<VdbHandlePrivateType>(new VdbHandlePrivateType(VdbHandle->FilePath, VdbHandle->EnableGridStats, VdbHandle->EnableDelayLoad));
			VdbHandlePrivatePtr->InitGrids();
			VdbRegistry.Add(VdbObjectName, VdbHandlePrivatePtr);
		}
	}
	catch (const openvdb::Exception &e)
	{
		UE_LOG(LogOpenVDBModule, Error, TEXT("OpenVDBModule OpenVDB exception: %s"), UTF8_TO_TCHAR(e.what()));
	}
	catch (const std::exception& e)
	{
		UE_LOG(LogOpenVDBModule, Error, TEXT("OpenVDBModule exception: %s"), UTF8_TO_TCHAR(e.what()));
	}
	catch (const std::string& e)
	{
		UE_LOG(LogOpenVDBModule, Error, TEXT("OpenVDBModule exception: %s"), UTF8_TO_TCHAR(e.c_str()));
	}
	catch (...)
	{
		UE_LOG(LogOpenVDBModule, Fatal, TEXT("OpenVDBModule unexpected exception"));
	}
}

void FOpenVDBModule::UnregisterVdb(UVdbHandle const * VdbHandle)
{
	const FString VdbObjectName = VdbHandle->GetReadableName();
	try
	{
		if (VdbRegistry.Contains(VdbObjectName))
		{
			VdbRegistry[VdbObjectName]->WriteChanges();
		}
	}
	catch (const openvdb::Exception &e)
	{
		UE_LOG(LogOpenVDBModule, Error, TEXT("OpenVDBModule OpenVDB exception: %s"), UTF8_TO_TCHAR(e.what()));
	}
	catch (const std::exception& e)
	{
		UE_LOG(LogOpenVDBModule, Error, TEXT("OpenVDBModule exception: %s"), UTF8_TO_TCHAR(e.what()));
	}
	catch (const std::string& e)
	{
		UE_LOG(LogOpenVDBModule, Error, TEXT("OpenVDBModule exception: %s"), UTF8_TO_TCHAR(e.c_str()));
	}
	catch (...)
	{
		UE_LOG(LogOpenVDBModule, Fatal, TEXT("OpenVDBModule unexpected exception"));
	}
}

FString FOpenVDBModule::AddGrid(UVdbHandle const * VdbHandle, const FString &gridName, const FIntVector &regionIndex, const FVector &voxelSize)
{
	FString gridID;
	TSharedPtr<VdbHandlePrivateType> VdbHandlePrivatePtr = VdbRegistry.FindChecked(VdbHandle->GetReadableName());
	try
	{
		TSharedPtr<openvdb::TypedMetadata<openvdb::math::ScaleMap>> regionSizeMetaValue = VdbHandlePrivatePtr->GetFileMetaValue<openvdb::math::ScaleMap>(VdbHandlePrivateType::MetaName_RegionScale());
		check(regionSizeMetaValue.IsValid());

		const openvdb::Coord startIndexCoord = openvdb::Coord((int32)regionIndex.X, (int32)regionIndex.Y, (int32)regionIndex.Z);
		const openvdb::Coord endIndexCoord = openvdb::Coord((int32)regionIndex.X + 1, (int32)regionIndex.Y + 1, (int32)regionIndex.Z + 1);
		const openvdb::Vec3d regionStart = regionSizeMetaValue->value().applyMap(openvdb::Vec3d((double)startIndexCoord.x(), (double)startIndexCoord.y(), (double)startIndexCoord.z()));
		const openvdb::Vec3d regionEnd = regionSizeMetaValue->value().applyMap(openvdb::Vec3d((double)endIndexCoord.x(), (double)endIndexCoord.y(), (double)endIndexCoord.z()));
		const FIntVector indexStart = FIntVector(regionStart.x(), regionStart.y(), regionStart.z());
		const FIntVector indexEnd = FIntVector(((int32)regionEnd.x())-1, ((int32)regionEnd.y())-1, ((int32)regionEnd.z())-1);

		gridID = indexStart.ToString() + TEXT(",") + indexEnd.ToString();
		VdbHandlePrivatePtr->AddGrid(gridID, indexStart, indexEnd, voxelSize);
	}
	catch (const openvdb::Exception &e)
	{
		UE_LOG(LogOpenVDBModule, Error, TEXT("OpenVDBModule OpenVDB exception: %s"), UTF8_TO_TCHAR(e.what()));
	}
	catch (const std::exception& e)
	{
		UE_LOG(LogOpenVDBModule, Error, TEXT("OpenVDBModule exception: %s"), UTF8_TO_TCHAR(e.what()));
	}
	catch (const std::string& e)
	{
		UE_LOG(LogOpenVDBModule, Error, TEXT("OpenVDBModule exception: %s"), UTF8_TO_TCHAR(e.c_str()));
	}
	catch (...)
	{
		UE_LOG(LogOpenVDBModule, Fatal, TEXT("OpenVDBModule unexpected exception"));
	}
	return gridID;
}

TArray<FString> FOpenVDBModule::GetAllGridIDs(UVdbHandle const * VdbHandle)
{
	TArray<FString> GridIDs;
	TSharedPtr<VdbHandlePrivateType> VdbHandlePrivatePtr = VdbRegistry.FindChecked(VdbHandle->GetReadableName());
	try
	{
		VdbHandlePrivatePtr->GetAllGridIDs(GridIDs);
	}
	catch (const openvdb::Exception &e)
	{
		UE_LOG(LogOpenVDBModule, Error, TEXT("OpenVDBModule OpenVDB exception: %s"), UTF8_TO_TCHAR(e.what()));
	}
	catch (const std::exception& e)
	{
		UE_LOG(LogOpenVDBModule, Error, TEXT("OpenVDBModule exception: %s"), UTF8_TO_TCHAR(e.what()));
	}
	catch (const std::string& e)
	{
		UE_LOG(LogOpenVDBModule, Error, TEXT("OpenVDBModule exception: %s"), UTF8_TO_TCHAR(e.c_str()));
	}
	catch (...)
	{
		UE_LOG(LogOpenVDBModule, Fatal, TEXT("OpenVDBModule unexpected exception"));
	}
	return GridIDs;
}

void FOpenVDBModule::RemoveGrid(UVdbHandle const * VdbHandle, const FString &gridID)
{
	TSharedPtr<VdbHandlePrivateType> VdbHandlePrivatePtr = VdbRegistry.FindChecked(VdbHandle->GetReadableName());
	try
	{
		VdbHandlePrivatePtr->RemoveFileMeta(gridID);
		VdbHandlePrivatePtr->RemoveGridFromGridVec(gridID);
	}
	catch (const openvdb::Exception &e)
	{
		UE_LOG(LogOpenVDBModule, Error, TEXT("OpenVDBModule OpenVDB exception: %s"), UTF8_TO_TCHAR(e.what()));
	}
	catch (const std::exception& e)
	{
		UE_LOG(LogOpenVDBModule, Error, TEXT("OpenVDBModule exception: %s"), UTF8_TO_TCHAR(e.what()));
	}
	catch (const std::string& e)
	{
		UE_LOG(LogOpenVDBModule, Error, TEXT("OpenVDBModule exception: %s"), UTF8_TO_TCHAR(e.c_str()));
	}
	catch (...)
	{
		UE_LOG(LogOpenVDBModule, Fatal, TEXT("OpenVDBModule unexpected exception"));
	}
}

void FOpenVDBModule::SetRegionScale(UVdbHandle const * VdbHandle, const FIntVector &regionScale)
{
	TSharedPtr<VdbHandlePrivateType> VdbHandlePrivatePtr = VdbRegistry.FindChecked(VdbHandle->GetReadableName());
	try
	{
		if (regionScale.X > 0 && regionScale.Y > 0 && regionScale.Z > 0)
		{
			VdbHandlePrivatePtr->InsertFileMeta<openvdb::math::ScaleMap>(VdbHandlePrivateType::MetaName_RegionScale(), openvdb::math::ScaleMap(openvdb::Vec3d((double)regionScale.X, (double)regionScale.Y, (double)regionScale.Z)));
		}
	}
	catch (const openvdb::Exception &e)
	{
		UE_LOG(LogOpenVDBModule, Error, TEXT("OpenVDBModule OpenVDB exception: %s"), UTF8_TO_TCHAR(e.what()));
	}
	catch (const std::exception& e)
	{
		UE_LOG(LogOpenVDBModule, Error, TEXT("OpenVDBModule exception: %s"), UTF8_TO_TCHAR(e.what()));
	}
	catch (const std::string& e)
	{
		UE_LOG(LogOpenVDBModule, Error, TEXT("OpenVDBModule exception: %s"), UTF8_TO_TCHAR(e.c_str()));
	}
	catch (...)
	{
		UE_LOG(LogOpenVDBModule, Fatal, TEXT("OpenVDBModule unexpected exception"));
	}
}

void FOpenVDBModule::ReadGridTree(UVdbHandle const * VdbHandle, const FString &gridID, EMeshType MeshMethod, FIntVector &startFill, FIntVector &endFill)
{
	TSharedPtr<VdbHandlePrivateType> VdbHandlePrivatePtr = VdbRegistry.FindChecked(VdbHandle->GetReadableName());
	try
	{
		const bool threaded = true;
		VdbHandlePrivateType::GridTypePtr GridPtr = VdbHandlePrivatePtr->ReadGridTree(gridID, startFill, endFill);
		VdbHandlePrivatePtr->FillGrid_PerlinDensity(gridID, threaded, startFill, endFill, VdbHandle->PerlinSeed, VdbHandle->PerlinFrequency, VdbHandle->PerlinLacunarity, VdbHandle->PerlinPersistence, VdbHandle->PerlinOctaveCount);
		if (MeshMethod == EMeshType::MESH_TYPE_CUBES)
		{
			VdbHandlePrivatePtr->ExtractGridSurface_Cubes(gridID, threaded);
		}
		else if(MeshMethod == EMeshType::MESH_TYPE_MARCHING_CUBES)
		{
			VdbHandlePrivatePtr->ExtractGridSurface_MarchingCubes(gridID, threaded);
		}
		else
		{
			throw(std::string("Invalid mesh type!"));
		}
	}
	catch (const openvdb::Exception &e)
	{
		UE_LOG(LogOpenVDBModule, Error, TEXT("OpenVDBModule OpenVDB exception: %s"), UTF8_TO_TCHAR(e.what()));
	}
	catch (const std::exception& e)
	{
		UE_LOG(LogOpenVDBModule, Error, TEXT("OpenVDBModule exception: %s"), UTF8_TO_TCHAR(e.what()));
	}
	catch (const std::string& e)
	{
		UE_LOG(LogOpenVDBModule, Error, TEXT("OpenVDBModule exception: %s"), UTF8_TO_TCHAR(e.c_str()));
	}
	catch (...)
	{
		UE_LOG(LogOpenVDBModule, Fatal, TEXT("OpenVDBModule unexpected exception"));
	}
}

void FOpenVDBModule::GetVoxelCoord(UVdbHandle const * VdbHandle, const FString &gridID, const FVector &worldLocation, FIntVector &outVoxelCoord)
{
	TSharedPtr<VdbHandlePrivateType> VdbHandlePrivatePtr = VdbRegistry.FindChecked(VdbHandle->GetReadableName());
	try
	{
		VdbHandlePrivatePtr->GetIndexCoord(gridID, worldLocation, outVoxelCoord);
	}
	catch (const openvdb::Exception &e)
	{
		UE_LOG(LogOpenVDBModule, Error, TEXT("OpenVDBModule OpenVDB exception: %s"), UTF8_TO_TCHAR(e.what()));
	}
	catch (const std::exception& e)
	{
		UE_LOG(LogOpenVDBModule, Error, TEXT("OpenVDBModule exception: %s"), UTF8_TO_TCHAR(e.what()));
	}
	catch (const std::string& e)
	{
		UE_LOG(LogOpenVDBModule, Error, TEXT("OpenVDBModule exception: %s"), UTF8_TO_TCHAR(e.c_str()));
	}
	catch (...)
	{
		UE_LOG(LogOpenVDBModule, Fatal, TEXT("OpenVDBModule unexpected exception"));
	}
}

void FOpenVDBModule::MeshGrid(UVdbHandle const * VdbHandle,
	const FString &gridID,
	EMeshType MeshMethod,
	TSharedPtr<TArray<FVector>> &OutVertexBufferPtr,
	TSharedPtr<TArray<int32>> &OutPolygonBufferPtr,
	TSharedPtr<TArray<FVector>> &OutNormalBufferPtr,
	TSharedPtr<TArray<FVector2D>> &OutUVMapBufferPtr,
	TSharedPtr<TArray<FColor>> &OutVertexColorsBufferPtr,
	TSharedPtr<TArray<FProcMeshTangent>> &OutTangentsBufferPtr,
	FVector &worldStart,
	FVector &worldEnd,
	FVector &firstActive)
{
	TSharedPtr<VdbHandlePrivateType> VdbHandlePrivatePtr = VdbRegistry.FindChecked(VdbHandle->GetReadableName());
	try
	{
		VdbHandlePrivateType::GridTypePtr GridPtr = VdbHandlePrivatePtr->GetGridPtrChecked(gridID);
		if (MeshMethod == EMeshType::MESH_TYPE_CUBES)
		{
			VdbHandlePrivatePtr->MeshRegionCubes(gridID, worldStart, worldEnd, firstActive);
		}
		else if (MeshMethod == EMeshType::MESH_TYPE_MARCHING_CUBES)
		{
			VdbHandlePrivatePtr->MeshRegionMarchingCubes(gridID, worldStart, worldEnd, firstActive);
		}
		else
		{
			throw(std::string("Invalid mesh type!"));
		}
		VdbHandlePrivatePtr->GetGridSectionBuffers(gridID, OutVertexBufferPtr, OutPolygonBufferPtr, OutNormalBufferPtr, OutUVMapBufferPtr, OutVertexColorsBufferPtr, OutTangentsBufferPtr);
	}
	catch (const openvdb::Exception &e)
	{
		UE_LOG(LogOpenVDBModule, Error, TEXT("OpenVDBModule OpenVDB exception: %s"), UTF8_TO_TCHAR(e.what()));
	}
	catch (const std::exception& e)
	{
		UE_LOG(LogOpenVDBModule, Error, TEXT("OpenVDBModule exception: %s"), UTF8_TO_TCHAR(e.what()));
	}
	catch (const std::string& e)
	{
		UE_LOG(LogOpenVDBModule, Error, TEXT("OpenVDBModule exception: %s"), UTF8_TO_TCHAR(e.c_str()));
	}
	catch (...)
	{
		UE_LOG(LogOpenVDBModule, Fatal, TEXT("OpenVDBModule unexpected exception"));
	}
}

FIntVector FOpenVDBModule::GetRegionIndex(UVdbHandle const * VdbHandle, const FVector &worldLocation)
{
	FIntVector regionIndex;
	TSharedPtr<VdbHandlePrivateType> VdbHandlePrivatePtr = VdbRegistry.FindChecked(VdbHandle->GetReadableName());
	try
	{
		TSharedPtr<openvdb::TypedMetadata<openvdb::math::ScaleMap>> regionSizeMetaValue = VdbHandlePrivatePtr->GetFileMetaValue<openvdb::math::ScaleMap>(VdbHandlePrivateType::MetaName_RegionScale());
		check(regionSizeMetaValue.IsValid());
		openvdb::Vec3d regionCoords = regionSizeMetaValue->value().applyMap(openvdb::Vec3d(worldLocation.X, worldLocation.Y, worldLocation.Z));
		regionIndex.X = regionCoords.x();
		regionIndex.Y = regionCoords.y();
		regionIndex.Z = regionCoords.z();
	}
	catch (const openvdb::Exception &e)
	{
		UE_LOG(LogOpenVDBModule, Error, TEXT("OpenVDBModule OpenVDB exception: %s"), UTF8_TO_TCHAR(e.what()));
	}
	catch (const std::exception& e)
	{
		UE_LOG(LogOpenVDBModule, Error, TEXT("OpenVDBModule exception: %s"), UTF8_TO_TCHAR(e.what()));
	}
	catch (const std::string& e)
	{
		UE_LOG(LogOpenVDBModule, Error, TEXT("OpenVDBModule exception: %s"), UTF8_TO_TCHAR(e.c_str()));
	}
	catch (...)
	{
		UE_LOG(LogOpenVDBModule, Fatal, TEXT("OpenVDBModule unexpected exception"));
	}
	return regionIndex;
}

void FOpenVDBModule::WriteAllGrids(UVdbHandle const * VdbHandle)
{
	TSharedPtr<VdbHandlePrivateType> VdbHandlePrivatePtr = VdbRegistry.FindChecked(VdbHandle->GetReadableName());
	try
	{
		VdbHandlePrivatePtr->WriteChanges();
	}
	catch (const openvdb::Exception &e)
	{
		UE_LOG(LogOpenVDBModule, Error, TEXT("OpenVDBModule OpenVDB exception: %s"), UTF8_TO_TCHAR(e.what()));
	}
	catch (const std::exception& e)
	{
		UE_LOG(LogOpenVDBModule, Error, TEXT("OpenVDBModule exception: %s"), UTF8_TO_TCHAR(e.what()));
	}
	catch (const std::string& e)
	{
		UE_LOG(LogOpenVDBModule, Error, TEXT("OpenVDBModule exception: %s"), UTF8_TO_TCHAR(e.c_str()));
	}
	catch (...)
	{
		UE_LOG(LogOpenVDBModule, Fatal, TEXT("OpenVDBModule unexpected exception"));
	}
}

IMPLEMENT_GAME_MODULE(FOpenVDBModule, OpenVDBModule);