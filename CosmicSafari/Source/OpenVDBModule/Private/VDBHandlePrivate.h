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
	typedef typename openvdb::Grid<TreeType> GridType;
	typedef typename GridType::TreeType GridTreeType;
	typedef typename GridType::Ptr GridTypePtr;
	typedef typename GridType::ConstPtr GridTypeCPtr;
	typedef typename openvdb::Grid<IndexTreeType> IndexGridType;
	typedef typename IndexGridType::TreeType IndexTreeType;
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

	VdbHandlePrivate(const FString &filePath, const bool &enableGridStats, const bool &enableDelayLoad)
		: isFileInSync(false), FilePath(filePath), EnableGridStats(enableGridStats), EnableDelayLoad(enableDelayLoad)
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

	void InitGrids()
	{
		//Initialize OpenVDB, our metadata types, and the vdb file
		openvdb::initialize();
		if (!IndexGridType::isRegistered())
		{
			IndexGridType::registerGrid();
		}
		InitializeMetadata<MetadataTypeA>(); //TODO: Allow multiple metadata types - for now use just this one (grrr variadic templates)

		if (FilePtr.IsValid())
		{
			WriteChanges();
		}
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
		GridsPtr = FilePtr->readAllGridMetadata(); //Initially just read metadata but not tree data values
		FileMetaPtr = FilePtr->getMetadata(); //Read file-level metadata
		
		for (auto i = GridsPtr->begin(); i != GridsPtr->end(); ++i)
		{
			InitMeshSection(openvdb::gridPtrCast<GridType>(*i));
		}

		//Start in a clean state
		SetIsFileInSync(true);
	}

	GridTypePtr GetGridPtrChecked(const FString &gridName)
	{
		GridTypePtr gridPtr = GetGridPtr(gridName);
		check(gridPtr != nullptr);
		return gridPtr;
	}

	void AddGrid(const FString &gridName, const FIntVector &indexStart, const FIntVector &indexEnd, const FVector &voxelSize)
	{
		GridTypePtr gridPtr = GetGridPtr(gridName);		
		if (gridPtr == nullptr)
		{
			gridPtr = CreateGrid(gridName, indexStart, indexEnd, voxelSize);
		}
		
		const openvdb::Vec3d start(indexStart.X, indexStart.Y, indexStart.Z);
		const openvdb::Vec3d end(indexEnd.X, indexEnd.Y, indexEnd.Z);
		const openvdb::Vec3d voxelScale(voxelSize.X, voxelSize.Y, voxelSize.Z);
		openvdb::math::ScaleMap::Ptr scale = openvdb::math::ScaleMap::Ptr(new openvdb::math::ScaleMap(voxelScale));
		openvdb::math::ScaleTranslateMap::Ptr map(new openvdb::math::ScaleTranslateMap(voxelScale, scale->applyMap(start)));
		openvdb::math::Transform::Ptr xformPtr = openvdb::math::Transform::Ptr(new openvdb::math::Transform(map));
		if (gridPtr->transform() != *xformPtr)
		{
			gridPtr->setTransform(xformPtr);
			SetIsFileInSync(false);
		}

		const openvdb::Vec3IMetadata::Ptr currentIndexStartMeta = gridPtr->getMetadata<openvdb::Vec3IMetadata>(TCHAR_TO_UTF8(*MetaName_RegionIndexStart()));
		const openvdb::Vec3i indexStartVec(indexStart.X, indexStart.Y, indexStart.Z);
		if (currentIndexStartMeta == nullptr || !openvdb::math::isExactlyEqual(currentIndexStartMeta->value(), indexStartVec))
		{
			gridPtr->insertMeta(TCHAR_TO_UTF8(*MetaName_RegionIndexStart()), openvdb::Vec3IMetadata(indexStartVec));
			gridPtr->insertMeta(TCHAR_TO_UTF8(*MetaName_RegionStart()), openvdb::Vec3DMetadata(map->applyMap(openvdb::Vec3d(indexStartVec.x(), indexStartVec.y(), indexStartVec.z()))));
			SetIsFileInSync(false);
		}

		const openvdb::Vec3IMetadata::Ptr currentIndexEndMeta = gridPtr->getMetadata<openvdb::Vec3IMetadata>(TCHAR_TO_UTF8(*MetaName_RegionIndexEnd()));
		const openvdb::Vec3i indexEndVec(indexEnd.X, indexEnd.Y, indexEnd.Z);
		if (currentIndexEndMeta == nullptr || !openvdb::math::isExactlyEqual(currentIndexEndMeta->value(), indexEndVec))
		{
			gridPtr->insertMeta(TCHAR_TO_UTF8(*MetaName_RegionIndexEnd()), openvdb::Vec3IMetadata(indexEndVec));
			gridPtr->insertMeta(TCHAR_TO_UTF8(*MetaName_RegionEnd()), openvdb::Vec3DMetadata(map->applyMap(openvdb::Vec3d(indexEndVec.x(), indexEndVec.y(), indexEndVec.z()))));
			SetIsFileInSync(false);
		}
	}

	GridTypePtr ReadGridTree(const FString &gridName, FIntVector &boundsStart, FIntVector &boundsEnd)
	{
		GridTypePtr gridPtr = GetGridPtrChecked(gridName);
		if (gridPtr->activeVoxelCount() == 0)
		{
			if (FilePtr->hasGrid(TCHAR_TO_UTF8(*gridName))) //This grid may not have been written to file yet
			{
				OpenFileGuard();
				GridTypePtr activeGrid = openvdb::gridPtrCast<GridType>(FilePtr->readGrid(TCHAR_TO_UTF8(*gridName)));
				check(activeGrid != nullptr);
				check(activeGrid->treePtr() != nullptr);
				gridPtr->setTree(activeGrid->treePtr());
			}
		}
		const auto metaMin = gridPtr->getMetadata<openvdb::Vec3IMetadata>(TCHAR_TO_UTF8(*MetaName_RegionIndexStart()));
		const auto metaMax = gridPtr->getMetadata<openvdb::Vec3IMetadata>(TCHAR_TO_UTF8(*MetaName_RegionIndexEnd()));
		const openvdb::CoordBBox boundsBBox(openvdb::Coord(metaMin->value()), openvdb::Coord(metaMax->value()));
		boundsStart.X = boundsBBox.min().x();
		boundsStart.Y = boundsBBox.min().y();
		boundsStart.Z = boundsBBox.min().z();
		boundsEnd.X = boundsBBox.max().x();
		boundsEnd.Y = boundsBBox.max().y();
		boundsEnd.Z = boundsBBox.max().z();
		return gridPtr;
	}

	GridTypePtr ReadGridTree(const FString &gridName, FVector &boundsStart, FVector &boundsEnd)
	{
		FIntVector indexBoundsStart;
		FIntVector indexBoundsEnd;
		GridTypePtr gridPtr = ReadGridTree(gridName, indexBoundsStart, indexBoundsEnd);
		const openvdb::CoordBBoxd boundsBBoxd = gridPtr->indexToWorld(openvdb::CoordBBox(openvdb::Coord(indexBoundsStart.X, indexBoundsStart.Y, indexBoundsStart.Z), openvdb::Coord(indexBoundsEnd.X, indexBoundsEnd.Y, indexBoundsEnd.Z)));
		boundsStart.X = boundsBBoxd.min().x();
		boundsStart.Y = boundsBBoxd.min().y();
		boundsStart.Z = boundsBBoxd.min().z();
		boundsEnd.X = boundsBBoxd.max().x();
		boundsEnd.Y = boundsBBoxd.max().y();
		boundsEnd.Z = boundsBBoxd.max().z();
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
		openvdb::TypedMetadata<FileMetaType>::Ptr currentFileMeta = FileMetaPtr->getMetadata<openvdb::TypedMetadata<FileMetaType>>(TCHAR_TO_UTF8(*metaName));
		if (currentFileMeta == nullptr || currentFileMeta->value() != metaValue)
		{
			FileMetaPtr->insertMeta(TCHAR_TO_UTF8(*metaName), openvdb::TypedMetadata<FileMetaType>(metaValue));
			SetIsFileInSync(false);
		}
	}

	void RemoveFileMeta(const FString &metaName)
	{
		CloseFileGuard();
		if((*FileMetaPtr)[TCHAR_TO_UTF8(*metaName)] != nullptr)
		{
			FileMetaPtr->removeMeta(TCHAR_TO_UTF8(*metaName));
			SetIsFileInSync(false);
		}
	}

	void RemoveGridFromGridVec(const FString &gridName)
	{
		for (auto i = GridsPtr->begin(); i != GridsPtr->end(); ++i)
		{
			if (gridName == UTF8_TO_TCHAR((*i)->getName().c_str()))
			{
				IsGridSectionChanged.Remove(gridName);
				VertexSectionBuffers.Remove(gridName);
				PolygonSectionBuffers.Remove(gridName);
				NormalSectionBuffers.Remove(gridName);
				MeshOps.Remove(gridName);
				i->reset();
				GridsPtr->erase(i);
				SetIsFileInSync(false);
				return;
			}
		}
		//TODO: Log message if not found?
	}

	template<typename GridMetaType>
	TSharedPtr<openvdb::TypedMetadata<GridMetaType>> GetGridMetaValue(const FString &gridName, const FString &metaName) const
	{
		TSharedPtr<openvdb::TypedMetadata<GridMetaType>> metaDataTShared(nullptr);
		GridTypePtr gridPtr = GetGridPtrChecked(gridName);
		openvdb::TypedMetadata<GridMetaType>::Ptr metaDataPtr = gridPtr->getMetadata<openvdb::TypedMetadata<GridMetaType>>(TCHAR_TO_UTF8(*gridName));
		if (metaDataPtr != nullptr)
		{
			metaDataTShared = TSharedPtr<openvdb::TypedMetadata<GridMetaType>>(new openvdb::TypedMetadata<GridMetaType>());
			metaDataTShared->copy(*metaDataPtr);
		}
		return metaDataTShared;
	}

	template<typename GridMetaType>
	void InsertGridMeta(const FString &gridName, const FString &metaName, const GridMetaType &metaValue)
	{
		GridTypePtr gridPtr = GetGridPtrChecked(gridName);
		openvdb::TypedMetadata<GridMetaType>::Ptr currentGridMeta = gridPtr->getMetadata<openvdb::TypedMetadata<GridMetaType>>(TCHAR_TO_UTF8(*metaName));
		if (currentGridMeta == nullptr || currentGridMeta->value() != metaValue)
		{
			gridPtr->insertMeta(TCHAR_TO_UTF8(*metaName), openvdb::TypedMetadata<GridMetaType>(metaValue));
			SetIsFileInSync(false);
		}
	}

	template<typename GridMetaType>
	void RemoveGridMeta(const FString &gridName, const FString &metaName)
	{
		GridTypePtr gridPtr = GetGridPtrChecked(gridName);
		if ((*gridPtr)[TCHAR_TO_UTF8(*metaName)] != nullptr)
		{
			gridPtr->removeMeta(TCHAR_TO_UTF8(*metaName));
			SetIsFileInSync(false);
		}
	}

	void WriteChanges()
	{
		CloseFileGuard(); //openvdb::io::File must be closed in order to write
		if (!isFileInSync)
		{
			SetIsFileInSync(true);
			FilePtr->write(*GridsPtr, *FileMetaPtr);
		}
	}

	void WriteChangesAsync()
	{
		CloseFileGuard(); //openvdb::io::File must be closed in order to write
		if (!isFileInSync)
		{
			SetIsFileInSync(true);

			AsyncIONotifier notifier;
			openvdb::io::Queue queue;
			queue.addNotifier(boost::bind(&AsyncIONotifier::callback, &notifier, _1, _2));

			openvdb::GridPtrVec outGrids; //TODO: Make static (or somesuch) and add a critical section? Would need to lock the critical section in the destructor
			for (auto i = GridsPtr->begin(); i != GridsPtr->end(); ++i)
			{
				outGrids.push_back((*i)->deepCopyGrid());
			}
			queue.write(outGrids, *(FilePtr->copy()), *(FileMetaPtr->deepCopyMeta()));
		}
	}

	void MeshRegion(const FString &gridName, float surfaceValue)
	{
		GridTypePtr gridPtr = GetGridPtrChecked(gridName);
		openvdb::CoordBBox bbox = gridPtr->evalActiveVoxelBoundingBox();
		UE_LOG(LogOpenVDBModule, Display, TEXT("Pre activate values op: %s has %d active voxels with bbox %d,%d,%d %d,%d,%d"), *gridName, gridPtr->activeVoxelCount(), bbox.min().x(), bbox.min().y(), bbox.min().z(), bbox.max().x(), bbox.max().y(), bbox.max().z());
		
		TSharedPtr<Vdb::GridOps::BasicMesher<GridTreeType, IndexTreeType>> mesherOp = MeshOps.FindChecked(gridName);
		mesherOp->doActivateValuesOp(surfaceValue);
		SetIsFileInSync(false); //Assume values were changed by the extraction because otherwise we'd have to somehow compare them all
		bbox = gridPtr->evalActiveVoxelBoundingBox();
		UE_LOG(LogOpenVDBModule, Display, TEXT("Post activate values op: %s has %d active voxels with bbox %d,%d,%d %d,%d,%d"), *gridName, gridPtr->activeVoxelCount(), bbox.min().x(), bbox.min().y(), bbox.min().z(), bbox.max().x(), bbox.max().y(), bbox.max().z());

		mesherOp->doMeshOp();
		bbox = gridPtr->evalActiveVoxelBoundingBox();
		UE_LOG(LogOpenVDBModule, Display, TEXT("Post do mesh op: %s has %d active voxels with bbox %d,%d,%d %d,%d,%d"), *gridName, gridPtr->activeVoxelCount(), bbox.min().x(), bbox.min().y(), bbox.min().z(), bbox.max().x(), bbox.max().y(), bbox.max().z());
	}

	void FillGrid_PerlinDensity(const FString &gridName, const FIntVector &fillIndexStart, const FIntVector &fillIndexEnd, int32 seed, float frequency, float lacunarity, float persistence, int32 octaveCount, FIntVector &activeStart, FIntVector &activeEnd)
	{
		GridTypePtr gridPtr = GetGridPtrChecked(gridName);
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
			SetIsFileInSync(false);

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

			//NOTE: This appears to make the iters only step into voxel values, but needs more investigation
			//    Vdb::GridOps::PerlinNoiseFillOp<GridTreeType>::IterType beginIter = mask->cbeginValueOn();
			//    beginIter.setMinDepth(Vdb::GridOps::PerlinNoiseFillOp<GridTreeType>::IterType::ROOT_LEVEL);
			//    Vdb::GridOps::PerlinNoiseFillOp<GridTreeType>::transformValues(beginIter, *gridPtr, noiseFillOp);

			Vdb::GridOps::PerlinNoiseFillOp<GridTreeType> noiseFillOp(gridPtr->transformPtr(), seed, frequency, lacunarity, persistence, octaveCount);
			Vdb::GridOps::PerlinNoiseFillOp<GridTreeType>::transformValues(mask->cbeginValueOn(), *gridPtr, noiseFillOp);
			maskBBox = mask->evalActiveVoxelBoundingBox();
			UE_LOG(LogOpenVDBModule, Display, TEXT("Post noise fill op Mask bbox: %d,%d,%d  %d,%d,%d"), maskBBox.min().x(), maskBBox.min().y(), maskBBox.min().z(), maskBBox.max().x(), maskBBox.max().y(), maskBBox.max().z());
			UE_LOG(LogOpenVDBModule, Display, TEXT("Post noise-fill op: %s has %d active voxels"), *gridName, gridPtr->activeVoxelCount());
			UE_LOG(LogOpenVDBModule, Display, TEXT("Post noise-fill op: mask has %d active voxels"), mask->activeVoxelCount());
			openvdb::tools::pruneTiles<GridTreeType>(gridPtr->tree());
			openvdb::tools::pruneInactive<GridTreeType>(gridPtr->tree());
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

	void GetAllGridIDs(TArray<FString> &OutGridIDs)
	{
		for (auto i = GridsPtr->begin(); i != GridsPtr->end(); ++i)
		{
			OutGridIDs.Add(FString::Printf(TEXT("%s"), UTF8_TO_TCHAR((*i)->getName().c_str())));
		}
	}

	void GetGridSectionBuffers(const FString &gridName,
							   TSharedPtr<TArray<FVector>> &OutVertexBufferPtr,
							   TSharedPtr<TArray<int32>> &OutPolygonBufferPtr,
							   TSharedPtr<TArray<FVector>> &OutNormalBufferPtr,
							   TSharedPtr<TArray<FVector2D>> &OutUVMapBufferPtr,
							   TSharedPtr<TArray<FColor>> &OutVertexColorsBufferPtr,
							   TSharedPtr<TArray<FProcMeshTangent>> &OutTangentsBufferPtr)
	{
		check(VertexSectionBuffers.Contains(gridName));
		check(PolygonSectionBuffers.Contains(gridName));
		check(NormalSectionBuffers.Contains(gridName));
		check(UVMapSectionBuffers.Contains(gridName));
		check(VertexColorsSectionBuffers.Contains(gridName));
		check(TangentsSectionBuffers.Contains(gridName));
		OutVertexBufferPtr = TSharedPtr<TArray<FVector>>(VertexSectionBuffers[gridName]);
		OutPolygonBufferPtr = TSharedPtr<TArray<int32>>(PolygonSectionBuffers[gridName]);
		OutNormalBufferPtr = TSharedPtr<TArray<FVector>>(NormalSectionBuffers[gridName]);
		OutUVMapBufferPtr = TSharedPtr<TArray<FVector2D>>(UVMapSectionBuffers[gridName]);
		OutVertexColorsBufferPtr = TSharedPtr<TArray<FColor>>(VertexColorsSectionBuffers[gridName]);
		OutTangentsBufferPtr = TSharedPtr<TArray<FProcMeshTangent>>(TangentsSectionBuffers[gridName]);
	}

private:
	bool isFileInSync;
	TSharedPtr<openvdb::io::File> FilePtr;
	openvdb::GridPtrVecPtr GridsPtr;
	openvdb::MetaMap::Ptr FileMetaPtr;
	TMap<FString, bool> IsGridSectionChanged;
	TMap<FString, TSharedRef<TArray<FVector>>> VertexSectionBuffers;
	TMap<FString, TSharedRef<TArray<int32>>> PolygonSectionBuffers;
	TMap<FString, TSharedRef<TArray<FVector>>> NormalSectionBuffers;
	TMap<FString, TSharedRef<TArray<FVector2D>>> UVMapSectionBuffers;
	TMap<FString, TSharedRef<TArray<FColor>>> VertexColorsSectionBuffers;
	TMap<FString, TSharedRef<TArray<FProcMeshTangent>>> TangentsSectionBuffers;
	TMap<FString, TSharedRef<Vdb::GridOps::BasicMesher<GridTreeType, IndexTreeType>>> MeshOps;

	void SetIsFileInSync(bool isInSync)
	{
		isFileInSync = isInSync;
	}

	GridTypePtr GetGridPtr(const FString &gridName)
	{
		GridTypePtr gridPtr = nullptr;
		auto i = GridsPtr->begin();
		for (; i != GridsPtr->end(); ++i)
		{
			if (gridName == UTF8_TO_TCHAR((*i)->getName().c_str()))
			{
				gridPtr = openvdb::gridPtrCast<GridType>(*i);
				check(gridPtr != nullptr);
				break;
			}
		}
		return gridPtr;
	}

	void InitMeshSection(GridTypePtr gridPtr)
	{
		const FString gridName = UTF8_TO_TCHAR(gridPtr->getName().c_str());
		IsGridSectionChanged.Emplace(gridName, false);
		auto VertexBufferRef = VertexSectionBuffers.Emplace(gridName, TSharedRef<TArray<FVector>>(new TArray<FVector>()));
		auto PolygonBufferRef = PolygonSectionBuffers.Emplace(gridName, TSharedRef<TArray<int32>>(new TArray<int32>()));
		auto NormalBufferRef = NormalSectionBuffers.Emplace(gridName, TSharedRef<TArray<FVector>>(new TArray<FVector>()));
		auto UVMapBufferRef = UVMapSectionBuffers.Emplace(gridName, TSharedRef<TArray<FVector2D>>(new TArray<FVector2D>()));
		auto VertexColorsBufferRef = VertexColorsSectionBuffers.Emplace(gridName, TSharedRef<TArray<FColor>>(new TArray<FColor>()));
		auto TangentsBufferRef = TangentsSectionBuffers.Emplace(gridName, TSharedRef<TArray<FProcMeshTangent>>(new TArray<FProcMeshTangent>()));
		MeshOps.Emplace(gridName, TSharedRef<Vdb::GridOps::BasicMesher<GridTreeType, IndexTreeType>>(new Vdb::GridOps::BasicMesher<GridTreeType, IndexTreeType>(gridPtr, VertexBufferRef.Get(), PolygonBufferRef.Get(), NormalBufferRef.Get())));
	}

	GridTypePtr CreateGrid(const FString &gridName, const FIntVector &indexStart, const FIntVector &indexEnd, const FVector &voxelSize)
	{
		GridTypePtr gridPtr = GridType::create();
		gridPtr->setName(TCHAR_TO_UTF8(*gridName));
		InitMeshSection(gridPtr);
		GridsPtr->push_back(gridPtr);
		SetIsFileInSync(false);
		return gridPtr;
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
};

typedef openvdb::FloatTree TreeType;
typedef VdbHandlePrivate<TreeType, Vdb::GridOps::IndexTreeType, openvdb::math::ScaleMap> VdbHandlePrivateType;
typedef TMap<FString, TSharedPtr<VdbHandlePrivateType>> VdbRegistryType;