#include "OpenVDBModule.h"
#include "VDBHandlePrivate.h"

DEFINE_LOG_CATEGORY(LogOpenVDBModule)
typedef TMap<FString, TSharedPtr<VdbHandlePrivateType>> VdbRegistryType;
static VdbRegistryType VdbRegistry;

bool FOpenVDBModule::RegisterVdb(const FString &vdbName, const FString &vdbFilepath, bool enableGridStats, bool enableDelayLoad)
{
	bool isRegistered = false;
	try
	{
		if (!VdbRegistry.Contains(vdbName))
		{
			TSharedPtr<VdbHandlePrivateType> VdbHandlePrivatePtr = TSharedPtr<VdbHandlePrivateType>(new VdbHandlePrivateType(vdbFilepath, enableGridStats, enableDelayLoad));
			VdbHandlePrivatePtr->InitGrids();
			VdbRegistry.Add(vdbName, VdbHandlePrivatePtr);
		}
		//Registration successful if we made it here with no errors thrown
		isRegistered = true;
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
	return isRegistered;
}

void FOpenVDBModule::UnregisterVdb(const FString &vdbName)
{
	try
	{
		if (VdbRegistry.Contains(vdbName))
		{
			VdbRegistry[vdbName]->WriteChanges();
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

FString FOpenVDBModule::AddGrid(const FString &vdbName, const FString &gridName, const FIntVector &regionIndex, const FVector &voxelSize, TArray<FProcMeshSection> &sectionBuffers)
{
	FString gridID;
	TSharedPtr<VdbHandlePrivateType> VdbHandlePrivatePtr = VdbRegistry.FindChecked(vdbName);
	try
	{
		TSharedPtr<openvdb::TypedMetadata<openvdb::math::ScaleMap>> regionSizeMetaValue = VdbHandlePrivatePtr->GetFileMetaValue<openvdb::math::ScaleMap>(VdbHandlePrivateType::MetaName_RegionScale());
		check(regionSizeMetaValue.IsValid());

		const openvdb::Coord startIndexCoord = openvdb::Coord((int32)regionIndex.X, (int32)regionIndex.Y, (int32)regionIndex.Z);
		const openvdb::Coord endIndexCoord = openvdb::Coord((int32)regionIndex.X + 1, (int32)regionIndex.Y + 1, (int32)regionIndex.Z + 1);
		const openvdb::Vec3d regionStart = regionSizeMetaValue->value().applyMap(openvdb::Vec3d((double)startIndexCoord.x(), (double)startIndexCoord.y(), (double)startIndexCoord.z()));
		const openvdb::Vec3d regionEnd = regionSizeMetaValue->value().applyMap(openvdb::Vec3d((double)endIndexCoord.x(), (double)endIndexCoord.y(), (double)endIndexCoord.z()));
		const openvdb::Vec3d regionDimensions = regionEnd - regionStart;

		const FVector startWorld(regionStart.x(), regionStart.y(), regionStart.z());
		const FVector endWorld(regionEnd.x()-1, regionEnd.y()-1, regionEnd.z()-1); //Minus 1 of each coordinate just for the display string. The value is not used
		const FIntVector dimensions = FIntVector(regionDimensions.x(), regionDimensions.y(), regionDimensions.z()) - FIntVector(1, 1, 1);
		gridID = gridName + TEXT(" ") + startWorld.ToString() + TEXT(",") + endWorld.ToString();
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

void FOpenVDBModule::FillTreePerlin(const FString &vdbName,
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
	TSharedPtr<VdbHandlePrivateType> VdbHandlePrivatePtr = VdbRegistry.FindChecked(vdbName);
	try
	{
		VdbHandlePrivatePtr->FillGrid_PerlinDensity(gridID, threaded, startFill, endFill, seed, frequency, lacunarity, persistence, octaveCount);
		VdbHandlePrivatePtr->CalculateGradient(gridID, threaded);
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
			//TODO sectionMaterialIDs
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
		//TODO: Remove terrain mesh component and reconcile mesh section indices
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

void FOpenVDBModule::SetRegionScale(const FString &vdbName, const FIntVector &regionScale)
{
	TSharedPtr<VdbHandlePrivateType> VdbHandlePrivatePtr = VdbRegistry.FindChecked(vdbName);
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

void FOpenVDBModule::WriteAllGrids(const FString &vdbName)
{
	TSharedPtr<VdbHandlePrivateType> VdbHandlePrivatePtr = VdbRegistry.FindChecked(vdbName);
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

bool FOpenVDBModule::GetGridDimensions(const FString &vdbName, const FString &gridID, FBox &worldBounds, FVector &firstActive)
{
	TSharedPtr<VdbHandlePrivateType> VdbHandlePrivatePtr = VdbRegistry.FindChecked(vdbName);
	bool hasActiveVoxels = false;
	try
	{
		hasActiveVoxels = VdbHandlePrivatePtr->GetGridDimensions(gridID, worldBounds, firstActive);
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