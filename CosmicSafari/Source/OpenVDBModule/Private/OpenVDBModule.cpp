#include "OpenVDBModule.h"
#include "VDBHandlePrivate.h"

DEFINE_LOG_CATEGORY(LogOpenVDBModule)
typedef TMap<FString, TSharedPtr<VdbHandlePrivateType>> VdbRegistryType;
static VdbRegistryType VdbRegistry;
static AsyncIONotifierType AsyncIO;

bool FOpenVDBModule::OpenVoxelDatabase(const FString &vdbName, const FString &vdbFilepath, bool enableGridStats, bool enableDelayLoad)
{
	bool isOpened = false;
	TSharedPtr<VdbHandlePrivateType> VdbHandlePrivatePtr;
	try
	{
		check(!VdbHandlePrivatePtr.IsValid());
		TSharedPtr<VdbHandlePrivateType> *vdb = VdbRegistry.Find(vdbName);
		if (vdb == nullptr)
		{
			VdbHandlePrivatePtr = TSharedPtr<VdbHandlePrivateType>(new VdbHandlePrivateType(AsyncIO));
			VdbRegistry.Add(vdbName, VdbHandlePrivatePtr);
		}
		else
		{
			check(vdb->IsValid());
			VdbHandlePrivatePtr = *vdb;
		}
		check(VdbHandlePrivatePtr.IsValid());

		FString path = FPaths::GetPath(vdbFilepath);
		FString file = FPaths::GetCleanFilename(vdbFilepath);
		FPaths::RemoveDuplicateSlashes(path);
		FPaths::NormalizeDirectoryName(path);
		FPaths::NormalizeFilename(file);
		FString validatedFullPath = path + TEXT("/") + file;
		FPaths::NormalizeDirectoryName(validatedFullPath);
		isOpened = VdbHandlePrivatePtr->InitializeDatabase(validatedFullPath, enableGridStats, enableDelayLoad);
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

	if (VdbHandlePrivatePtr.IsValid())
	{
		VdbHandlePrivatePtr->IsDatabaseOpen = isOpened;
	}
	return isOpened;
}

bool FOpenVDBModule::CloseVoxelDatabase(const FString &vdbName, bool isFinal, bool asyncWrite)
{
	bool isClosed = false;
	TSharedPtr<VdbHandlePrivateType> VdbHandlePrivatePtr;
	try
	{
		//If async writing, never release shared resources after writing
		check(!VdbHandlePrivatePtr.IsValid());
		TSharedPtr<VdbHandlePrivateType> *vdb = VdbRegistry.Find(vdbName);
		if (vdb)
		{
			//If changes successfully end up in the file then remove the VDB
			check(vdb->IsValid());
			VdbHandlePrivatePtr = *vdb;
			if (asyncWrite)
			{
				VdbHandlePrivatePtr->WriteChangesAsync(isFinal);
			}
			else
			{
				VdbHandlePrivatePtr->WriteChanges(isFinal);
			}
		}
		isClosed = true;
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

	VdbHandlePrivatePtr->IsDatabaseOpen = !isClosed;
	if (isFinal)
	{
		VdbRegistry.Remove(vdbName);
	}
	return isClosed;
}

bool FOpenVDBModule::WriteChanges(const FString &vdbName, bool isFinal, bool asyncWrite)
{
	bool isFileChanged = false;
	TSharedPtr<VdbHandlePrivateType> VdbHandlePrivatePtr = VdbRegistry.FindChecked(vdbName);
	try
	{
		if (asyncWrite)
		{
			isFileChanged = VdbHandlePrivatePtr->WriteChangesAsync(isFinal);
		}
		else
		{
			isFileChanged = VdbHandlePrivatePtr->WriteChanges(isFinal);
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
	return isFileChanged;
}

FString FOpenVDBModule::AddGrid(const FString &vdbName, const FString &gridName, const FIntVector &regionIndex, const FVector &voxelSize, TArray<FProcMeshSection> &sectionBuffers)
{
	FString gridID;
	TSharedPtr<VdbHandlePrivateType> VdbHandlePrivatePtr = VdbRegistry.FindChecked(vdbName);
	try
	{
		openvdb::math::ScaleMap regionSizeMetaValue(openvdb::Vec3d(1));
		const bool isMetaValueValid = VdbHandlePrivatePtr->GetFileMetaValue<openvdb::math::ScaleMap>(MetaName_RegionScale(), regionSizeMetaValue);
		check(isMetaValueValid);

		const openvdb::Coord startIndexCoord = openvdb::Coord((int32)regionIndex.X, (int32)regionIndex.Y, (int32)regionIndex.Z);
		const openvdb::Coord endIndexCoord = openvdb::Coord((int32)regionIndex.X + 1, (int32)regionIndex.Y + 1, (int32)regionIndex.Z + 1);
		const openvdb::Vec3d regionStart = regionSizeMetaValue.applyMap(openvdb::Vec3d((double)startIndexCoord.x(), (double)startIndexCoord.y(), (double)startIndexCoord.z()));
		const openvdb::Vec3d regionEnd = regionSizeMetaValue.applyMap(openvdb::Vec3d((double)endIndexCoord.x(), (double)endIndexCoord.y(), (double)endIndexCoord.z()));
		const openvdb::Vec3d regionDimensions = regionEnd - regionStart;

		const FVector startWorld(regionStart.x(), regionStart.y(), regionStart.z());
		const FVector endWorld(regionEnd.x()-1, regionEnd.y()-1, regionEnd.z()-1); //Minus 1 of each coordinate just for the display string. The value is not used
		const FIntVector dimensions = FIntVector(regionDimensions.x(), regionDimensions.y(), regionDimensions.z()) - FIntVector(1, 1, 1);
		gridID = gridName + TEXT("[") + startWorld.ToString() + TEXT("]");
		VdbHandlePrivatePtr->AddGrid(gridID, dimensions, startWorld, voxelSize, sectionBuffers);
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

void FOpenVDBModule::ReadGridTree(const FString &vdbName, const FString &gridID, FIntVector &startIndex, FIntVector &endIndex)
{
	TSharedPtr<VdbHandlePrivateType> VdbHandlePrivatePtr = VdbRegistry.FindChecked(vdbName);
	try
	{
		VdbHandlePrivatePtr->ReadGridTree(gridID, startIndex, endIndex);
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

bool FOpenVDBModule::FillTreePerlin(const FString &vdbName,
	const FString &gridID,
	FIntVector &startFill,
	FIntVector &endFill,
	int32 seed,
	float frequency,
	float lacunarity,
	float persistence,
	int32 octaveCount,
	bool threaded)
{
	bool isChanged = false;
	TSharedPtr<VdbHandlePrivateType> VdbHandlePrivatePtr = VdbRegistry.FindChecked(vdbName);
	try
	{
		isChanged = VdbHandlePrivatePtr->FillGrid_PerlinDensity(gridID, threaded, startFill, endFill, seed, frequency, lacunarity, persistence, octaveCount);
		if (isChanged)
		{
			VdbHandlePrivatePtr->CalculateGradient(gridID, threaded);
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
	return isChanged;
}

bool FOpenVDBModule::ExtractIsoSurface(const FString &vdbName, const FString &gridID, EMeshType MeshMethod, TArray<TEnumAsByte<EVoxelType>> &sectionMaterialIDs, FBox &gridDimensions, FVector &initialLocation, bool threaded)
{
	TSharedPtr<VdbHandlePrivateType> VdbHandlePrivatePtr = VdbRegistry.FindChecked(vdbName);
	bool hasActiveVoxels = false;
	try
	{
		if (MeshMethod == EMeshType::MESH_TYPE_CUBES)
		{
			VdbHandlePrivatePtr->ExtractGridSurface_Cubes(gridID, threaded);
		}
		else if (MeshMethod == EMeshType::MESH_TYPE_MARCHING_CUBES)
		{
			VdbHandlePrivatePtr->ExtractGridSurface_MarchingCubes(gridID, threaded);
		}
		else
		{
			throw(std::string("Invalid mesh type!"));
		}

		VdbHandlePrivatePtr->ApplyVoxelTypes(gridID, threaded, sectionMaterialIDs);
		hasActiveVoxels = VdbHandlePrivatePtr->GetGridDimensions(gridID, gridDimensions, initialLocation);
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
	return hasActiveVoxels;
}

void FOpenVDBModule::MeshGrid(const FString &vdbName, const FString &gridID, EMeshType MeshMethod)
{
	TSharedPtr<VdbHandlePrivateType> VdbHandlePrivatePtr = VdbRegistry.FindChecked(vdbName);
	try
	{
		if (MeshMethod == EMeshType::MESH_TYPE_CUBES)
		{
			VdbHandlePrivatePtr->MeshRegionCubes(gridID);
		}
		else if (MeshMethod == EMeshType::MESH_TYPE_MARCHING_CUBES)
		{
			VdbHandlePrivatePtr->MeshRegionMarchingCubes(gridID);
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

TArray<FString> FOpenVDBModule::GetAllGridIDs(const FString &vdbName)
{
	TArray<FString> GridIDs;
	TSharedPtr<VdbHandlePrivateType> VdbHandlePrivatePtr = VdbRegistry.FindChecked(vdbName);
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

void FOpenVDBModule::RemoveGrid(const FString &vdbName, const FString &gridID)
{
	TSharedPtr<VdbHandlePrivateType> VdbHandlePrivatePtr = VdbRegistry.FindChecked(vdbName);
	try
	{
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

void FOpenVDBModule::SetRegionScale(const FString &vdbName, const FIntVector &regionScale)
{
	TSharedPtr<VdbHandlePrivateType> VdbHandlePrivatePtr = VdbRegistry.FindChecked(vdbName);
	try
	{
		if (regionScale.X > 0 && regionScale.Y > 0 && regionScale.Z > 0)
		{
			VdbHandlePrivatePtr->InsertFileMeta<openvdb::math::ScaleMap>(MetaName_RegionScale(), openvdb::math::ScaleMap(openvdb::Vec3d((double)regionScale.X, (double)regionScale.Y, (double)regionScale.Z)));
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

void FOpenVDBModule::GetVoxelCoord(const FString &vdbName, const FString &gridID, const FVector &worldLocation, FIntVector &outVoxelCoord)
{
	TSharedPtr<VdbHandlePrivateType> VdbHandlePrivatePtr = VdbRegistry.FindChecked(vdbName);
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

FIntVector FOpenVDBModule::GetRegionIndex(const FString &vdbName, const FVector &worldLocation)
{
	FIntVector regionIndex;
	TSharedPtr<VdbHandlePrivateType> VdbHandlePrivatePtr = VdbRegistry.FindChecked(vdbName);
	try
	{
		openvdb::math::ScaleMap regionSizeMetaValue(openvdb::Vec3d(1));
		const bool isMetaValueValid = VdbHandlePrivatePtr->GetFileMetaValue<openvdb::math::ScaleMap>(MetaName_RegionScale(), regionSizeMetaValue);
		check(isMetaValueValid);

		openvdb::Vec3d regionCoords = regionSizeMetaValue.applyMap(openvdb::Vec3d(worldLocation.X, worldLocation.Y, worldLocation.Z));
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

bool FOpenVDBModule::GetGridDimensions(const FString &vdbName, const FString &gridID, FVector &startLocation)
{
	TSharedPtr<VdbHandlePrivateType> VdbHandlePrivatePtr = VdbRegistry.FindChecked(vdbName);
	bool hasActiveVoxels = false;
	try
	{
		hasActiveVoxels = VdbHandlePrivatePtr->GetGridDimensions(gridID, startLocation);
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
	return hasActiveVoxels;
}

bool FOpenVDBModule::GetGridDimensions(const FString &vdbName, const FString &gridID, FBox &worldBounds)
{
	TSharedPtr<VdbHandlePrivateType> VdbHandlePrivatePtr = VdbRegistry.FindChecked(vdbName);
	bool hasActiveVoxels = false;
	try
	{
		hasActiveVoxels = VdbHandlePrivatePtr->GetGridDimensions(gridID, worldBounds);
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
	return hasActiveVoxels;
}

bool FOpenVDBModule::GetGridDimensions(const FString &vdbName, const FString &gridID, FBox &worldBounds, FVector &startLocation)
{
	TSharedPtr<VdbHandlePrivateType> VdbHandlePrivatePtr = VdbRegistry.FindChecked(vdbName);
	bool hasActiveVoxels = false;
	try
	{
		hasActiveVoxels = VdbHandlePrivatePtr->GetGridDimensions(gridID, worldBounds, startLocation);
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
	return hasActiveVoxels;
}

IMPLEMENT_GAME_MODULE(FOpenVDBModule, OpenVDBModule);