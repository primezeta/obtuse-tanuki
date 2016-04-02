#pragma once
#include "EngineMinimal.h"
#include "VDBHandle.h"
#pragma warning(push)
#pragma warning(1:4211 4800 4503 4146)
#include "openvdb/openvdb.h"
#include "openvdb/io/Queue.h"
#include "GridOps.h"
#include "GridMetadata.h"

DECLARE_LOG_CATEGORY_EXTERN(LogVDBHandle, Log, All)

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
				UE_LOG(LogVDBHandle, Verbose, TEXT("IONotifier: Wrote %s"), UTF8_TO_TCHAR(acc->second.c_str()));
			}
			else
			{
				UE_LOG(LogVDBHandle, Verbose, TEXT("IONotifier: Failed to write %s"), UTF8_TO_TCHAR(acc->second.c_str()));
			}
			filenames.erase(acc);
		}
	}
};

template<typename TreeType, typename... MetadataTypes>
class VDBHandlePrivate
{
public:
	typedef Vdb::GridOps::BasicMesher<TreeType> MesherOpType;
	typedef openvdb::Grid<TreeType> GridType;

	UVDBHandle const * VDBHandle;
	TSharedPtr<openvdb::io::File> FilePtr;
	TSharedPtr<openvdb::GridPtrVec> GridsPtr;
	TSharedPtr<openvdb::MetaMap> FileMetaPtr;

	VDBHandlePrivate(UVDBHandle const * vdbHandle)
		: VDBHandle(vdbHandle)
	{
		//Initialize OpenVDB, our metadata types, and the vdb file
		openvdb::initialize();
		InitializeMetadataTypes<MetadataTypes...>();
		FilePtr = TSharedPtr<openvdb::io::File>(new openvdb::io::File(TCHAR_TO_UTF8(*(VDBHandle->FilePath))));
		check(FilePtr.IsValid());
		FilePtr->setGridStatsMetadataEnabled(VDBHandle->EnableGridStats);

		//Create the vdb file if it does not exist
		if (!FPaths::FileExists(VDBHandle->FilePath))
		{
			FilePtr->write(openvdb::GridPtrVec());
			UE_LOG(LogVDBHandle, Verbose, TEXT("IVdb: Created %s"), *(VDBHandle->FilePath));
		}
		check(FPaths::FileExists(VDBHandle->FilePath)); //TODO: Error handling when unable to create file. For now assume the file exists

		OpenFileGuard();
		GridsPtr = TSharedPtr<openvdb::GridPtrVec>(FilePtr->readAllGridMetadata().get());
		FileMetaPtr = TSharedPtr<openvdb::MetaMap>(FilePtr->getMetadata().get());
		check(GridsPtr.IsValid());
		check(FileMetaPtr.IsValid());
	}

	~VDBHandlePrivate()
	{
		//TODO: Write changes if they exist?
		openvdb::uninitialize();
	}

	template<typename TreeType, typename... MetadataTypes>
	TSharedPtr<openvdb::MetaMap> GetGridMeta(const FString &gridName)
	{
		TSharedPtr<openvdb::MetaMap> gridMetaPtr;
		TSharedPtr<GridType> gridPtr = GetGridPtr<TreeType>(gridName);
		if (gridPtr.isValid())
		{
			gridMetaPtr = TSharedPtr<openvdb::MetaMap>(gridPtr->copyMeta().get());
		}
		return gridMetaPtr;
	}

	template<typename TreeType, typename... MetadataTypes>
	TSharedPtr<GridType> ReadGridTree(const FString &gridName, FIntVector &indexStart, FIntVector &indexEnd)
	{
		OpenFileGuard();
		for (openvdb::GridPtrVec::iterator i = GridsPtr->begin(); i != GridsPtr->end(); ++i)
		{
			if (*i != nullptr && gridName == UTF8_TO_TCHAR((*i)->getName().c_str()))
			{
				TSharedPtr<GridType> gridPtr = TSharedPtr<GridType>(openvdb::gridPtrCast<GridType>(*i).get());
				openvdb::BBoxd bboxd = gridPtr->transform().indexToWorld(openvdb::CoordBBox(openvdb::Coord(indexStart.X, indexStart.Y, indexStart.Z), openvdb::Coord(indexEnd.X, indexEnd.Y, indexEnd.Z)));
				if (gridPtr->tree().empty())
				{
					GridType::Ptr ptr = openvdb::gridPtrCast<GridType>(FilePtr->readGrid(TCHAR_TO_UTF8(*gridName), bboxd));
					gridPtr = TSharedPtr<GridType>(ptr.get());
					*i = ptr;
				}
				return gridPtr;
			}
		}
		return TSharedPtr<GridType>();
	}

	template<typename TreeType, typename... MetadataTypes>
	TSharedPtr<GridType> ReadGridTree(const FString &gridName, FVector &start, FVector &end)
	{
		OpenFileGuard();
		for (openvdb::GridPtrVec::iterator i = GridsPtr->begin(); i != GridsPtr->end(); ++i)
		{
			if (*i != nullptr && gridName == UTF8_TO_TCHAR((*i)->getName().c_str()))
			{
				openvdb::BBoxd bboxd(openvdb::Coord(start.X, start.Y, start.Z), openvdb::Coord(end.X, end.Y, end.Z));
				TSharedPtr<GridType> gridPtr = TSharedPtr<GridType>(openvdb::gridPtrCast<GridType>(*i).get());
				if (gridPtr->tree().empty())
				{
					GridType::Ptr ptr = openvdb::gridPtrCast<GridType>(FilePtr->readGrid(TCHAR_TO_UTF8(*gridName), bboxd));
					gridPtr = TSharedPtr<GridType>(ptr.get());
					*i = ptr;
				}
				return gridPtr;
			}
		}
		return TSharedPtr<GridType>();
	}

	template<typename FileMetaType>
	TSharedPtr<openvdb::TypedMetadata<FileMetaType>> GetFileMetaValue(const FString &metaName, const FileMetaType &metaValue)
	{
		OpenFileGuard();
		return TSharedPtr<openvdb::TypedMetadata<FileMetaType>>(FileMetaPtr->getMetadata<FileMetaType>(TCHAR_TO_UTF8(*metaName).get()));
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

	template<typename TreeType>
	TSharedPtr<GridType> GetGridPtr(const FString &gridName)
	{
		for (openvdb::GridPtrVec::iterator i = GridsPtr->begin(); i != GridsPtr->end(); ++i)
		{
			if (gridName == UTF8_TO_TCHAR((*i)->getName().c_str()))
			{
				return TSharedPtr<GridType>(openvdb::gridPtrCast<GridType>(*i).get());
			}
		}
		return TSharedPtr<GridType>();
	}

	template<typename TreeType, typename MetadataType>
	TSharedPtr<openvdb::TypedMetadata<MetadataType>> GetGridMetaValue(const FString &gridName, const FString &metaName)
	{
		TSharedPtr<openvdb::TypedMetadata<MetadataType>> metaValuePtr;
		TSharedPtr<GridType> gridPtr = GetGridPtr<TreeType>(gridName);
		if (gridPtr.IsValid())
		{
			metaValuePtr = TSharedPtr<openvdb::TypedMetadata<MetadataType>>(gridPtr->getMetadata<openvdb::TypedMetadata<MetadataType>>(TCHAR_TO_UTF8(*gridName)).get());
		}
		return metaValuePtr;
	}

	template<typename TreeType, typename MetadataType>
	void InsertGridMeta(const FString &gridName, const FString &metaName, const MetadataType &metaValue)
	{
		TSharedPtr<GridType> gridPtr = GetGridPtr<TreeType>(gridName);
		if (gridPtr.IsValid())
		{
			gridPtr->insertMeta<openvdb::TypedMetadata<MetadataType>>(TCHAR_TO_UTF8(*metaName), openvdb::TypedMetadata<MetadataType>(metaValue));
		}
	}

	template<typename TreeType, typename MetadataType>
	void RemoveGridMeta(const FString &gridName, const FString &metaName)
	{
		TSharedPtr<GridType> gridPtr = GetGridPtr<TreeType>(gridName);
		if (gridPtr.IsValid())
		{
			gridPtr->removeMeta(TCHAR_TO_UTF8(*metaName));
		}
	}

	void WriteChanges()
	{
		OpenFileGuard();
		FilePtr->write(*GridsPtr, *FileMetaPtr);
	}

	void WriteChangesAsync()
	{
		OpenFileGuard();
		AsyncIONotifier notifier;
		openvdb::io::Queue queue;
		queue.addNotifier(boost::bind(&AsyncIONotifier::callback, &notifier, _1, _2));
		queue.write<openvdb::GridPtrVec>(*GridsPtr, *FilePtr, *FileMetaPtr);
	}

	template<typename TreeType, typename... MetadataTypes>
	void MeshRegion(const FString &gridName, float surfaceValue, TArray<FVector> &vertexBuffer, TArray<int32> &polygonBuffer, TArray<FVector> &normalBuffer)
	{
		TSharedPtr<GridType> gridPtr = GetGridPtr<TreeType>(gridName);
		if (gridPtr.IsValid())
		{
			TSharedPtr<MesherOpType> mesherOpPtr = TSharedPtr<MesherOpType>(new MesherOpType(gridPtr, vertexBuffer, polygonBuffer, normalBuffer));
			mesherOpPtr->doActivateValuesOp(surfaceValue);
			mesherOpPtr->doMeshOp(true);
		}
	}

	template<typename TreeType, typename... MetadataTypes>
	void ReadGridIndexBounds(const FString &gridName, FIntVector &indexStart, FIntVector &indexEnd)
	{
		TSharedPtr<GridType> gridPtr = GetGridPtr<TreeType>(gridName);
		if (gridPtr.IsValid())
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

	template<typename TreeType, typename... MetadataTypes>
	void FillGrid_PerlinDensity(const FString &gridName, float frequency, float lacunarity, float persistence, int32 octaveCount)
	{
		TSharedPtr<GridType> gridPtr = GetGridPtr<TreeType>(gridName);
		if (gridPtr.IsValid())
		{
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
				const openvdb::CoordBBox gridBBox = gridPtr->evalActiveVoxelBoundingBox();
				check(!gridBBox.empty());
				openvdb::CoordBBox bboxPadded = gridBBox;
				bboxPadded.expand(1);

				//Create a mask enclosing the region such that the outer edge voxels are on but false
				openvdb::BoolGrid::Ptr mask = openvdb::BoolGrid::create(false);
				mask->fill(bboxPadded, /*value*/false, /*state*/true);
				mask->fill(gridBBox, /*value*/true, /*state*/true);
				Vdb::GridOps::PerlinNoiseFillOp<TreeType> noiseFillOp(gridPtr->transform(), frequency, lacunarity, persistence, octaveCount);
				openvdb::tools::transformValues(mask->cbeginValueOn(), *gridPtr, noiseFillOp);
			}
		}
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
		if (FilePtr.IsValid() && !FilePtr->isOpen())
		{
			FilePtr->open(VDBHandle->EnableDelayLoad);
		}
	}

	void CloseFileGuard()
	{
		if (FilePtr.IsValid() && FilePtr->isOpen())
		{
			FilePtr->close();
		}
	}
};

typedef openvdb::FloatTree TreeType;
typedef VDBHandlePrivate<TreeType, Vdb::Metadata::RegionMetadata> VDBHandlePrivateType;
typedef TMap<FString, TSharedPtr<VDBHandlePrivateType>> VDBRegistryType;