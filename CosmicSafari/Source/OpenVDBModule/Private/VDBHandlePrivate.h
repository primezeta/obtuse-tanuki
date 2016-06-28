#pragma once
#include "OpenVDBModule.h"
#pragma warning(push)
#pragma warning(1:4211 4800 4503 4146)
#include <openvdb/openvdb.h>
#include <openvdb/io/Queue.h>
#include <openvdb/tools/Prune.h>
#include "GridOps.h"
#include "GridMetadata.h"

PRAGMA_DISABLE_OPTIMIZATION

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

	VdbHandlePrivate(UObject const * parent, const FString &filePath, const bool &enableGridStats, const bool &enableDelayLoad)
		: Parent(parent), isFileInSync(false), FilePath(filePath), EnableGridStats(enableGridStats), EnableDelayLoad(enableDelayLoad)
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

		//TODO: Ensure path exists before writing file
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

	void AddGrid(const FString &gridName,
		const FIntVector &indexStart,
		const FIntVector &indexEnd,
		const FVector &voxelSize,
		FGridMeshBuffers &meshBuffers)
	{
		GridTypePtr gridPtr = GetGridPtr(gridName);	
		const openvdb::Vec3d start(indexStart.X, indexStart.Y, indexStart.Z);
		const openvdb::Vec3d end(indexEnd.X, indexEnd.Y, indexEnd.Z);
		const openvdb::Vec3d voxelScale(voxelSize.X, voxelSize.Y, voxelSize.Z);
		openvdb::math::ScaleMap::Ptr map = openvdb::math::ScaleMap::Ptr(new openvdb::math::ScaleMap(voxelScale));
		openvdb::math::Transform::Ptr xformPtr = openvdb::math::Transform::Ptr(new openvdb::math::Transform(map));

		if (gridPtr == nullptr)
		{
			gridPtr = CreateGrid(gridName, indexStart, indexEnd, xformPtr, meshBuffers);
		}
		
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
			//This grid may not have been written to file yet
			if (FilePtr->hasGrid(TCHAR_TO_UTF8(*gridName)))
			{
				//It has been written to file already so swap the grid tree to the grid tree that was read from file
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
			GridsPtr->erase(CachedGrid);
			CachedGrid = GridsPtr->end();
			return;
		}
		for (auto i = GridsPtr->begin(); i != GridsPtr->end(); ++i)
		{
			if (gridName == UTF8_TO_TCHAR((*i)->getName().c_str()))
			{
				CubesMeshOps.Remove(gridName);
				MarchingCubesMeshOps.Remove(gridName);
				i->reset();
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

	bool FillGrid_PerlinDensity(const FString &gridName, bool threaded, const FIntVector &fillIndexStart, const FIntVector &fillIndexEnd, int32 seed, float frequency, float lacunarity, float persistence, int32 octaveCount)
	{
		bool isChanged = false;
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
			isChanged = true;
			SetIsFileInSync(false);

			//Update the Perlin noise parameters
			gridPtr->insertMeta("seed", openvdb::Int32Metadata(seed));
			gridPtr->insertMeta("frequency", openvdb::FloatMetadata(frequency));
			gridPtr->insertMeta("lacunarity", openvdb::FloatMetadata(lacunarity));
			gridPtr->insertMeta("persistence", openvdb::FloatMetadata(persistence));
			gridPtr->insertMeta("octaveCount", openvdb::Int32Metadata(octaveCount));

			CubesMeshOps[gridName]->markChanged();
			MarchingCubesMeshOps[gridName]->markChanged();

			typedef typename Vdb::GridOps::PerlinNoiseFillOp<GridTreeType, GridTreeType::ValueOnIter> NoiseFillOpType;
			typedef typename openvdb::tools::valxform::SharedOpApplier<GridTreeType::ValueOnIter, NoiseFillOpType> NoiseFillProcessor;
			openvdb::CoordBBox fillBBox = openvdb::CoordBBox(openvdb::Coord(fillIndexStart.X, fillIndexStart.Y, fillIndexStart.Z), openvdb::Coord(fillIndexEnd.X, fillIndexEnd.Y, fillIndexEnd.Z));
			check(!fillBBox.empty());
			NoiseFillOpType noiseFillOp(gridPtr, fillBBox, seed, frequency, lacunarity, persistence, octaveCount);
			NoiseFillProcessor NoiseFillProc(gridPtr->beginValueOn(), noiseFillOp);
			UE_LOG(LogOpenVDBModule, Display, TEXT("%s"), *FString::Printf(TEXT("%s (pre perlin op) %d active voxels"), UTF8_TO_TCHAR(gridPtr->getName().c_str()), gridPtr->activeVoxelCount()));
			NoiseFillProc.process(threaded);
			UE_LOG(LogOpenVDBModule, Display, TEXT("%s"), *FString::Printf(TEXT("%s (post perlin op) %d active voxels"), UTF8_TO_TCHAR(gridPtr->getName().c_str()), gridPtr->activeVoxelCount()));
		}
		return isChanged;
	}

	void ExtractGridSurface_Cubes(const FString &gridName, bool threaded)
	{
		typedef typename Vdb::GridOps::BasicExtractSurfaceOp<GridTreeType, GridTreeType::ValueOnIter> BasicExtractSurfaceOpType;
		typedef typename openvdb::tools::valxform::SharedOpApplier<GridTreeType::ValueOnIter, BasicExtractSurfaceOpType> BasicExtractSurfaceProcessor;
		GridTypePtr gridPtr = GetGridPtrChecked(gridName);
		BasicExtractSurfaceOpType BasicExtractSurfaceOp(gridPtr);
		BasicExtractSurfaceProcessor BasicExtractSurfaceProc(gridPtr->beginValueOn(), BasicExtractSurfaceOp);
		UE_LOG(LogOpenVDBModule, Display, TEXT("%s"), *FString::Printf(TEXT("%s (pre basic surface op) %d active voxels"), UTF8_TO_TCHAR(gridPtr->getName().c_str()), gridPtr->activeVoxelCount()));
		BasicExtractSurfaceProc.process(threaded);
		UE_LOG(LogOpenVDBModule, Display, TEXT("%s"), *FString::Printf(TEXT("%s (post basic surface op) %d active voxels"), UTF8_TO_TCHAR(gridPtr->getName().c_str()), gridPtr->activeVoxelCount()));
	}

	void ExtractGridSurface_MarchingCubes(const FString &gridName, bool threaded)
	{
		typedef typename Vdb::GridOps::ExtractSurfaceOp<GridTreeType, GridTreeType::ValueOnIter, Vdb::GridOps::BitTreeType> ExtractSurfaceOpType;
		typedef typename openvdb::TreeAdapter<openvdb::Grid<Vdb::GridOps::BitTreeType>> Adapter;
		typedef typename openvdb::tools::valxform::SharedOpTransformer<GridTreeType::ValueOnIter, Adapter::TreeType, ExtractSurfaceOpType> ExtractSurfaceProcessor;
		GridTypePtr gridPtr = GetGridPtrChecked(gridName);
		ExtractSurfaceOpType ExtractSurfaceOp(gridPtr);
		ExtractSurfaceProcessor ExtractSurfaceProc(gridPtr->beginValueOn(), Adapter::tree(*(MarchingCubesMeshOps[gridName]->GridPtr)), ExtractSurfaceOp, openvdb::MERGE_ACTIVE_STATES);
		UE_LOG(LogOpenVDBModule, Display, TEXT("%s"), *FString::Printf(TEXT("%s (pre marching cubes surface op) %d active voxels"), UTF8_TO_TCHAR(gridPtr->getName().c_str()), gridPtr->activeVoxelCount()));
		UE_LOG(LogOpenVDBModule, Display, TEXT("%s"), *FString::Printf(TEXT("%s (pre marching cubes surface op) %d active voxels"), UTF8_TO_TCHAR(MarchingCubesMeshOps[gridName]->GridPtr->getName().c_str()), MarchingCubesMeshOps[gridName]->GridPtr->activeVoxelCount()));
		ExtractSurfaceProc.process(threaded);
		UE_LOG(LogOpenVDBModule, Display, TEXT("%s"), *FString::Printf(TEXT("%s (post marching cubes surface op) %d active voxels"), UTF8_TO_TCHAR(gridPtr->getName().c_str()), gridPtr->activeVoxelCount()));
		UE_LOG(LogOpenVDBModule, Display, TEXT("%s"), *FString::Printf(TEXT("%s (post marching cubes surface op) %d active voxels"), UTF8_TO_TCHAR(MarchingCubesMeshOps[gridName]->GridPtr->getName().c_str()), MarchingCubesMeshOps[gridName]->GridPtr->activeVoxelCount()));
	}

	void ApplyVoxelTypes(const FString &gridName, bool threaded, TArray<TEnumAsByte<EVoxelType>> &sectionMaterialIDs)
	{
		typedef typename Vdb::GridOps::BasicSetVoxelTypeOp<GridTreeType, GridTreeType::ValueOnIter> BasicSetVoxelTypeOpType;
		typedef typename openvdb::tools::valxform::SharedOpApplier<GridTreeType::ValueOnIter, BasicSetVoxelTypeOpType> BasicSetVoxelProcessor;
		GridTypePtr gridPtr = GetGridPtrChecked(gridName);
		BasicSetVoxelTypeOpType BasicSetVoxelTypeOp(gridPtr);
		BasicSetVoxelProcessor BasicSetVoxelProc(gridPtr->beginValueOn(), BasicSetVoxelTypeOp);
		BasicSetVoxelProc.process(threaded);
		BasicSetVoxelTypeOp.GetActiveMaterials(sectionMaterialIDs);
	}

	void MeshRegionCubes(const FString &gridName)
	{
		GridTypePtr gridPtr = GetGridPtrChecked(gridName);
		const bool threaded = true;
		check(CubesMeshOps.Contains(gridName));
		CubesMeshOps[gridName]->doMeshOp(threaded);
	}

	void MeshRegionMarchingCubes(const FString &gridName)
	{
		GridTypePtr gridPtr = GetGridPtrChecked(gridName);
		const bool threaded = true;
		check(MarchingCubesMeshOps.Contains(gridName));
		MarchingCubesMeshOps[gridName]->doMeshOp(threaded);
	}

	void GetAllGridIDs(TArray<FString> &OutGridIDs) const
	{
		for (auto i = GridsPtr->begin(); i != GridsPtr->end(); ++i)
		{
			OutGridIDs.Add(FString::Printf(TEXT("%s"), UTF8_TO_TCHAR((*i)->getName().c_str())));
		}
	}

	void GetGridDimensions(const FString &gridName, FVector &worldStart, FVector &worldEnd, FVector &firstActive)
	{
		GridTypePtr gridPtr = GetGridPtrChecked(gridName);
		openvdb::Coord firstActiveCoord;
		openvdb::CoordBBox activeIndexBBox = gridPtr->evalActiveVoxelBoundingBox();
		GetFirstActiveCoord(gridPtr, activeIndexBBox, firstActiveCoord);
		const openvdb::BBoxd worldBBox = gridPtr->transform().indexToWorld(activeIndexBBox);
		const openvdb::Vec3d firstActiveWorld = gridPtr->indexToWorld(firstActiveCoord);
		const openvdb::Vec3d voxelSize = gridPtr->transform().voxelSize();
		worldStart.X = worldBBox.min().x();
		worldStart.Y = worldBBox.min().y();
		worldStart.Z = worldBBox.min().z();
		worldEnd.X = worldBBox.min().x();
		worldEnd.Y = worldBBox.min().y();
		worldEnd.Z = worldBBox.min().z();
		firstActive.X = firstActiveWorld.x() + voxelSize.x()*0.5;
		firstActive.Y = firstActiveWorld.y() + voxelSize.y()*0.5;
		firstActive.Z = firstActiveWorld.z() + voxelSize.z();
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
		CubesMeshOps.FindChecked(gridName)->markChanged();
	}

	void SetVoxelActiveState(const FString &gridName, const FIntVector &indexCoord, const bool &isActive)
	{
		GridTypePtr gridPtr = GetGridPtrChecked(gridName);
		const openvdb::Coord coord(indexCoord.X, indexCoord.Y, indexCoord.Z);
		GridType::Accessor acc = gridPtr->getAccessor();
		acc.setActiveState(coord, isActive);
		CubesMeshOps.FindChecked(gridName)->markChanged();
	}

	void SetVoxelValueAndActiveState(const FString &gridName, const FIntVector &indexCoord, const typename GridType::ValueType &value, const bool &isActive)
	{
		GridTypePtr gridPtr = GetGridPtrChecked(gridName);
		const openvdb::Coord coord(indexCoord.X, indexCoord.Y, indexCoord.Z);
		GridType::Accessor acc = gridPtr->getAccessor();
		acc.modifyValueAndActiveState<Vdb::GridOps::BasicModifyActiveOp<typename GridType::ValueType>>(coord, Vdb::GridOps::BasicModifyActiveOp<typename GridType::ValueType>>(value, isActive));
		CubesMeshOps.FindChecked(gridName)->markChanged();
	}

private:
	UObject const * Parent;
	bool isFileInSync;
	TSharedPtr<openvdb::io::File> FilePtr;
	openvdb::GridPtrVecPtr GridsPtr;
	openvdb::MetaMap::Ptr FileMetaPtr;
	mutable openvdb::GridPtrVec::iterator CachedGrid;
	TMap<FString, TSharedRef<Vdb::GridOps::CubeMesher<GridTreeType>>> CubesMeshOps;
	TMap<FString, TSharedRef<Vdb::GridOps::MarchingCubesMesher<GridTreeType>>> MarchingCubesMeshOps;

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

	GridTypePtr CreateGrid(const FString &gridName,
		const FIntVector &indexStart,
		const FIntVector &indexEnd,
		openvdb::math::Transform::Ptr xform,
		FGridMeshBuffers &meshBuffers)
	{
		GridTypePtr gridPtr = GridType::create();
		gridPtr->setName(TCHAR_TO_UTF8(*gridName));
		gridPtr->setTransform(xform);
		GridsPtr->push_back(gridPtr);
		CachedGrid = GridsPtr->end();		
		CubesMeshOps.Emplace(gridName, TSharedRef<Vdb::GridOps::CubeMesher<GridTreeType>>(new Vdb::GridOps::CubeMesher<GridTreeType>(gridPtr, meshBuffers)));
		MarchingCubesMeshOps.Emplace(gridName, TSharedRef<Vdb::GridOps::MarchingCubesMesher<GridTreeType>>(new Vdb::GridOps::MarchingCubesMesher<GridTreeType>(gridPtr, meshBuffers)));
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

	inline void GetFirstActiveCoord(GridTypePtr gridPtr, openvdb::CoordBBox &activeIndexBBox, openvdb::Coord &firstActive)
	{
		activeIndexBBox = gridPtr->evalActiveVoxelBoundingBox();
		for (auto i = gridPtr->cbeginValueOn(); i; ++i)
		{
			if (i.isVoxelValue())
			{
				firstActive = i.getCoord();
				//Find the first voxel above that is off
				for (int32 z = i.getCoord().z(); z <= activeIndexBBox.max().z(); ++z)
				{
					firstActive.setZ(z);
					if (i.getTree()->isValueOff(firstActive))
					{
						return;
					}
				}
			}
		}
		//TODO: Handle when no such voxel found
		firstActive = openvdb::Coord(0, 0, 0);
	}
};

typedef VdbHandlePrivate<TreeType, Vdb::GridOps::IndexTreeType, openvdb::math::ScaleMap> VdbHandlePrivateType;