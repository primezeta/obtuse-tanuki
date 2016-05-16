#pragma once
#include "OpenVDBModule.h"
#include "VoxelData.h"
#pragma warning(push)
#pragma warning(1:4211 4800 4503 4146)
#include <openvdb/openvdb.h>
#include <openvdb/io/Queue.h>
#include <openvdb/tools/Prune.h>
#include "GridOps.h"
#include "GridMetadata.h"
#include <iostream>

DECLARE_LOG_CATEGORY_EXTERN(LogOpenVDBModule, Log, All)

//5,4,3 is the standard openvdb tree configuration
typedef openvdb::tree::Tree4<FVoxelData, 5, 4, 3>::Type TreeType;

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
		if (GridType::isRegistered())
		{
			GridType::unregisterGrid();
		}
		if (IndexGridType::isRegistered())
		{
			IndexGridType::unregisterGrid();
		}
	}

	void InitGrids()
	{
		//Initialize OpenVDB, our metadata types, and the vdb file
		openvdb::initialize();
		if (!GridType::isRegistered())
		{
			GridType::registerGrid();
		}
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
		MasksPtr = openvdb::GridPtrVecPtr(new openvdb::GridPtrVec());

		for (auto i = GridsPtr->begin(); i != GridsPtr->end(); ++i)
		{
			InitMeshSection(openvdb::gridPtrCast<GridType>(*i));
		}
		CachedGrid = GridsPtr->end();

		//Start in a clean state
		SetIsFileInSync(true);
	}

	inline GridTypePtr GetGridPtrChecked(const FString &gridName) const
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

	GridTypePtr ReadGridTree(const FString &gridName, FIntVector &indexStart, FIntVector &indexEnd)
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
		const openvdb::CoordBBox indexBBox(openvdb::Coord(metaMin->value()), openvdb::Coord(metaMax->value()));
		indexStart.X = indexBBox.min().x();
		indexStart.Y = indexBBox.min().y();
		indexStart.Z = indexBBox.min().z();
		indexEnd.X = indexBBox.max().x();
		indexEnd.Y = indexBBox.max().y();
		indexEnd.Z = indexBBox.max().z();
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
		if (CachedGrid != GridsPtr->end() && gridName == UTF8_TO_TCHAR((*CachedGrid)->getName().c_str()))
		{
			//TODO: Remove mask grid
			GridsPtr->erase(CachedGrid);
			CachedGrid = GridsPtr->end();
			return;
		}
		for (auto i = GridsPtr->begin(); i != GridsPtr->end(); ++i)
		{
			if (gridName == UTF8_TO_TCHAR((*i)->getName().c_str()))
			{
				VertexSectionBuffers.Remove(gridName);
				PolygonSectionBuffers.Remove(gridName);
				NormalSectionBuffers.Remove(gridName);
				MeshOps.Remove(gridName);
				i->reset();
				//TODO: Remove mask grid
				GridsPtr->erase(i);
				CachedGrid = GridsPtr->end();
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

	void MeshRegion(const FString &gridName, FVector &worldStart, FVector &worldEnd, FVector &firstActive) const
	{
		GridTypePtr gridPtr = GetGridPtrChecked(gridName);		
		TSharedPtr<Vdb::GridOps::BasicMesher<GridTreeType>> mesherOp = MeshOps.FindChecked(gridName);

		openvdb::BBoxd activeWorldBBox;
		openvdb::Vec3d startWorldCoord;
		openvdb::Vec3d voxelSize;
		mesherOp->doMeshOp(activeWorldBBox, startWorldCoord, voxelSize);

		worldStart.X = activeWorldBBox.min().x();
		worldStart.Y = activeWorldBBox.min().y();
		worldStart.Z = activeWorldBBox.min().z();
		worldEnd.X = activeWorldBBox.max().x();
		worldEnd.Y = activeWorldBBox.max().y();
		worldEnd.Z = activeWorldBBox.max().z();
		//Set start location to the center of the voxel surface
		firstActive.X = startWorldCoord.x() + voxelSize.x()*0.5;
		firstActive.Y = startWorldCoord.y() + voxelSize.y()*0.5;
		firstActive.Z = startWorldCoord.z() + voxelSize.z();
	}

	void FillGrid_PerlinDensity(const FString &gridName, const FIntVector &fillIndexStart, const FIntVector &fillIndexEnd, int32 seed, float frequency, float lacunarity, float persistence, int32 octaveCount)
	{
		GridTypePtr gridPtr = GetGridPtrChecked(gridName);

		//Noise module parameters are at the grid-level metadata
		openvdb::Int32Metadata::Ptr seedMeta = gridPtr->getMetadata<openvdb::Int32Metadata>("seed");
		openvdb::FloatMetadata::Ptr frequencyMeta = gridPtr->getMetadata<openvdb::FloatMetadata>("frequency");
		openvdb::FloatMetadata::Ptr lacunarityMeta = gridPtr->getMetadata<openvdb::FloatMetadata>("lacunarity");
		openvdb::FloatMetadata::Ptr persistenceMeta = gridPtr->getMetadata<openvdb::FloatMetadata>("persistence");
		openvdb::Int32Metadata::Ptr octaveCountMeta = gridPtr->getMetadata<openvdb::Int32Metadata>("octaveCount");
		bool isEmpty = gridPtr->tree().empty();
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

			//NOTE: This appears to make the iters only step into voxel values, but needs more investigation
			//    Vdb::GridOps::PerlinNoiseFillOp<GridTreeType>::IterType beginIter = mask->cbeginValueOn();
			//    beginIter.setMinDepth(Vdb::GridOps::PerlinNoiseFillOp<GridTreeType>::IterType::ROOT_LEVEL);
			//    Vdb::GridOps::PerlinNoiseFillOp<GridTreeType>::transformValues(beginIter, *gridPtr, noiseFillOp);

			openvdb::BoolGrid::Ptr valuesMaskPtr;
			for (openvdb::GridPtrVec::const_iterator i = MasksPtr->begin(); i != MasksPtr->end(); ++i)
			{
				const std::string name = gridPtr->getName() + ".mask";
				if (name == (*i)->getName())
				{
					valuesMaskPtr = openvdb::gridPtrCast<openvdb::BoolGrid>(*i);
					break;
				}
			}			
			check(valuesMaskPtr != nullptr);

			//Activate mask values to define the region that is filled with values
			valuesMaskPtr->setTransform(gridPtr->transformPtr());
			if (!valuesMaskPtr->tree().empty())
			{
				valuesMaskPtr->clear();
				gridPtr->clear();
				MeshOps.FindChecked(gridName)->markChanged();
			}

			openvdb::CoordBBox fillBBox = openvdb::CoordBBox(openvdb::Coord(fillIndexStart.X, fillIndexStart.Y, fillIndexStart.Z), openvdb::Coord(fillIndexEnd.X, fillIndexEnd.Y, fillIndexEnd.Z));
			check(!fillBBox.empty());
			valuesMaskPtr->fill(fillBBox, /*value*/false, /*state*/true);
			fillBBox.expand(-1);
			check(!fillBBox.empty());
			valuesMaskPtr->fill(fillBBox, /*value*/true, /*state*/true);
			valuesMaskPtr->tree().voxelizeActiveTiles();
			//gridPtr->topologyUnion(*valuesMaskPtr);

			//openvdb transformValues requires that ops are copyable even if shared = true, so instead call only the shared op applier here (code adapted from openvdb transformValues() in ValueTransformer.h)
			typedef typename Vdb::GridOps::PerlinNoiseFillOp<openvdb::BoolTree, openvdb::BoolTree::ValueOnIter, GridTreeType> NoiseFillOpType;
			typedef typename Vdb::GridOps::ExtractSurfaceOp<GridTreeType, GridTreeType::ValueOnIter> ExtractSurfaceOpType;
			typedef typename openvdb::TreeAdapter<GridType> Adapter;
			typedef typename openvdb::tools::valxform::SharedOpTransformer<openvdb::BoolTree::ValueOnIter, Adapter::TreeType, NoiseFillOpType> NoiseFillProcessor;
			typedef typename openvdb::tools::valxform::SharedOpApplier<GridTreeType::ValueOnIter, ExtractSurfaceOpType> ExtractSurfaceProcessor;

			const bool threaded = true;

			NoiseFillOpType noiseFillOp(gridPtr, seed, frequency, lacunarity, persistence, octaveCount);
			NoiseFillProcessor NoiseFillProc(valuesMaskPtr->beginValueOn(), Adapter::tree(*gridPtr), noiseFillOp, openvdb::MERGE_ACTIVE_STATES);
			UE_LOG(LogOpenVDBModule, Display, TEXT("%s"), *FString::Printf(TEXT("%s (pre perlin op) %d active voxels"), UTF8_TO_TCHAR(valuesMaskPtr->getName().c_str()), valuesMaskPtr->activeVoxelCount()));
			UE_LOG(LogOpenVDBModule, Display, TEXT("%s"), *FString::Printf(TEXT("%s (pre perlin op) %d active voxels"), UTF8_TO_TCHAR(gridPtr->getName().c_str()), gridPtr->activeVoxelCount()));
			NoiseFillProc.process(threaded);
			UE_LOG(LogOpenVDBModule, Display, TEXT("%s"), *FString::Printf(TEXT("%s (post perlin op) %d active voxels"), UTF8_TO_TCHAR(valuesMaskPtr->getName().c_str()), valuesMaskPtr->activeVoxelCount()));
			UE_LOG(LogOpenVDBModule, Display, TEXT("%s"), *FString::Printf(TEXT("%s (post perlin op) %d active voxels"), UTF8_TO_TCHAR(gridPtr->getName().c_str()), gridPtr->activeVoxelCount()));

			ExtractSurfaceOpType extractSurfaceOp(gridPtr);
			ExtractSurfaceProcessor ExtractSurfaceProc(gridPtr->beginValueOn(), extractSurfaceOp);
			UE_LOG(LogOpenVDBModule, Display, TEXT("%s"), *FString::Printf(TEXT("%s (pre surface op) %d active voxels"), UTF8_TO_TCHAR(gridPtr->getName().c_str()), gridPtr->activeVoxelCount()));
			ExtractSurfaceProc.process(threaded);
			UE_LOG(LogOpenVDBModule, Display, TEXT("%s"), *FString::Printf(TEXT("%s (post surface op) %d active voxels"), UTF8_TO_TCHAR(gridPtr->getName().c_str()), gridPtr->activeVoxelCount()));
		}
	}

	void GetAllGridIDs(TArray<FString> &OutGridIDs) const
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

	void GetIndexCoord(const FString &gridName, const FVector &location, FIntVector &outIndexCoord) const
	{
		GridTypePtr gridPtr = GetGridPtrChecked(gridName);
		const openvdb::Coord coord = openvdb::Coord::floor(gridPtr->worldToIndex(openvdb::Vec3d(location.X, location.Y, location.Z)));
		outIndexCoord.X = coord.x();
		outIndexCoord.Y = coord.y();
		outIndexCoord.Z = coord.z();
	}

	void GetVoxelValue(const FString &gridName, const FIntVector &indexCoord, typename GridType::ValueType &outValue) const
	{
		GridTypePtr gridPtr = GetGridPtrChecked(gridName);
		const openvdb::Coord coord(indexCoord.X, indexCoord.Y, indexCoord.Z);
		GridType::ConstAccessor cacc = gridPtr->getConstAccessor();
		outValue = cacc.getValue(coord);
	}

	bool GetVoxelActiveState(const FString &gridName, const FIntVector &indexCoord) const
	{
		GridTypePtr gridPtr = GetGridPtrChecked(gridName);
		const openvdb::Coord coord(indexCoord.X, indexCoord.Y, indexCoord.Z);
		GridType::ConstAccessor cacc = gridPtr->getConstAccessor();
		return cacc.isValueOn(coord);
	}

	bool GetVoxelValueAndActiveState(const FString &gridName, const FIntVector &indexCoord, typename GridType::ValueType &outValue) const
	{
		GridTypePtr gridPtr = GetGridPtrChecked(gridName);
		const openvdb::Coord coord(indexCoord.X, indexCoord.Y, indexCoord.Z);
		GridType::ConstAccessor cacc = gridPtr->getConstAccessor();
		outValue = cacc.getValue(coord);
		return cacc.isValueOn(coord);
	}

	void SetVoxelValue(const FString &gridName, const FIntVector &indexCoord, const typename GridType::ValueType &value)
	{
		GridTypePtr gridPtr = GetGridPtrChecked(gridName);
		const openvdb::Coord coord(indexCoord.X, indexCoord.Y, indexCoord.Z);
		GridType::Accessor acc = gridPtr->getAccessor();
		acc.modifyValueAndActiveState<Vdb::GridOps::BasicModifyOp<typename GridType::ValueType>>(coord, Vdb::GridOps::BasicModifyOp<typename GridType::ValueType>>(value));
		MeshOps.FindChecked(gridName)->markChanged();
	}

	void SetVoxelActiveState(const FString &gridName, const FIntVector &indexCoord, const bool &isActive)
	{
		GridTypePtr gridPtr = GetGridPtrChecked(gridName);
		const openvdb::Coord coord(indexCoord.X, indexCoord.Y, indexCoord.Z);
		GridType::Accessor acc = gridPtr->getAccessor();
		acc.setActiveState(coord, isActive);
		MeshOps.FindChecked(gridName)->markChanged();
	}

	void SetVoxelValueAndActiveState(const FString &gridName, const FIntVector &indexCoord, const typename GridType::ValueType &value, const bool &isActive)
	{
		GridTypePtr gridPtr = GetGridPtrChecked(gridName);
		const openvdb::Coord coord(indexCoord.X, indexCoord.Y, indexCoord.Z);
		GridType::Accessor acc = gridPtr->getAccessor();
		acc.modifyValueAndActiveState<Vdb::GridOps::BasicModifyActiveOp<typename GridType::ValueType>>(coord, Vdb::GridOps::BasicModifyActiveOp<typename GridType::ValueType>>(value, isActive));
		MeshOps.FindChecked(gridName)->markChanged();
	}

private:
	bool isFileInSync;
	TSharedPtr<openvdb::io::File> FilePtr;
	openvdb::GridPtrVecPtr GridsPtr;
	openvdb::GridPtrVecPtr MasksPtr;
	openvdb::MetaMap::Ptr FileMetaPtr;
	mutable openvdb::GridPtrVec::iterator CachedGrid;
	TMap<FString, TSharedRef<TArray<FVector>>> VertexSectionBuffers;
	TMap<FString, TSharedRef<TArray<int32>>> PolygonSectionBuffers;
	TMap<FString, TSharedRef<TArray<FVector>>> NormalSectionBuffers;
	TMap<FString, TSharedRef<TArray<FVector2D>>> UVMapSectionBuffers;
	TMap<FString, TSharedRef<TArray<FColor>>> VertexColorsSectionBuffers;
	TMap<FString, TSharedRef<TArray<FProcMeshTangent>>> TangentsSectionBuffers;
	TMap<FString, TSharedRef<Vdb::GridOps::BasicMesher<GridTreeType>>> MeshOps;

	inline void SetIsFileInSync(bool isInSync)
	{
		isFileInSync = isInSync;
	}

	inline GridTypePtr GetGridPtr(const FString &gridName) const
	{
		if (CachedGrid != GridsPtr->end() && gridName == UTF8_TO_TCHAR((*CachedGrid)->getName().c_str()))
		{
			return openvdb::gridPtrCast<GridType>(*CachedGrid);
		}
		GridTypePtr gridPtr = nullptr;
		CachedGrid = GridsPtr->begin();
		for (; CachedGrid != GridsPtr->end(); ++CachedGrid)
		{
			if (gridName == UTF8_TO_TCHAR((*CachedGrid)->getName().c_str()))
			{
				gridPtr = openvdb::gridPtrCast<GridType>(*CachedGrid);
				check(gridPtr != nullptr);
				break;
			}
		}
		return gridPtr;
	}

	void InitMeshSection(GridTypePtr gridPtr)
	{
		const FString gridName = UTF8_TO_TCHAR(gridPtr->getName().c_str());
		auto VertexBufferRef = VertexSectionBuffers.Emplace(gridName, TSharedRef<TArray<FVector>>(new TArray<FVector>()));
		auto PolygonBufferRef = PolygonSectionBuffers.Emplace(gridName, TSharedRef<TArray<int32>>(new TArray<int32>()));
		auto NormalBufferRef = NormalSectionBuffers.Emplace(gridName, TSharedRef<TArray<FVector>>(new TArray<FVector>()));
		auto UVMapBufferRef = UVMapSectionBuffers.Emplace(gridName, TSharedRef<TArray<FVector2D>>(new TArray<FVector2D>()));
		auto VertexColorsBufferRef = VertexColorsSectionBuffers.Emplace(gridName, TSharedRef<TArray<FColor>>(new TArray<FColor>()));
		auto TangentsBufferRef = TangentsSectionBuffers.Emplace(gridName, TSharedRef<TArray<FProcMeshTangent>>(new TArray<FProcMeshTangent>()));
		MeshOps.Emplace(gridName, TSharedRef<Vdb::GridOps::BasicMesher<GridTreeType>>(new Vdb::GridOps::BasicMesher<GridTreeType>(gridPtr, VertexBufferRef.Get(), PolygonBufferRef.Get(), NormalBufferRef.Get(), UVMapBufferRef.Get(), VertexColorsBufferRef.Get(), TangentsBufferRef.Get())));
		openvdb::BoolGrid::Ptr valuesMask = openvdb::BoolGrid::create(false);
		valuesMask->setName(gridPtr->getName() + ".mask");
		MasksPtr->push_back(valuesMask);
	}

	GridTypePtr CreateGrid(const FString &gridName, const FIntVector &indexStart, const FIntVector &indexEnd, const FVector &voxelSize)
	{
		GridTypePtr gridPtr = GridType::create();
		gridPtr->setName(TCHAR_TO_UTF8(*gridName));
		InitMeshSection(gridPtr);
		GridsPtr->push_back(gridPtr);
		CachedGrid = GridsPtr->end();
		SetIsFileInSync(false);
		return gridPtr;
	}

	template<typename MetadataType>
	inline void InitializeMetadata() const
	{
		if (!openvdb::TypedMetadata<MetadataType>::isRegisteredType())
		{
			openvdb::TypedMetadata<MetadataType>::registerType();
		}
	}

	inline void OpenFileGuard()
	{
		if (FilePtr.IsValid() && !FilePtr->isOpen())
		{
			FilePtr->open(EnableDelayLoad);
		}
	}

	inline void CloseFileGuard()
	{
		if (FilePtr.IsValid() && FilePtr->isOpen())
		{
			FilePtr->close();
		}
	}
};

typedef VdbHandlePrivate<TreeType, Vdb::GridOps::IndexTreeType, openvdb::math::ScaleMap> VdbHandlePrivateType;

//The following non-class member operators are required by openvdb
template<> OPENVDBMODULE_API inline FVoxelData openvdb::zeroVal<FVoxelData>();
OPENVDBMODULE_API std::ostream& operator<<(std::ostream& os, const FVoxelData& voxelData);
OPENVDBMODULE_API FVoxelData operator+(const FVoxelData &lhs, const float &rhs);
OPENVDBMODULE_API FVoxelData operator+(const FVoxelData &lhs, const FVoxelData &rhs);
OPENVDBMODULE_API FVoxelData operator-(const FVoxelData &lhs, const FVoxelData &rhs);
OPENVDBMODULE_API bool operator<(const FVoxelData &lhs, const FVoxelData &rhs);
OPENVDBMODULE_API bool operator>(const FVoxelData &lhs, const FVoxelData &rhs);
OPENVDBMODULE_API bool operator==(const FVoxelData &lhs, const FVoxelData &rhs);
OPENVDBMODULE_API inline FVoxelData Abs(const FVoxelData &voxelData);
OPENVDBMODULE_API FVoxelData operator-(const FVoxelData &voxelData);