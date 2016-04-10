#pragma once
#include "OpenVDBModule.h"
#pragma warning(push)
#pragma warning(1:4211 4800 4503 4146)
#include "openvdb/openvdb.h"
#include "openvdb/io/Queue.h"
#include "GridOps.h"
#include "GridMetadata.h"

DECLARE_LOG_CATEGORY_EXTERN(LogOpenVDBModule, Log, All)

struct AsyncIONotifier
{
	typedef tbb::concurrent_hash_map<openvdb::io::Queue::Id, std::string> FilenameMap;
	FilenameMap filenames;

	// Callback function that prints the status of a completed task.
	void callback(openvdb::io::Queue::Id id, openvdb::io::Queue::Status status)
	{
		const bool succeeded = (status == openvdb::io::Queue::SUCCEEDED);
		FilenameMap::accessor acc;
		if (filenames.find(acc, id))
		{
			if (succeeded)
			{
				UE_LOG(LogOpenVDBModule, Verbose, TEXT("IONotifier: Wrote %s"), UTF8_TO_TCHAR(acc->second.c_str()));
			}
			else
			{
				UE_LOG(LogOpenVDBModule, Verbose, TEXT("IONotifier: Failed to write %s"), UTF8_TO_TCHAR(acc->second.c_str()));
			}
			filenames.erase(acc);
		}
	}
};

template<typename TreeType, typename IndexTreeType, typename... MetadataTypes>
class VdbHandlePrivate
{
public:
	typedef Vdb::GridOps::BasicMesher<TreeType, IndexTreeType> MesherOpType;
	typedef openvdb::Grid<TreeType> GridType;
	typedef typename GridType::Ptr GridTypePtr;
	typedef typename GridType::ConstPtr GridTypeCPtr;
	typedef openvdb::Grid<IndexTreeType> IndexGridType;
	typedef typename IndexGridType::Ptr IndexGridTypePtr;
	typedef typename IndexGridType::ConstPtr IndexGridTypeCPtr;

	UVdbHandle const * VdbHandle;
	boost::shared_ptr<openvdb::io::File> FilePtr;
	openvdb::GridPtrVecPtr GridsPtr;
	openvdb::MetaMap::Ptr FileMetaPtr;
	
	static FString MetaName_WorldName() { return TEXT("WorldName"); }
	static FString MetaName_RegionSize() { return TEXT("RegionSize"); }

	VdbHandlePrivate(UVdbHandle const * vdbHandle)
		: VdbHandle(vdbHandle)
	{
		//Initialize OpenVDB, our metadata types, and the vdb file
		openvdb::initialize();
		if (!IndexGridType::isRegistered())
		{
			IndexGridType::registerGrid();
		}
		InitializeMetadataTypes<MetadataTypes...>();
		FilePtr = boost::shared_ptr<openvdb::io::File>(new openvdb::io::File(TCHAR_TO_UTF8(*(VdbHandle->FilePath))));
		check(FilePtr != nullptr);
		FilePtr->setGridStatsMetadataEnabled(VdbHandle->EnableGridStats);

		if (!FPaths::FileExists(VdbHandle->FilePath))
		{
			//Create an empty vdb file
			FilePtr->write(openvdb::GridCPtrVec(), openvdb::MetaMap());
			UE_LOG(LogOpenVDBModule, Verbose, TEXT("IVdb: Created %s"), *(VdbHandle->FilePath));
		}
		check(FPaths::FileExists(VdbHandle->FilePath)); //TODO: Error handling when unable to create file. For now assume the file exists

		OpenFileGuard();
		GridsPtr = FilePtr->readAllGridMetadata();
		FileMetaPtr = FilePtr->getMetadata();
		check(GridsPtr != nullptr);
		check(FileMetaPtr != nullptr);
	}

	~VdbHandlePrivate()
	{
		//TODO: Write changes if they exist?
		openvdb::uninitialize();
		if (IndexGridType::isRegistered())
		{
			IndexGridType::unregisterGrid();
		}
	}

	template<typename TreeType, typename... MetadataTypes>
	openvdb::MetaMap::Ptr GetGridMeta(const FString &gridName)
	{
		openvdb::MetaMap::Ptr gridMetaPtr;
		GridTypePtr gridPtr = GetGridPtr<TreeType>(gridName);
		if (gridPtr != nullptr)
		{
			gridMetaPtr = gridPtr->copyMeta();
		}
		return gridMetaPtr;
	}

	template<typename TreeType>
	GridTypePtr ReadGridTree(const FString &gridName, FIntVector &activeStart, FIntVector &activeEnd)
	{
		GridTypePtr gridPtr = nullptr;
		OpenFileGuard();
		for (openvdb::GridPtrVec::iterator i = GridsPtr->begin(); i != GridsPtr->end(); ++i)
		{
			if (*i != nullptr && gridName == UTF8_TO_TCHAR((*i)->getName().c_str()))
			{
				if ((*i)->activeVoxelCount() == 0)
				{
					*i = FilePtr->readGrid(TCHAR_TO_UTF8(*gridName));
				}
				gridPtr = openvdb::gridPtrCast<GridType>(*i);
				check(gridPtr != nullptr);
				openvdb::CoordBBox activeBBox = gridPtr->evalActiveVoxelBoundingBox();
				activeStart.X = activeBBox.min().x();
				activeStart.Y = activeBBox.min().y();
				activeStart.Z = activeBBox.min().z();
				activeEnd.X = activeBBox.max().x();
				activeEnd.Y = activeBBox.max().y();
				activeEnd.Z = activeBBox.max().z();
				break;
			}
		}
		return gridPtr;
	}

	template<typename TreeType>
	GridTypePtr ReadGridTree(const FString &gridName, FVector &activeStart, FVector &activeEnd)
	{
		GridTypePtr gridPtr = nullptr;
		OpenFileGuard();
		for (openvdb::GridPtrVec::iterator i = GridsPtr->begin(); i != GridsPtr->end(); ++i)
		{
			if (*i != nullptr && gridName == UTF8_TO_TCHAR((*i)->getName().c_str()))
			{
				if ((*i)->activeVoxelCount() == 0)
				{
					*i = FilePtr->readGrid(TCHAR_TO_UTF8(*gridName));
				}
				gridPtr = openvdb::gridPtrCast<GridType>(*i);
				check(gridPtr != nullptr);
				openvdb::BBoxd activeBBoxd = gridPtr->indexToWorld(gridPtr->evalActiveVoxelBoundingBox());
				activeStart.X = activeBBoxd.min().x();
				activeStart.Y = activeBBoxd.min().y();
				activeStart.Z = activeBBoxd.min().z();
				activeEnd.X = activeBBoxd.max().x();
				activeEnd.Y = activeBBoxd.max().y();
				activeEnd.Z = activeBBoxd.max().z();
				break;
			}
		}
		return gridPtr;
	}

	template<typename FileMetaType>
	typename openvdb::TypedMetadata<FileMetaType>::Ptr GetFileMetaValue(const FString &metaName)
	{
		OpenFileGuard();
		return FileMetaPtr->getMetadata<openvdb::TypedMetadata<FileMetaType>>(TCHAR_TO_UTF8(*metaName));
	}

	template<typename FileMetaType>
	void InsertFileMeta(const FString &metaName, const FileMetaType &metaValue)
	{
		CloseFileGuard();
		FileMetaPtr->insertMeta(TCHAR_TO_UTF8(*metaName), openvdb::TypedMetadata<FileMetaType>(metaValue));
	}

	void RemoveFileMeta(const FString &metaName)
	{
		CloseFileGuard();
		FileMetaPtr->removeMeta(TCHAR_TO_UTF8(*metaName));
	}

	void RemoveGridFromGridVec(const FString &gridName)
	{
		for (openvdb::GridPtrVec::iterator i = GridsPtr->begin(); i != GridsPtr->end(); ++i)
		{
			if (gridName == UTF8_TO_TCHAR((*i)->getName().c_str()))
			{
				GridsPtr->erase(i);
				return;
			}
		}
	}

	openvdb::GridBase::Ptr GetGridBasePtr(const FString &gridName) const
	{
		for (openvdb::GridPtrVec::iterator i = GridsPtr->begin(); i != GridsPtr->end(); ++i)
		{
			if (gridName == UTF8_TO_TCHAR((*i)->getName().c_str()))
			{
				return *i;
			}
		}
		return nullptr;
	}

	template<typename TreeType>
	GridTypePtr GetGridPtr(const FString &gridName) const
	{
		for (openvdb::GridPtrVec::iterator i = GridsPtr->begin(); i != GridsPtr->end(); ++i)
		{
			if (gridName == UTF8_TO_TCHAR((*i)->getName().c_str()))
			{
				GridTypePtr gridPtr = openvdb::gridPtrCast<GridType>(*i);
				check(gridPtr != nullptr);
				return gridPtr;
			}
		}
		return nullptr;
	}

	template<typename MetadataType>
	typename openvdb::TypedMetadata<MetadataType>::Ptr GetGridMetaValue(const FString &gridName, const FString &metaName) const
	{
		openvdb::TypedMetadata<MetadataType>::Ptr metaValuePtr;
		openvdb::GridBase::Ptr gridBasePtr = GetGridBasePtr(gridName);
		if (gridBasePtr != nullptr)
		{
			metaValuePtr = gridBasePtr->getMetadata<openvdb::TypedMetadata<MetadataType>>(TCHAR_TO_UTF8(*gridName));
		}
		return metaValuePtr;
	}

	template<typename TreeType, typename MetadataType>
	void InsertGridMeta(const FString &gridName, const FString &metaName, const MetadataType &metaValue)
	{
		GridTypePtr gridPtr = GetGridPtr<TreeType>(gridName);
		if (gridPtr != nullptr)
		{
			gridPtr->insertMeta<openvdb::TypedMetadata<MetadataType>>(TCHAR_TO_UTF8(*metaName), openvdb::TypedMetadata<MetadataType>(metaValue));
		}
	}

	template<typename TreeType, typename MetadataType>
	void RemoveGridMeta(const FString &gridName, const FString &metaName)
	{
		GridTypePtr gridPtr = GetGridPtr<TreeType>(gridName);
		if (gridPtr != nullptr)
		{
			gridPtr->removeMeta(TCHAR_TO_UTF8(*metaName));
		}
	}

	void WriteChanges()
	{
		CloseFileGuard();
		FilePtr->write(*GridsPtr, *FileMetaPtr);
	}

	void WriteChangesAsync()
	{
		OpenFileGuard();
		AsyncIONotifier notifier;
		openvdb::io::Queue queue;
		queue.addNotifier(boost::bind(&AsyncIONotifier::callback, &notifier, _1, _2));
		queue.write(*GridsPtr, *FilePtr, *FileMetaPtr);
	}

	template<typename TreeType, typename IndexTreeType>
	void MeshRegion(const FString &gridName, float surfaceValue, TArray<FVector> &vertexBuffer, TArray<int32> &polygonBuffer, TArray<FVector> &normalBuffer)
	{
		GridTypePtr gridPtr = GetGridPtr<TreeType>(gridName);
		if (gridPtr != nullptr)
		{
			TSharedPtr<MesherOpType> mesherOpPtr = TSharedPtr<MesherOpType>(new MesherOpType(gridPtr, vertexBuffer, polygonBuffer, normalBuffer));
			mesherOpPtr->doActivateValuesOp(surfaceValue);
			check(gridPtr->activeVoxelCount() <= INT32_MAX);
			vertexBuffer.Reserve(gridPtr->activeVoxelCount());
			mesherOpPtr->doMeshOp(true);
		}
	}

	template<typename TreeType>
	void ReadGridIndexBounds(const FString &gridName, FIntVector &indexStart, FIntVector &indexEnd) const
	{
		GridTypePtr gridPtr = GetGridPtr<TreeType>(gridName);
		if (gridPtr != nullptr)
		{
			const openvdb::CoordBBox bbox = gridPtr->evalActiveVoxelBoundingBox();
			indexStart.X = bbox.min().x();
			indexStart.Y = bbox.min().y();
			indexStart.Z = bbox.min().z();
			indexEnd.X = bbox.max().x();
			indexEnd.Y = bbox.max().y();
			indexEnd.Z = bbox.max().z();
		}
	}

	template<typename TreeType>
	void FillGrid_PerlinDensity(const FString &gridName, const FIntVector &fillIndexStart, const FIntVector &fillIndexEnd, float frequency, float lacunarity, float persistence, int32 octaveCount, FIntVector &activeStart, FIntVector &activeEnd)
	{
		GridTypePtr gridPtr = GetGridPtr<TreeType>(gridName);
		const openvdb::CoordBBox fillBBox = openvdb::CoordBBox(openvdb::Coord(fillIndexStart.X, fillIndexStart.Y, fillIndexStart.Z), openvdb::Coord(fillIndexEnd.X, fillIndexEnd.Y, fillIndexEnd.Z));
		if (gridPtr == nullptr)
		{
			GridsPtr->push_back(GridType::create());
			gridPtr = openvdb::gridPtrCast<GridType>(GridsPtr->back());
			gridPtr->setName(TCHAR_TO_UTF8(*gridName));
			gridPtr->setTransform(openvdb::math::Transform::Ptr(new openvdb::math::Transform(openvdb::math::UniformScaleMap::Ptr(new openvdb::math::UniformScaleMap(1.0)))));
		}

		//Noise module parameters are at the grid-level metadata
		openvdb::FloatMetadata::Ptr frequencyMeta = gridPtr->getMetadata<openvdb::FloatMetadata>("frequency");
		openvdb::FloatMetadata::Ptr lacunarityMeta = gridPtr->getMetadata<openvdb::FloatMetadata>("lacunarity");
		openvdb::FloatMetadata::Ptr persistenceMeta = gridPtr->getMetadata<openvdb::FloatMetadata>("persistence");
		openvdb::Int32Metadata::Ptr octaveCountMeta = gridPtr->getMetadata<openvdb::Int32Metadata>("octaveCount");
		if (frequencyMeta == nullptr || !openvdb::math::isApproxEqual(frequency, frequencyMeta->value()) ||
			lacunarityMeta == nullptr || !openvdb::math::isApproxEqual(lacunarity, lacunarityMeta->value()) ||
			persistenceMeta == nullptr || !openvdb::math::isApproxEqual(persistence, persistenceMeta->value()) ||
			octaveCountMeta == nullptr || !openvdb::math::isExactlyEqual(octaveCount, octaveCountMeta->value()))
		{
			//Update the Perlin noise parameters
			gridPtr->insertMeta("frequency", openvdb::FloatMetadata(frequency));
			gridPtr->insertMeta("lacunarity", openvdb::FloatMetadata(lacunarity));
			gridPtr->insertMeta("persistence", openvdb::FloatMetadata(persistence));
			gridPtr->insertMeta("octaveCount", openvdb::Int32Metadata(octaveCount));

			//Activate mask values such that there is a single padded region along the outer edge with
			//values on and false and all other values within the padded region have values on and true.
			check(!fillBBox.empty());
			openvdb::CoordBBox bboxPadded = fillBBox;
			bboxPadded.expand(1);

			//Create a mask enclosing the region such that the outer edge voxels are on but false
			openvdb::BoolGrid::Ptr mask = openvdb::BoolGrid::create(false);
			mask->fill(bboxPadded, /*value*/false, /*state*/true);
			mask->fill(fillBBox, /*value*/true, /*state*/true);
			Vdb::GridOps::PerlinNoiseFillOp<TreeType> noiseFillOp(gridPtr->transform(), frequency, lacunarity, persistence, octaveCount);
			openvdb::tools::transformValues(mask->cbeginValueOn(), *gridPtr, noiseFillOp);
		}
		openvdb::CoordBBox activeBBox = gridPtr->evalActiveVoxelBoundingBox();
		activeStart.X = activeBBox.min().x();
		activeStart.Y = activeBBox.min().y();
		activeStart.Z = activeBBox.min().z();
		activeEnd.X = activeBBox.max().x();
		activeEnd.Y = activeBBox.max().y();
		activeEnd.Z = activeBBox.max().z();
	}

private:
	template<typename MetadataType, typename... OtherMetadataTypes>
	inline void InitializeMetadataTypes()
	{
		InitializeMetadata<MetadataType>();
	}

	template<typename MetadataType>
	void InitializeMetadata()
	{
		if (!openvdb::TypedMetadata<MetadataType>::isRegisteredType())
		{
			openvdb::TypedMetadata<MetadataType>::registerType();
		}
	}

	void OpenFileGuard()
	{
		if (FilePtr != nullptr && !FilePtr->isOpen())
		{
			FilePtr->open(VdbHandle->EnableDelayLoad);
		}
	}

	void CloseFileGuard()
	{
		if (FilePtr != nullptr && FilePtr->isOpen())
		{
			FilePtr->close();
		}
	}
	
	void ClipGridTree(GridTypePtr gridPtr, const openvdb::CoordBBox &indexBBox, openvdb::CoordBBox &activeBBox)
	{
		if (gridPtr->tree().empty())
		{
			gridPtr = openvdb::gridPtrCast<GridType>(FilePtr->readGrid(TCHAR_TO_UTF8(*gridName), gridPtr->tree().indexToWorld(indexBBox)));
		}
		activeBBox = gridPtr->evalActiveVoxelBoundingBox();
	}

	void ClipGridTree(GridTypePtr gridPtr, const openvdb::BBoxd &worldBBox, openvdb::BBoxd &activeBBoxd)
	{
		if (gridPtr->tree().empty())
		{
			gridPtr = openvdb::gridPtrCast<GridType>(FilePtr->readGrid(TCHAR_TO_UTF8(*gridName), worldBBox));
		}
		activeBBoxd = gridPtr->tree().indexToWorld(gridPtr->evalActiveVoxelBoundingBox());
	}
};

typedef openvdb::FloatTree TreeType;
typedef VdbHandlePrivate<TreeType, Vdb::GridOps::IndexTreeType, Vdb::Metadata::RegionMetadata, openvdb::math::ScaleMap> VdbHandlePrivateType;
typedef TMap<FString, TSharedPtr<VdbHandlePrivateType>> VdbRegistryType;