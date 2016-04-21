#pragma once
#include "OpenVDBModule.h"
#pragma warning(push)
#pragma warning(1:4211 4800 4503 4146)
#include <openvdb/openvdb.h>
#include <openvdb/io/Queue.h>
#include <openvdb/tools/Prune.h>
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

template<typename TreeType, typename IndexTreeType, typename MetadataTypeA>
class VdbHandlePrivate
{
public:
	typedef openvdb::Grid<TreeType> GridType;
	typedef typename GridType::Ptr GridTypePtr;
	typedef typename GridType::ConstPtr GridTypeCPtr;
	typedef openvdb::Grid<IndexTreeType> IndexGridType;
	typedef typename IndexGridType::Ptr IndexGridTypePtr;
	typedef typename IndexGridType::ConstPtr IndexGridTypeCPtr;

	const FString FilePath;
	const bool EnableGridStats;
	const bool EnableDelayLoad;
	
	static FString MetaName_WorldName() { return TEXT("WorldName"); }
	static FString MetaName_RegionScale() { return TEXT("RegionScale"); }
	static FString MetaName_RegionStart() { return TEXT("RegionStart"); }
	static FString MetaName_RegionEnd() { return TEXT("RegionEnd"); }
	static FString MetaName_RegionIndexStart() { return TEXT("RegionIndexStart"); }
	static FString MetaName_RegionIndexEnd() { return TEXT("RegionIndexEnd"); }

	VdbHandlePrivate(const FString &filePath, const bool &enableGridStats, const bool &enableDelayLoad, TArray<TArray<FVector>> *vertexBuffers, TArray<TArray<int32>> *polygonBuffers, TArray<TArray<FVector>> *normalBuffers)
		: FilePath(filePath), EnableGridStats(enableGridStats), EnableDelayLoad(enableDelayLoad), VertexSectionBuffer(vertexBuffers), PolygonSectionBuffer(polygonBuffers), NormalSectionBuffer(normalBuffers)
	{
	}

	~VdbHandlePrivate()
	{
		WriteChanges();
		openvdb::uninitialize();
		if (IndexGridType::isRegistered())
		{
			IndexGridType::unregisterGrid();
		}
	}

	void InitBuffers(TArray<TArray<FVector>> *vertexBuffers, TArray<TArray<int32>> *polygonBuffers, TArray<TArray<FVector>> *normalBuffers)
	{
		VertexSectionBuffer = vertexBuffers;
		PolygonSectionBuffer = polygonBuffers;
		NormalSectionBuffer = normalBuffers;
	}

	void Init()
	{
		//Initialize OpenVDB, our metadata types, and the vdb file
		openvdb::initialize();
		if (!IndexGridType::isRegistered())
		{
			IndexGridType::registerGrid();
		}
		InitializeMetadata<MetadataTypeA>();

		CloseFileGuard();
		FilePtr = TSharedPtr<openvdb::io::File>(new openvdb::io::File(TCHAR_TO_UTF8(*(FilePath))));
		check(FilePtr.IsValid());
		FilePtr->setGridStatsMetadataEnabled(EnableGridStats);

		if (!FPaths::FileExists(FilePath))
		{
			//Create an empty vdb file
			FilePtr->write(openvdb::GridCPtrVec(), openvdb::MetaMap());
			UE_LOG(LogOpenVDBModule, Verbose, TEXT("IVdb: Created %s"), *(FilePath));
		}
		check(FPaths::FileExists(FilePath)); //TODO: Error handling when unable to create file. For now assume the file exists

		OpenFileGuard();
		if (GridsPtr == nullptr)
		{
			GridsPtr = FilePtr->readAllGridMetadata();
		}
		if (FileMetaPtr == nullptr)
		{
			FileMetaPtr = FilePtr->getMetadata();
		}
		
		int32 sectionIndex = 0;
		for (auto i = GridsPtr->begin(); i != GridsPtr->end(); ++i)
		{
			const FString gridName = UTF8_TO_TCHAR((*i)->getName().c_str());
			if (MeshOps.Contains(gridName))
			{
				MeshOps.Remove(gridName);
			}

			if (VertexSectionBuffer->Num() > sectionIndex)
			{
				(*VertexSectionBuffer)[sectionIndex] = TArray<FVector>();
			}
			else
			{
				VertexSectionBuffer->Add(TArray<FVector>());
			}
			if (PolygonSectionBuffer->Num() > sectionIndex)
			{
				(*PolygonSectionBuffer)[sectionIndex] = TArray<int32>();
			}
			else
			{
				PolygonSectionBuffer->Add(TArray<int32>());
			}
			if (NormalSectionBuffer->Num() > sectionIndex)
			{
				(*NormalSectionBuffer)[sectionIndex] = TArray<FVector>();
			}
			else
			{
				NormalSectionBuffer->Add(TArray<FVector>());
			}
			GridTypePtr gridPtr = openvdb::gridPtrCast<GridType>(*i);
			check(gridPtr != nullptr);
			MeshOps.Add(gridName, TSharedPtr<Vdb::GridOps::BasicMesher<TreeType, IndexTreeType>>(new Vdb::GridOps::BasicMesher<TreeType, IndexTreeType>(gridPtr, (*VertexSectionBuffer)[sectionIndex], (*PolygonSectionBuffer)[sectionIndex], (*NormalSectionBuffer)[sectionIndex])));
			sectionIndex++;
		}
	}

	template<typename TreeType>
	GridTypePtr GetGridPtr(const FString &gridName)
	{
		GridTypePtr grid = nullptr;
		auto i = GridsPtr->begin();
		for (; i != GridsPtr->end(); ++i)
		{
			if (gridName == UTF8_TO_TCHAR((*i)->getName().c_str()))
			{
				grid = openvdb::gridPtrCast<GridType>(*i);
				check(grid != nullptr);
				break;
			}
		}
		return grid;
	}

	template<typename TreeType>
	void AddGrid(const FString &gridName, const FIntVector &indexStart, const FIntVector &indexEnd, const FVector &voxelSize)
	{
		GridTypePtr gridPtr = GetGridPtr<TreeType>(gridName);		
		if (gridPtr == nullptr)
		{
			gridPtr = GridType::create();
			gridPtr->setName(TCHAR_TO_UTF8(*gridName));
			int32 sectionIndex = GridsPtr->size();
			GridsPtr->push_back(gridPtr);
			WriteChanges();

			VertexSectionBuffer->Add(TArray<FVector>());
			PolygonSectionBuffer->Add(TArray<int32>());
			NormalSectionBuffer->Add(TArray<FVector>());
			MeshOps.Add(gridName, TSharedPtr<Vdb::GridOps::BasicMesher<TreeType, IndexTreeType>>(new Vdb::GridOps::BasicMesher<TreeType, IndexTreeType>(gridPtr, (*VertexSectionBuffer)[sectionIndex], (*PolygonSectionBuffer)[sectionIndex], (*NormalSectionBuffer)[sectionIndex])));
		}
		
		const openvdb::Vec3d start(indexStart.X, indexStart.Y, indexStart.Z);
		const openvdb::Vec3d end(indexEnd.X, indexEnd.Y, indexEnd.Z);
		const openvdb::Vec3d voxelScale(voxelSize.X, voxelSize.Y, voxelSize.Z);
		openvdb::math::ScaleMap::Ptr scale = openvdb::math::ScaleMap::Ptr(new openvdb::math::ScaleMap(voxelScale));
		openvdb::math::ScaleTranslateMap::Ptr map(new openvdb::math::ScaleTranslateMap(voxelScale, scale->applyMap(start)));
		gridPtr->setTransform(openvdb::math::Transform::Ptr(new openvdb::math::Transform(map)));

		const openvdb::Vec3d worldStart = map->applyMap(start);
		const openvdb::Vec3d worldEnd = map->applyMap(end);
		gridPtr->insertMeta(TCHAR_TO_UTF8(*MetaName_RegionStart()), openvdb::Vec3DMetadata(worldStart));
		gridPtr->insertMeta(TCHAR_TO_UTF8(*MetaName_RegionEnd()), openvdb::Vec3DMetadata(worldEnd));
		gridPtr->insertMeta(TCHAR_TO_UTF8(*MetaName_RegionIndexStart()), openvdb::Vec3IMetadata(openvdb::Vec3i(indexStart.X, indexStart.Y, indexStart.Z)));
		gridPtr->insertMeta(TCHAR_TO_UTF8(*MetaName_RegionIndexEnd()), openvdb::Vec3IMetadata(openvdb::Vec3i(indexEnd.X, indexEnd.Y, indexEnd.Z)));
	}

	template<typename TreeType>
	GridTypePtr ReadGridTree(const FString &gridName, FIntVector &boundsStart, FIntVector &boundsEnd)
	{
		GridTypePtr gridPtr = GetGridPtr<TreeType>(gridName);
		if (gridPtr->activeVoxelCount() == 0)
		{
			OpenFileGuard();
			GridTypePtr activeGrid = openvdb::gridPtrCast<GridType>(FilePtr->readGrid(TCHAR_TO_UTF8(*gridName)));
			check(activeGrid != nullptr);
			check(activeGrid->treePtr() != nullptr);
			gridPtr->setTree(activeGrid->treePtr());
		}
		auto metaMin = gridPtr->getMetadata<openvdb::Vec3IMetadata>(TCHAR_TO_UTF8(*MetaName_RegionIndexStart()));
		auto metaMax = gridPtr->getMetadata<openvdb::Vec3IMetadata>(TCHAR_TO_UTF8(*MetaName_RegionIndexEnd()));
		openvdb::CoordBBox activeBBox(openvdb::Coord(metaMin->value()), openvdb::Coord(metaMax->value()));
		boundsStart.X = activeBBox.min().x();
		boundsStart.Y = activeBBox.min().y();
		boundsStart.Z = activeBBox.min().z();
		boundsEnd.X = activeBBox.max().x();
		boundsEnd.Y = activeBBox.max().y();
		boundsEnd.Z = activeBBox.max().z();
		return gridPtr;
	}

	template<typename TreeType>
	GridTypePtr ReadGridTree(const FString &gridName, FVector &activeStart, FVector &activeEnd)
	{
		GridTypePtr gridPtr = GetGridPtr<TreeType>(gridName);
		if (gridPtr->activeVoxelCount() == 0)
		{
			OpenFileGuard();
			GridTypePtr activeGrid = openvdb::gridPtrCast<GridType>(FilePtr->readGrid(TCHAR_TO_UTF8(*gridName)));
			check(activeGrid != nullptr);
			check(activeGrid->treePtr() != nullptr);
			gridPtr->setTree(activeGrid->treePtr());
		}
		//openvdb::CoordBBox activeBBox = grid->indexToWorld(grid->evalActiveVoxelBoundingBox());
		auto metaMin = gridPtr->getMetadata<openvdb::Vec3IMetadata>(TCHAR_TO_UTF8(*MetaName_RegionStart()));
		auto metaMax = gridPtr->getMetadata<openvdb::Vec3IMetadata>(TCHAR_TO_UTF8(*MetaName_RegionEnd()));
		openvdb::CoordBBox activeBBox = gridPtr->indexToWorld(openvdb::CoordBBox(openvdb::Coord(metaMin->value()), openvdb::Coord(metaMax->value())));
		activeStart.X = activeBBoxd.min().x();
		activeStart.Y = activeBBoxd.min().y();
		activeStart.Z = activeBBoxd.min().z();
		activeEnd.X = activeBBoxd.max().x();
		activeEnd.Y = activeBBoxd.max().y();
		activeEnd.Z = activeBBoxd.max().z();
		return gridPtr;
	}

	template<typename FileMetaType>
	TSharedPtr<openvdb::TypedMetadata<FileMetaType>> GetFileMetaValue(const FString &metaName)
	{
		OpenFileGuard();
		TSharedPtr<openvdb::TypedMetadata<FileMetaType>> metaDataTShared(nullptr);
		openvdb::TypedMetadata<FileMetaType>::Ptr metaDataPtr = FileMetaPtr->getMetadata<openvdb::TypedMetadata<FileMetaType>>(TCHAR_TO_UTF8(*metaName));
		if (metaDataPtr != nullptr)
		{
			metaDataTShared = TSharedPtr<openvdb::TypedMetadata<FileMetaType>>(new openvdb::TypedMetadata<FileMetaType>());
			metaDataTShared->copy(*metaDataPtr);
		}
		return metaDataTShared;
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
		for (auto i = GridsPtr->begin(); i != GridsPtr->end(); ++i)
		{
			const FString name = UTF8_TO_TCHAR((*i)->getName().c_str());
			if (gridName == name)
			{
				MeshOps.Remove(name);
				i->reset();
				GridsPtr->erase(i);
				return;
			}
		}
	}

	template<typename TreeType, typename GridMetaType>
	TSharedPtr<openvdb::TypedMetadata<GridMetaType>> GetGridMetaValue(const FString &gridName, const FString &metaName) const
	{
		TSharedPtr<openvdb::TypedMetadata<GridMetaType>> metaDataTShared(nullptr);
		GridTypePtr gridPtr = GetGridPtr<TreeType>(gridName);
		openvdb::TypedMetadata<GridMetaType>::Ptr metaDataPtr = gridPtr->getMetadata<openvdb::TypedMetadata<GridMetaType>>(TCHAR_TO_UTF8(*gridName));
		if (metaDataPtr != nullptr)
		{
			metaDataTShared = TSharedPtr<openvdb::TypedMetadata<GridMetaType>>(new openvdb::TypedMetadata<GridMetaType>());
			metaDataTShared->copy(*metaDataPtr);
		}
		return metaDataTShared;
	}

	template<typename TreeType, typename GridMetaType>
	void InsertGridMeta(const FString &gridName, const FString &metaName, const GridMetaType &metaValue)
	{
		GridTypePtr gridPtr = GetGridPtr<TreeType>(gridName);
		gridPtr->insertMeta<openvdb::TypedMetadata<GridMetaType>>(TCHAR_TO_UTF8(*metaName), openvdb::TypedMetadata<GridMetaType>(metaValue));
	}

	template<typename TreeType, typename GridMetaType>
	void RemoveGridMeta(const FString &gridName, const FString &metaName)
	{
		GridTypePtr gridPtr = GetGridPtr<TreeType>(gridName);
		gridPtr->removeMeta(TCHAR_TO_UTF8(*metaName));
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

		openvdb::GridPtrVec outGrids; //TODO: Make static and add a critical section?
		for (auto i = GridsPtr->begin(); i != GridsPtr->end(); ++i)
		{
			outGrids.push_back((*i)->deepCopyGrid());
		}
		queue.write(outGrids, *(FilePtr->copy()), *(FileMetaPtr->deepCopyMeta()));
	}

	template<typename TreeType, typename IndexTreeType>
	void MeshRegion(const FString &gridName, float surfaceValue)
	{
		GridTypePtr gridPtr = GetGridPtr<TreeType>(gridName);
		openvdb::CoordBBox bbox = gridPtr->evalActiveVoxelBoundingBox();
		UE_LOG(LogOpenVDBModule, Display, TEXT("Pre activate values op: %s has %d active voxels with bbox %d,%d,%d %d,%d,%d"), *gridName, gridPtr->activeVoxelCount(), bbox.min().x(), bbox.min().y(), bbox.min().z(), bbox.max().x(), bbox.max().y(), bbox.max().z());
		
		TSharedPtr<Vdb::GridOps::BasicMesher<TreeType, IndexTreeType>> mesherOp = MeshOps.FindChecked(gridName);
		mesherOp->doActivateValuesOp(surfaceValue);
		bbox = gridPtr->evalActiveVoxelBoundingBox();
		UE_LOG(LogOpenVDBModule, Display, TEXT("Post activate values op: %s has %d active voxels with bbox %d,%d,%d %d,%d,%d"), *gridName, gridPtr->activeVoxelCount(), bbox.min().x(), bbox.min().y(), bbox.min().z(), bbox.max().x(), bbox.max().y(), bbox.max().z());

		mesherOp->doMeshOp();
		bbox = gridPtr->evalActiveVoxelBoundingBox();
		UE_LOG(LogOpenVDBModule, Display, TEXT("Post do mesh op: %s has %d active voxels with bbox %d,%d,%d %d,%d,%d"), *gridName, gridPtr->activeVoxelCount(), bbox.min().x(), bbox.min().y(), bbox.min().z(), bbox.max().x(), bbox.max().y(), bbox.max().z());
	}

	template<typename TreeType>
	void FillGrid_PerlinDensity(const FString &gridName, const FIntVector &fillIndexStart, const FIntVector &fillIndexEnd, int32 seed, float frequency, float lacunarity, float persistence, int32 octaveCount, FIntVector &activeStart, FIntVector &activeEnd)
	{
		GridTypePtr gridPtr = GetGridPtr<TreeType>(gridName);
		const openvdb::CoordBBox fillBBox = openvdb::CoordBBox(openvdb::Coord(fillIndexStart.X, fillIndexStart.Y, fillIndexStart.Z), openvdb::Coord(fillIndexEnd.X, fillIndexEnd.Y, fillIndexEnd.Z));

		//Noise module parameters are at the grid-level metadata
		openvdb::Int32Metadata::Ptr seedMeta = gridPtr->getMetadata<openvdb::Int32Metadata>("seed");
		openvdb::FloatMetadata::Ptr frequencyMeta = gridPtr->getMetadata<openvdb::FloatMetadata>("frequency");
		openvdb::FloatMetadata::Ptr lacunarityMeta = gridPtr->getMetadata<openvdb::FloatMetadata>("lacunarity");
		openvdb::FloatMetadata::Ptr persistenceMeta = gridPtr->getMetadata<openvdb::FloatMetadata>("persistence");
		openvdb::Int32Metadata::Ptr octaveCountMeta = gridPtr->getMetadata<openvdb::Int32Metadata>("octaveCount");
		bool isEmpty = gridPtr->tree().empty();
		std::string arg;
		if (isEmpty)
		{
			arg = "empty";
		}
		else
		{
			arg = "full";
		}
		if (isEmpty ||
			seedMeta == nullptr || !openvdb::math::isExactlyEqual(seed, seedMeta->value()) ||
			frequencyMeta == nullptr || !openvdb::math::isApproxEqual(frequency, frequencyMeta->value()) ||
			lacunarityMeta == nullptr || !openvdb::math::isApproxEqual(lacunarity, lacunarityMeta->value()) ||
			persistenceMeta == nullptr || !openvdb::math::isApproxEqual(persistence, persistenceMeta->value()) ||
			octaveCountMeta == nullptr || !openvdb::math::isExactlyEqual(octaveCount, octaveCountMeta->value()))
		{
			//Update the Perlin noise parameters
			gridPtr->insertMeta("seed", openvdb::Int32Metadata(seed));
			gridPtr->insertMeta("frequency", openvdb::FloatMetadata(frequency));
			gridPtr->insertMeta("lacunarity", openvdb::FloatMetadata(lacunarity));
			gridPtr->insertMeta("persistence", openvdb::FloatMetadata(persistence));
			gridPtr->insertMeta("octaveCount", openvdb::Int32Metadata(octaveCount));

			//Activate mask values such that there is a single padded region along the outer edge with
			//values on and false and all other values within the padded region have values on and true.
			check(!fillBBox.empty());
			openvdb::CoordBBox bboxPadded = fillBBox;
			bboxPadded.expand(1);

			//Create a mask enclosing the region such that the outer edge voxels have value false
			openvdb::BoolGrid::Ptr mask = openvdb::BoolGrid::create(false);
			mask->fill(bboxPadded, /*value*/false, /*state*/true);
			mask->fill(fillBBox, /*value*/true, /*state*/true);
			UE_LOG(LogOpenVDBModule, Display, TEXT("BBox padded: %d,%d,%d  %d,%d,%d"), bboxPadded.min().x(), bboxPadded.min().y(), bboxPadded.min().z(), bboxPadded.max().x(), bboxPadded.max().y(), bboxPadded.max().z());
			UE_LOG(LogOpenVDBModule, Display, TEXT("BBox fill: %d,%d,%d  %d,%d,%d"), fillBBox.min().x(), fillBBox.min().y(), fillBBox.min().z(), fillBBox.max().x(), fillBBox.max().y(), fillBBox.max().z());
			openvdb::CoordBBox maskBBox = mask->evalActiveVoxelBoundingBox();
			UE_LOG(LogOpenVDBModule, Display, TEXT("Pre noise fill op Mask bbox: %d,%d,%d  %d,%d,%d"), maskBBox.min().x(), maskBBox.min().y(), maskBBox.min().z(), maskBBox.max().x(), maskBBox.max().y(), maskBBox.max().z());
			UE_LOG(LogOpenVDBModule, Display, TEXT("Pre noise-fill op: %s has %d active voxels"), *gridName, gridPtr->activeVoxelCount());
			UE_LOG(LogOpenVDBModule, Display, TEXT("Pre noise-fill op: mask has %d active voxels"), mask->activeVoxelCount());
			Vdb::GridOps::PerlinNoiseFillOp<TreeType> noiseFillOp(gridPtr->transformPtr(), seed, frequency, lacunarity, persistence, octaveCount);
			Vdb::GridOps::PerlinNoiseFillOp<TreeType>::transformValues(mask->cbeginValueOn(), *gridPtr, noiseFillOp);
			maskBBox = mask->evalActiveVoxelBoundingBox();
			UE_LOG(LogOpenVDBModule, Display, TEXT("Post noise fill op Mask bbox: %d,%d,%d  %d,%d,%d"), maskBBox.min().x(), maskBBox.min().y(), maskBBox.min().z(), maskBBox.max().x(), maskBBox.max().y(), maskBBox.max().z());
			UE_LOG(LogOpenVDBModule, Display, TEXT("Post noise-fill op: %s has %d active voxels"), *gridName, gridPtr->activeVoxelCount());
			UE_LOG(LogOpenVDBModule, Display, TEXT("Post noise-fill op: mask has %d active voxels"), mask->activeVoxelCount());
			openvdb::tools::pruneTiles<TreeType>(gridPtr->tree());
			openvdb::tools::pruneInactive<TreeType>(gridPtr->tree());
			UE_LOG(LogOpenVDBModule, Display, TEXT("Post noise-fill op after prune: %s has %d active voxels"), *gridName, gridPtr->activeVoxelCount());
		}
		openvdb::CoordBBox activeBBox = gridPtr->evalActiveVoxelBoundingBox();
		activeStart.X = activeBBox.min().x();
		activeStart.Y = activeBBox.min().y();
		activeStart.Z = activeBBox.min().z();
		activeEnd.X = activeBBox.max().x();
		activeEnd.Y = activeBBox.max().y();
		activeEnd.Z = activeBBox.max().z();
	}

	void GetAllGridIDs(TArray<FString> &GridIDs)
	{
		for (auto i = GridsPtr->begin(); i != GridsPtr->end(); ++i)
		{
			GridIDs.Add(FString::Printf(TEXT("%s"), UTF8_TO_TCHAR((*i)->getName().c_str())));
		}
	}

private:
	TSharedPtr<openvdb::io::File> FilePtr;
	openvdb::GridPtrVecPtr GridsPtr;
	openvdb::MetaMap::Ptr FileMetaPtr;
	TMap<FString, TSharedPtr<Vdb::GridOps::BasicMesher<TreeType, IndexTreeType>>> MeshOps;
	TArray<TArray<FVector>> *VertexSectionBuffer;
	TArray<TArray<int32>> *PolygonSectionBuffer;
	TArray<TArray<FVector>> *NormalSectionBuffer;

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
			FilePtr->open(EnableDelayLoad);
		}
	}

	void CloseFileGuard()
	{
		if (FilePtr.IsValid() && FilePtr->isOpen())
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
typedef VdbHandlePrivate<TreeType, Vdb::GridOps::IndexTreeType, openvdb::math::ScaleMap> VdbHandlePrivateType;
typedef TMap<FString, TSharedPtr<VdbHandlePrivateType>> VdbRegistryType;