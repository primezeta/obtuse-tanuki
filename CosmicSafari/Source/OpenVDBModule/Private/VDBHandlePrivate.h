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

//5,4,3 is the standard openvdb tree configuration
typedef openvdb::tree::Tree4<FVoxelData, 5, 4, 3>::Type TreeType;
struct AsyncIONotifier;

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
	
	static FString MetaName_WorldName() { return TEXT("WorldName"); }
	static FString MetaName_RegionScale() { return TEXT("RegionScale"); }
	static FString MetaName_RegionStart() { return TEXT("RegionStart"); }
	static FString MetaName_RegionEnd() { return TEXT("RegionEnd"); }
	static FString MetaName_RegionIndexStart() { return TEXT("RegionIndexStart"); }
	static FString MetaName_RegionIndexEnd() { return TEXT("RegionIndexEnd"); }

private:
	bool EnableGridStats;
	bool EnableDelayLoad;
	bool AreAllGridsInSync;
	bool IsFileMetaInSync;
	TSharedPtr<openvdb::io::File> FilePtr;
	openvdb::GridPtrVecPtr GridsPtr;
	openvdb::MetaMap::Ptr FileMetaPtr;
	mutable openvdb::GridPtrVec::iterator CachedGrid;
	TMap<FString, TSharedRef<Vdb::GridOps::CubeMesher<GridTreeType>>> CubesMeshOps;
	TMap<FString, TSharedRef<Vdb::GridOps::MarchingCubesMesher<GridTreeType>>> MarchingCubesMeshOps;
	AsyncIONotifier AsyncIO;
	friend struct AsyncIONotifier;

public:

	VdbHandlePrivate() :
		EnableGridStats(false),
		EnableDelayLoad(false),
		AreAllGridsInSync(true),
		IsFileMetaInSync(true),
		AsyncIO(*this)
	{
	}

	~VdbHandlePrivate()
	{
		WriteChanges(true); //TODO: Async write on destructor...however would need a non-member AsyncIONotifier object
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

	bool InitializeDatabase(const FString &filePath, const bool &enableGridStats, const bool &enableDelayLoad)
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

		const bool isInitialized = CreateOrVerifyDatabase(filePath, enableGridStats, enableDelayLoad);
		return isInitialized;
	}

	inline GridType& GetGrid(const FString &gridName)
	{
		bool isNewGrid = false; //not used
		return GetOrCreateGrid(gridName, isNewGrid);
	}

	inline GridType& GetOrCreateGrid(const FString &gridName, bool &isNewGrid, GridTypePtr gridPtr = nullptr)
	{
		//Find the grid using the cached iterator.
		//If not found, create a new grid and point the cached iterator to it.
		isNewGrid = false;
		if (CachedGrid == GridsPtr->end() || gridName != UTF8_TO_TCHAR((*CachedGrid)->getName().c_str()))
		{
			CachedGrid = GridsPtr->begin();
			for (; CachedGrid != GridsPtr->end() && gridName != UTF8_TO_TCHAR((*CachedGrid)->getName().c_str()); ++CachedGrid);
		}
		if (CachedGrid == GridsPtr->end())
		{
			UE_LOG(LogOpenVDBModule, Display, TEXT("Creating grid [%s]"), *gridName);
			gridPtr = GridType::create();
			gridPtr->setName(TCHAR_TO_UTF8(*gridName));
			GridsPtr->push_back(gridPtr);
			CachedGrid = GridsPtr->end();
			CachedGrid--;
			check(CachedGrid != GridsPtr->end());
			isNewGrid = true;
		}
		else
		{
			gridPtr = openvdb::gridPtrCast<GridType>(*CachedGrid);
		}
		check(gridPtr != nullptr);
		return *gridPtr;
	}

	void AddGrid(const FString &gridName,
		const FIntVector &gridDimensions,
		const FVector &worldOffset,
		const FVector &voxelSize,
		TArray<FProcMeshSection> &sectionBuffers)
	{
		const openvdb::Vec3d voxelScale(voxelSize.X, voxelSize.Y, voxelSize.Z);
		const openvdb::Vec3d worldOffsetVec3d(worldOffset.X, worldOffset.Y, worldOffset.Z);
		const openvdb::math::ScaleTranslateMap::Ptr map = openvdb::math::ScaleTranslateMap::Ptr(new openvdb::math::ScaleTranslateMap(voxelScale, worldOffsetVec3d));
		const openvdb::math::Transform::Ptr xformPtr = openvdb::math::Transform::Ptr(new openvdb::math::Transform(map));

		//The file is flagged as needing to be written if any or all of:
		//The grid has been created as new.
		//The grid's transform has changed (in which case the resulting mesh will probably have a different appearance).
		//The grid's start or ending index coords have changed (NOTE and TODO: the logic was changed so that all grids start indexed from 0,0,0!)
		bool isNewGrid = false;
		GridType &grid = GetOrCreateGrid(gridName, isNewGrid);
		if (isNewGrid || grid.transform() != *xformPtr)
		{
			UE_LOG(LogOpenVDBModule, Display, TEXT("Setting grid [%s] transform to [%s]"), UTF8_TO_TCHAR(grid.getName().c_str()), UTF8_TO_TCHAR(grid.transform().mapType().c_str()));
			grid.setTransform(xformPtr);
			CubesMeshOps.Emplace(gridName, TSharedRef<Vdb::GridOps::CubeMesher<GridTreeType>>(new Vdb::GridOps::CubeMesher<GridTreeType>(grid, sectionBuffers)));
			MarchingCubesMeshOps.Emplace(gridName, TSharedRef<Vdb::GridOps::MarchingCubesMesher<GridTreeType>>(new Vdb::GridOps::MarchingCubesMesher<GridTreeType>(grid, sectionBuffers)));
			DesyncGridsStatus();
		}

		//If the grid starting location is changed then update the meta
		const openvdb::Vec3IMetadata::Ptr currentIndexStartMeta = grid.getMetadata<openvdb::Vec3IMetadata>(TCHAR_TO_UTF8(*MetaName_RegionIndexStart()));
		const openvdb::Vec3i indexStartVec(0, 0, 0);
		const openvdb::Vec3d worldStart = map->applyMap(openvdb::Vec3d(indexStartVec.x(), indexStartVec.y(), indexStartVec.z()));
		if (currentIndexStartMeta == nullptr || !openvdb::math::isExactlyEqual(currentIndexStartMeta->value(), indexStartVec))
		{
			UE_LOG(LogOpenVDBModule, Display, TEXT("Setting grid [%s] start to [%s] (index [%s])"), UTF8_TO_TCHAR(grid.getName().c_str()), UTF8_TO_TCHAR(worldStart.str().c_str()), UTF8_TO_TCHAR(indexStartVec.str().c_str()));
			grid.insertMeta(TCHAR_TO_UTF8(*MetaName_RegionIndexStart()), openvdb::Vec3IMetadata(indexStartVec));
			grid.insertMeta(TCHAR_TO_UTF8(*MetaName_RegionStart()), openvdb::Vec3DMetadata(worldStart));
			DesyncGridsStatus();
		}

		const openvdb::Vec3IMetadata::Ptr currentIndexEndMeta = grid.getMetadata<openvdb::Vec3IMetadata>(TCHAR_TO_UTF8(*MetaName_RegionIndexEnd()));
		const openvdb::Vec3i indexEndVec(gridDimensions.X, gridDimensions.Y, gridDimensions.Z);
		const openvdb::Vec3d worldEnd = map->applyMap(openvdb::Vec3d(indexEndVec.x(), indexEndVec.y(), indexEndVec.z()));
		if (currentIndexEndMeta == nullptr || !openvdb::math::isExactlyEqual(currentIndexEndMeta->value(), indexEndVec))
		{
			UE_LOG(LogOpenVDBModule, Display, TEXT("Setting grid [%s] start to [%s] (index [%s])"), UTF8_TO_TCHAR(grid.getName().c_str()), UTF8_TO_TCHAR(worldEnd.str().c_str()), UTF8_TO_TCHAR(indexEndVec.str().c_str()));
			grid.insertMeta(TCHAR_TO_UTF8(*MetaName_RegionIndexEnd()), openvdb::Vec3IMetadata(indexEndVec));
			grid.insertMeta(TCHAR_TO_UTF8(*MetaName_RegionEnd()), openvdb::Vec3DMetadata(worldEnd));
			DesyncGridsStatus();
		}
	}

	GridType& ReadGridTree(const FString &gridName, FIntVector &indexStart, FIntVector &indexEnd)
	{
		//Only try to read the data tree from file if the current grid has no active voxels
		//TODO: Handle case when a grid has 0 active voxels legitimately?
		GridType &grid = GetGrid(gridName);
		const openvdb::Index64 activeVoxelCount = grid.activeVoxelCount();
		if (activeVoxelCount == 0)
		{
			UE_LOG(LogOpenVDBModule, Display, TEXT("Grid [%s] tree is empty"), UTF8_TO_TCHAR(grid.getName().c_str()));
			if (FilePtr->hasGrid(TCHAR_TO_UTF8(*gridName)))
			{
				//Read the tree from file then swap the current grid tree to the tree from file
				OpenFileGuard();
				UE_LOG(LogOpenVDBModule, Display, TEXT("Reading grid [%s] tree from file"), UTF8_TO_TCHAR(grid.getName().c_str()));
				GridTypePtr activeGrid = openvdb::gridPtrCast<GridType>(FilePtr->readGrid(TCHAR_TO_UTF8(*gridName)));
				check(activeGrid != nullptr);
				check(activeGrid->treePtr() != nullptr);
				grid.setTree(activeGrid->treePtr());
			}
		}
		UE_LOG(LogOpenVDBModule, Display, TEXT("Grid [%s] tree has %d active voxels"), UTF8_TO_TCHAR(grid.getName().c_str()), activeVoxelCount);

		//TODO: Create FIntBox class
		const auto metaMin = grid.getMetadata<openvdb::Vec3IMetadata>(TCHAR_TO_UTF8(*MetaName_RegionIndexStart()));
		const auto metaMax = grid.getMetadata<openvdb::Vec3IMetadata>(TCHAR_TO_UTF8(*MetaName_RegionIndexEnd()));
		const openvdb::CoordBBox indexBBox(openvdb::Coord(metaMin->value()), openvdb::Coord(metaMax->value()));
		indexStart.X = indexBBox.min().x();
		indexStart.Y = indexBBox.min().y();
		indexStart.Z = indexBBox.min().z();
		indexEnd.X = indexBBox.max().x();
		indexEnd.Y = indexBBox.max().y();
		indexEnd.Z = indexBBox.max().z();
		return grid;
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
		else
		{
			UE_LOG(LogOpenVDBModule, Error, TEXT("Failed to find file meta [%s] in [%s]"), *metaName, UTF8_TO_TCHAR(FilePtr->filename().c_str()));
		}
		return metaDataTShared;
	}

	template<typename FileMetaType>
	void InsertFileMeta(const FString &metaName, const FileMetaType &metaValue)
	{
		CloseFileGuard();
		openvdb::TypedMetadata<FileMetaType>::Ptr currentFileMeta = FileMetaPtr->getMetadata<openvdb::TypedMetadata<FileMetaType>>(TCHAR_TO_UTF8(*metaName));
		if (currentFileMeta == nullptr || !openvdb::math::isExactlyEqual(currentFileMeta->value(), metaValue))
		{
			UE_LOG(LogOpenVDBModule, Display, TEXT("Inserting file meta [%s] into [%s]"), *metaName, UTF8_TO_TCHAR(FilePtr->filename().c_str()));
			FileMetaPtr->insertMeta(TCHAR_TO_UTF8(*metaName), openvdb::TypedMetadata<FileMetaType>(metaValue));
			DesyncFileMetaStatus();
		}
	}

	void RemoveFileMeta(const FString &metaName)
	{
		CloseFileGuard();
		const std::string name = TCHAR_TO_UTF8(*metaName);
		for (auto i = FileMetaPtr->beginMeta(); i != FileMetaPtr->endMeta(); ++i)
		{
			if (i->first == name)
			{
				//Here we're iterating over the metamap twice because removeMeta() uses std::find. However, removeMeta() doesn't provide any
				//indication if the item to remove existed prior, which we need to know for setting the file status with DesyncFileMetaStatus().
				UE_LOG(LogOpenVDBModule, Display, TEXT("Removing file meta [%s] from [%s]"), *metaName, UTF8_TO_TCHAR(FilePtr->filename().c_str()));
				FileMetaPtr->removeMeta(name);
				DesyncFileMetaStatus();
			}
		}
	}

	void RemoveGridFromGridVec(const FString &gridName)
	{
		const std::string name = TCHAR_TO_UTF8(*gridName);
		for (auto i = GridsPtr->begin(); i != GridsPtr->end(); ++i)
		{
			const std::string &gname = (*i)->getName();
			if (gname == name)
			{
				UE_LOG(LogOpenVDBModule, Display, TEXT("Removing grid [%s]"), UTF8_TO_TCHAR(gname.c_str()));
				CubesMeshOps.Remove(gridName);
				MarchingCubesMeshOps.Remove(gridName);
				i->reset();
				GridsPtr->erase(i);
				CachedGrid = GridsPtr->end();
				DesyncGridsStatus();
				return;
			}
		}
		UE_LOG(LogOpenVDBModule, Error, TEXT("Could not find grid [%s] for removal"), *gridName);
	}

	template<typename GridMetaType>
	TSharedPtr<openvdb::TypedMetadata<GridMetaType>> GetGridMetaValue(const FString &gridName, const FString &metaName)
	{
		TSharedPtr<openvdb::TypedMetadata<GridMetaType>> metaDataTShared(nullptr);
		GridType &grid = GetGrid(gridName);
		openvdb::TypedMetadata<GridMetaType>::Ptr metaDataPtr = grid.getMetadata<openvdb::TypedMetadata<GridMetaType>>(TCHAR_TO_UTF8(*gridName));
		if (metaDataPtr != nullptr)
		{
			UE_LOG(LogOpenVDBModule, Display, TEXT("Getting grid meta [%s] from [%s]"), *metaName, UTF8_TO_TCHAR(grid.getName().c_str()));
			metaDataTShared = TSharedPtr<openvdb::TypedMetadata<GridMetaType>>(new openvdb::TypedMetadata<GridMetaType>());
			metaDataTShared->copy(*metaDataPtr);
		}
		return metaDataTShared;
	}

	template<typename GridMetaType>
	void InsertGridMeta(const FString &gridName, const FString &metaName, const GridMetaType &metaValue)
	{
		GridType &grid = GetGrid(gridName);
		openvdb::TypedMetadata<GridMetaType>::Ptr currentGridMeta = grid.getMetadata<openvdb::TypedMetadata<GridMetaType>>(TCHAR_TO_UTF8(*metaName));
		if (currentGridMeta == nullptr || !openvdb::math::isExactlyEqual(currentGridMeta->value(), metaValue))
		{
			UE_LOG(LogOpenVDBModule, Display, TEXT("Inserting grid meta [%s] into [%s]"), *metaName, UTF8_TO_TCHAR(grid.getName().c_str()));
			grid.insertMeta(TCHAR_TO_UTF8(*metaName), openvdb::TypedMetadata<GridMetaType>(metaValue));
			DesyncGridsStatus();
		}
	}

	template<typename GridMetaType>
	void RemoveGridMeta(const FString &gridName, const FString &metaName)
	{
		GridType &grid = GetGrid(gridName);
		const std::string name = TCHAR_TO_UTF8(*metaName);
		for (auto i = grid.beginMeta(); i != grid.endMeta(); ++i)
		{
			if (i->first == name)
			{
				//Here we're iterating over the metamap twice because removeMeta() uses std::find. However, removeMeta() doesn't provide any
				//indication if the item to remove existed prior, which we need to know for setting the file status with DesyncGridsStatus().
				UE_LOG(LogOpenVDBModule, Display, TEXT("Removing grid meta [%s] from [%s]"), *metaName, UTF8_TO_TCHAR(grid.getName().c_str()));
				grid.removeMeta(name);
				DesyncGridsStatus();
			}
		}
	}

	bool WriteChanges(bool isFinal)
	{
		//Write any pending changes
		bool isFileChanged = false;
		if (AreChangesPending())
		{
			isFileChanged = true;
			CloseFileGuard(); //openvdb::io::File must be closed in order to write
							  //openvdb::io::File can write only grids or both grids and file meta but can't write only file meta
			if (FileMetaPtr == nullptr)
			{
				check(GridsPtr);
				UE_LOG(LogOpenVDBModule, Display, TEXT("Writing grid changes to [%s]"), UTF8_TO_TCHAR(FilePtr->filename().c_str()));
				FilePtr->write(*GridsPtr, openvdb::MetaMap());
			}
			else
			{
				check(GridsPtr);
				check(FileMetaPtr);
				UE_LOG(LogOpenVDBModule, Display, TEXT("Writing grid and file meta changes to [%s]"), UTF8_TO_TCHAR(FilePtr->filename().c_str()));
				FilePtr->write(*GridsPtr, *FileMetaPtr);
			}

			if (isFinal)
			{
				UE_LOG(LogOpenVDBModule, Display, TEXT("Finalizing changes to [%s]"), UTF8_TO_TCHAR(FilePtr->filename().c_str()));
				ResetSharedResources();
			}
		}
		return isFileChanged;
	}

	bool WriteChangesAsync(bool isFinal)
	{
		//Queue up any pending changes to be written
		bool isFileChanged = false;
		if (AreChangesPending())
		{
			isFileChanged = true;
			CloseFileGuard(); //openvdb::io::File must be closed in order to write
							  //openvdb::io::File can write only grids or both grids and file meta but can't write only file meta			
			//Clear the async IO grids and then deep copy all of the current grids to the async IO. TODO: Better way to do this than deep copying all? Critical section?
			AsyncIO.Clear();
			AsyncIO.IsFinalChanges = isFinal;
			for (auto i = GridsPtr->begin(); i != GridsPtr->end(); ++i)
			{
				AsyncIO.OutGrids.push_back((*i)->deepCopyGrid());
			}

			openvdb::io::Queue queue;
			queue.addNotifier(boost::bind(&AsyncIONotifier::Callback, &AsyncIO, _1, _2));
			if (FileMetaPtr == nullptr)
			{
				check(GridsPtr);
				UE_LOG(LogOpenVDBModule, Display, TEXT("Queuing pending grid changes for async write to [%s]"), UTF8_TO_TCHAR(FilePtr->filename().c_str()));
				queue.write(AsyncIO.OutGrids, *(FilePtr->copy()), openvdb::MetaMap());
			}
			else
			{
				check(GridsPtr);
				check(FileMetaPtr);
				UE_LOG(LogOpenVDBModule, Display, TEXT("Queuing pending grid and file meta changes for async write to [%s]"), UTF8_TO_TCHAR(FilePtr->filename().c_str()));
				queue.write(AsyncIO.OutGrids, *(FilePtr->copy()), *(FileMetaPtr->deepCopyMeta()));
			}

			if (isFinal)
			{
				UE_LOG(LogOpenVDBModule, Display, TEXT("Async changes to [%s] will be finalized"), UTF8_TO_TCHAR(FilePtr->filename().c_str()));
			}
		}
		return isFileChanged; //TODO: Provide mechanism to wait on async changes to complete
	}

	bool FillGrid_PerlinDensity(const FString &gridName, bool threaded, const FIntVector &fillIndexStart, const FIntVector &fillIndexEnd, int32 seed, float frequency, float lacunarity, float persistence, int32 octaveCount)
	{
		bool isChanged = false;
		GridType &grid = GetGrid(gridName);

		//Noise module parameters are at the grid-level metadata
		const openvdb::Int32Metadata::Ptr seedMeta = grid.getMetadata<openvdb::Int32Metadata>("seed");
		const openvdb::FloatMetadata::Ptr frequencyMeta = grid.getMetadata<openvdb::FloatMetadata>("frequency");
		const openvdb::FloatMetadata::Ptr lacunarityMeta = grid.getMetadata<openvdb::FloatMetadata>("lacunarity");
		const openvdb::FloatMetadata::Ptr persistenceMeta = grid.getMetadata<openvdb::FloatMetadata>("persistence");
		const openvdb::Int32Metadata::Ptr octaveCountMeta = grid.getMetadata<openvdb::Int32Metadata>("octaveCount");
		const bool isEmpty = grid.tree().empty();
		if (isEmpty ||
			seedMeta == nullptr || !openvdb::math::isExactlyEqual(seed, seedMeta->value()) ||
			frequencyMeta == nullptr || !openvdb::math::isApproxEqual(frequency, frequencyMeta->value()) ||
			lacunarityMeta == nullptr || !openvdb::math::isApproxEqual(lacunarity, lacunarityMeta->value()) ||
			persistenceMeta == nullptr || !openvdb::math::isApproxEqual(persistence, persistenceMeta->value()) ||
			octaveCountMeta == nullptr || !openvdb::math::isExactlyEqual(octaveCount, octaveCountMeta->value()))
		{
			isChanged = true;

			//Update the Perlin noise parameters
			grid.insertMeta("seed", openvdb::Int32Metadata(seed));
			grid.insertMeta("frequency", openvdb::FloatMetadata(frequency));
			grid.insertMeta("lacunarity", openvdb::FloatMetadata(lacunarity));
			grid.insertMeta("persistence", openvdb::FloatMetadata(persistence));
			grid.insertMeta("octaveCount", openvdb::Int32Metadata(octaveCount));

			CubesMeshOps[gridName]->markChanged();
			MarchingCubesMeshOps[gridName]->markChanged();

			typedef typename Vdb::GridOps::PerlinNoiseFillOp<GridTreeType, GridTreeType::ValueOnIter> NoiseFillOpType;
			typedef typename openvdb::tools::valxform::SharedOpApplier<GridTreeType::ValueOnIter, NoiseFillOpType> NoiseFillProcessor;
			const openvdb::Coord startFill(0, 0, 0); //TODO: Finish cleaning up code due to changing to 0-indexed grids
			const openvdb::Coord endFill(fillIndexEnd.X - fillIndexStart.X, fillIndexEnd.Y - fillIndexStart.Y, fillIndexEnd.Z - fillIndexStart.Z);
			openvdb::CoordBBox fillBBox = openvdb::CoordBBox(startFill, endFill);
			check(!fillBBox.empty());
			NoiseFillOpType noiseFillOp(grid, fillBBox, seed, frequency, lacunarity, persistence, octaveCount);
			NoiseFillProcessor NoiseFillProc(grid.beginValueOn(), noiseFillOp);
			NoiseFillProc.process(threaded);
			DesyncGridsStatus();
		}
		return isChanged;
	}

	void ExtractGridSurface_Cubes(const FString &gridName, bool threaded)
	{
		typedef typename Vdb::GridOps::BasicExtractSurfaceOp<GridTreeType, GridTreeType::ValueOnIter> BasicExtractSurfaceOpType;
		typedef typename openvdb::tools::valxform::SharedOpApplier<GridTreeType::ValueOnIter, BasicExtractSurfaceOpType> BasicExtractSurfaceProcessor;
		GridType &grid = GetGrid(gridName);
		BasicExtractSurfaceOpType BasicExtractSurfaceOp(grid);
		BasicExtractSurfaceProcessor BasicExtractSurfaceProc(grid.beginValueOn(), BasicExtractSurfaceOp);
		BasicExtractSurfaceProc.process(threaded);
		DesyncGridsStatus();
		UE_LOG(LogOpenVDBModule, Display, TEXT("[%s] %d active voxels"), UTF8_TO_TCHAR(grid.getName().c_str()), grid.activeVoxelCount());
	}

	void ExtractGridSurface_MarchingCubes(const FString &gridName, bool threaded)
	{
		typedef typename Vdb::GridOps::ExtractSurfaceOp<GridTreeType, GridTreeType::ValueOnIter, Vdb::GridOps::BitTreeType> ExtractSurfaceOpType;
		typedef typename openvdb::TreeAdapter<openvdb::Grid<Vdb::GridOps::BitTreeType>> Adapter;
		typedef typename openvdb::tools::valxform::SharedOpTransformer<GridTreeType::ValueOnIter, Adapter::TreeType, ExtractSurfaceOpType> ExtractSurfaceProcessor;
		GridType &grid = GetGrid(gridName);
		ExtractSurfaceOpType ExtractSurfaceOp(grid);
		ExtractSurfaceProcessor ExtractSurfaceProc(grid.beginValueOn(), Adapter::tree(MarchingCubesMeshOps[gridName]->Grid), ExtractSurfaceOp, openvdb::MERGE_ACTIVE_STATES);
		ExtractSurfaceProc.process(threaded);
		DesyncGridsStatus();
		UE_LOG(LogOpenVDBModule, Display, TEXT("[%s] %d active voxels"), UTF8_TO_TCHAR(grid.getName().c_str()), grid.activeVoxelCount());
	}

	void CalculateGradient(const FString &gridName, bool threaded)
	{
		typedef typename Vdb::GridOps::CalcGradientOp_FVoxelData<GridTreeType, GridTreeType::ValueOnCIter, openvdb::Vec3fTree> CalcGradientOp_FVoxelDataType;
		typedef typename Vdb::GridOps::ISGradient_FVoxelData<openvdb::math::CD_2ND, CalcGradientOp_FVoxelDataType::SourceAccessorType>::Vec3Type VecType;
		typedef typename openvdb::TreeAdapter<openvdb::Grid<openvdb::Vec3fTree>> Adapter;
		typedef typename openvdb::tools::valxform::SharedOpTransformer<GridTreeType::ValueOnCIter, Adapter::TreeType, CalcGradientOp_FVoxelDataType> CalcGradientOp_FVoxelDataProcessor;
		GridType &grid = GetGrid(gridName);
		openvdb::Grid<openvdb::Vec3fTree> &gradientGrid = MarchingCubesMeshOps[gridName]->Gradient;
		CalcGradientOp_FVoxelDataType CalcGradientOp_FVoxelDataOp(grid);
		CalcGradientOp_FVoxelDataProcessor CalcGradientProc(grid.cbeginValueOn(), Adapter::tree(gradientGrid), CalcGradientOp_FVoxelDataOp, openvdb::MERGE_ACTIVE_STATES);
		CalcGradientProc.process(threaded);
		UE_LOG(LogOpenVDBModule, Verbose, TEXT("[%s] %d active voxels"), UTF8_TO_TCHAR(gradientGrid.getName().c_str()), gradientGrid.activeVoxelCount());
	}

	void ApplyVoxelTypes(const FString &gridName, bool threaded, TArray<TEnumAsByte<EVoxelType>> &sectionVoxelTypes)
	{
		typedef typename Vdb::GridOps::BasicSetVoxelTypeOp<GridTreeType, GridTreeType::ValueOnIter> BasicSetVoxelTypeOpType;
		typedef typename openvdb::tools::valxform::SharedOpApplier<GridTreeType::ValueOnIter, BasicSetVoxelTypeOpType> BasicSetVoxelProcessor;
		GridType &grid = GetGrid(gridName);
		BasicSetVoxelTypeOpType BasicSetVoxelTypeOp(grid);
		BasicSetVoxelProcessor BasicSetVoxelProc(grid.beginValueOn(), BasicSetVoxelTypeOp);
		BasicSetVoxelProc.process(threaded);
		BasicSetVoxelTypeOp.GetActiveVoxelTypes(sectionVoxelTypes);
		UEnum* Enum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EVoxelType"), true);
		check(Enum);
		FString msg = FString::Printf(TEXT("[%s] active voxel types are"), UTF8_TO_TCHAR(grid.getName().c_str()));
		for (auto i = sectionVoxelTypes.CreateConstIterator(); i; ++i)
		{
			msg += FString::Printf(TEXT(" [%s]"), *Enum->GetDisplayNameText((int32)i->GetValue()).ToString());
		}
		if (sectionVoxelTypes.Num() == 0)
		{
			msg = FString::Printf(TEXT("[%s] has no active voxel types"), UTF8_TO_TCHAR(grid.getName().c_str()));
		}
		DesyncGridsStatus();
		UE_LOG(LogOpenVDBModule, Display, TEXT("%s"), *msg);
	}

	void MeshRegionCubes(const FString &gridName)
	{
		GridType &grid = GetGrid(gridName);
		UE_LOG(LogOpenVDBModule, Verbose, TEXT("Running basic cubes mesher for [%s]"), UTF8_TO_TCHAR(grid.getName().c_str()));
		const bool threaded = true;
		check(CubesMeshOps.Contains(gridName));
		CubesMeshOps[gridName]->doMeshOp(threaded);
	}

	void MeshRegionMarchingCubes(const FString &gridName)
	{
		GridType &grid = GetGrid(gridName);
		UE_LOG(LogOpenVDBModule, Verbose, TEXT("Running marching cubes mesher for [%s]"), UTF8_TO_TCHAR(grid.getName().c_str()));
		const bool threaded = true;
		check(MarchingCubesMeshOps.Contains(gridName));
		MarchingCubesMeshOps[gridName]->doMeshOp(threaded);
	}

	void GetAllGridIDs(TArray<FString> &OutGridIDs) const
	{
		for (auto i = GridsPtr->begin(); i != GridsPtr->end(); ++i)
		{
			OutGridIDs.Add(FString(UTF8_TO_TCHAR((*i)->getName().c_str())));
		}
	}

	bool GetGridDimensions(const FString &gridName, FVector &startLocation)
	{
		GridType &grid = GetGrid(gridName);
		openvdb::CoordBBox activeIndexBBox = grid.evalActiveVoxelBoundingBox();
		const bool hasActiveVoxels = grid.activeVoxelCount() > 0;
		if (hasActiveVoxels)
		{
			//Search for a start location among the active voxels
			openvdb::Coord firstActiveCoord = openvdb::Coord::max();
			GetFirstActiveCoord(grid, activeIndexBBox, firstActiveCoord);
			const openvdb::Vec3d firstActiveWorld = grid.indexToWorld(firstActiveCoord);
			const openvdb::Vec3d voxelSize = grid.transform().voxelSize();
			startLocation = FVector(firstActiveWorld.x() + voxelSize.x()*0.5,
				firstActiveWorld.y() + voxelSize.y()*0.5,
				firstActiveWorld.z() + voxelSize.z());
		}
		else
		{
			//Grid has no active values and thus no valid start location
			startLocation = FVector(FLT_TRUE_MIN, FLT_TRUE_MIN, FLT_TRUE_MIN);
		}
		return hasActiveVoxels;
	}

	bool GetGridDimensions(const FString &gridName, FBox &worldBounds)
	{
		GridType &grid = GetGrid(gridName);
		openvdb::CoordBBox activeIndexBBox = grid.evalActiveVoxelBoundingBox();
		const bool hasActiveVoxels = grid.activeVoxelCount() > 0;
		if (!hasActiveVoxels)
		{
			//The grid has no active values so provide the defined bounds of the entire volume
			const auto metaMin = grid.getMetadata<openvdb::Vec3IMetadata>(TCHAR_TO_UTF8(*MetaName_RegionIndexStart()));
			const auto metaMax = grid.getMetadata<openvdb::Vec3IMetadata>(TCHAR_TO_UTF8(*MetaName_RegionIndexEnd()));
			activeIndexBBox = openvdb::CoordBBox(openvdb::Coord(metaMin->value()), openvdb::Coord(metaMax->value()));
		}
		const openvdb::BBoxd worldBBox = grid.transform().indexToWorld(activeIndexBBox);
		worldBounds.Min = FVector(worldBBox.min().x(), worldBBox.min().y(), worldBBox.min().z());
		worldBounds.Max = FVector(worldBBox.max().x(), worldBBox.max().y(), worldBBox.max().z());
		return hasActiveVoxels;
	}

	bool GetGridDimensions(const FString &gridName, FBox &worldBounds, FVector &startLocation)
	{
		GridType &grid = GetGrid(gridName);
		openvdb::CoordBBox activeIndexBBox = grid.evalActiveVoxelBoundingBox();
		const bool hasActiveVoxels = grid.activeVoxelCount() > 0;
		if (hasActiveVoxels)
		{
			//Search for a start location among the active voxels
			openvdb::Coord firstActiveCoord = openvdb::Coord::max();
			GetFirstActiveCoord(grid, activeIndexBBox, firstActiveCoord);
			const openvdb::Vec3d firstActiveWorld = grid.indexToWorld(firstActiveCoord);
			const openvdb::Vec3d voxelSize = grid.transform().voxelSize();
			startLocation = FVector(firstActiveWorld.x() + voxelSize.x()*0.5,
								  firstActiveWorld.y() + voxelSize.y()*0.5,
								  firstActiveWorld.z() + voxelSize.z());
		}
		else
		{
			//The grid has no active values so provide the defined bounds of the entire volume
			const auto metaMin = grid.getMetadata<openvdb::Vec3IMetadata>(TCHAR_TO_UTF8(*MetaName_RegionIndexStart()));
			const auto metaMax = grid.getMetadata<openvdb::Vec3IMetadata>(TCHAR_TO_UTF8(*MetaName_RegionIndexEnd()));
			activeIndexBBox = openvdb::CoordBBox(openvdb::Coord(metaMin->value()), openvdb::Coord(metaMax->value()));
			startLocation = FVector(FLT_TRUE_MIN, FLT_TRUE_MIN, FLT_TRUE_MIN);
		}
		const openvdb::BBoxd worldBBox = grid.transform().indexToWorld(activeIndexBBox);
		worldBounds.Min = FVector(worldBBox.min().x(), worldBBox.min().y(), worldBBox.min().z());
		worldBounds.Max = FVector(worldBBox.max().x(), worldBBox.max().y(), worldBBox.max().z());
		return hasActiveVoxels;
	}

	void GetIndexCoord(const FString &gridName, const FVector &location, FIntVector &outIndexCoord)
	{
		GridType &grid = GetGrid(gridName);
		//worldToIndex returns a Vec3d, so floor it to get the index coords
		const openvdb::Coord coord = openvdb::Coord::floor(grid.worldToIndex(openvdb::Vec3d(location.X, location.Y, location.Z)));
		outIndexCoord.X = coord.x(); //TODO: Create the class FIntBox
		outIndexCoord.Y = coord.y();
		outIndexCoord.Z = coord.z();
	}

	inline void GetVoxelValue(const FString &gridName, const FIntVector &indexCoord, typename GridType::ValueType &outValue)
	{
		GridType &grid = GetGrid(gridName);
		const openvdb::Coord coord(indexCoord.X, indexCoord.Y, indexCoord.Z);
		GridType::ConstAccessor cacc = grid.getConstAccessor();
		outValue = cacc.getValue(coord);
	}

	inline bool GetVoxelActiveState(const FString &gridName, const FIntVector &indexCoord)
	{
		GridType &grid = GetGrid(gridName);
		const openvdb::Coord coord(indexCoord.X, indexCoord.Y, indexCoord.Z);
		GridType::ConstAccessor cacc = grid.getConstAccessor();
		return cacc.isValueOn(coord);
	}

	inline bool GetVoxelValueAndActiveState(const FString &gridName, const FIntVector &indexCoord, typename GridType::ValueType &outValue)
	{
		GridType &grid = GetGrid(gridName);
		const openvdb::Coord coord(indexCoord.X, indexCoord.Y, indexCoord.Z);
		GridType::ConstAccessor cacc = grid.getConstAccessor();
		outValue = cacc.getValue(coord);
		return cacc.isValueOn(coord);
	}

	inline void SetVoxelValue(const FString &gridName, const FIntVector &indexCoord, const typename GridType::ValueType &value)
	{
		GridType &grid = GetGrid(gridName);
		const openvdb::Coord coord(indexCoord.X, indexCoord.Y, indexCoord.Z);
		GridType::Accessor acc = grid.getAccessor();
		const typename GridType::ValueType previousValue = acc.getValue(coord);
		if (!openvdb::math::isExactlyEqual(value, previousValue))
		{
			acc.setValueOnly(coord, value);
			CubesMeshOps.FindChecked(gridName)->markChanged();
			MarchingCubesMeshOps.FindChecked(gridName)->markChanged();
			DesyncGridsStatus();
		}
	}

	inline void SetVoxelActiveState(const FString &gridName, const FIntVector &indexCoord, const bool &isActive)
	{
		GridType &grid = GetGrid(gridName);
		const openvdb::Coord coord(indexCoord.X, indexCoord.Y, indexCoord.Z);
		GridType::Accessor acc = grid.getAccessor();
		const bool previousIsActive = acc.isValueOn(coord);
		if (isActive != previousIsActive)
		{
			acc.setActiveState(coord, isActive);
			CubesMeshOps.FindChecked(gridName)->markChanged();
			MarchingCubesMeshOps.FindChecked(gridName)->markChanged();
			DesyncGridsStatus();
		}
	}

	inline void SetVoxelValueAndActiveState(const FString &gridName, const FIntVector &indexCoord, const typename GridType::ValueType &value, const bool &isActive)
	{
		GridType &grid = GetGrid(gridName);
		const openvdb::Coord coord(indexCoord.X, indexCoord.Y, indexCoord.Z);
		GridType::Accessor acc = grid.getAccessor();
		const typename GridType::ValueType previousValue = acc.getValue(coord);
		const bool previousIsActive = acc.isValueOn(coord);
		if (!openvdb::math::isExactlyEqual(value, previousValue) || isActive != previousIsActive)
		{
			if(isActive)
			{
				acc.setValueOn(coord, value);
			}
			else
			{
				acc.setValueOff(coord, value);
			}
			CubesMeshOps.FindChecked(gridName)->markChanged();
			MarchingCubesMeshOps.FindChecked(gridName)->markChanged();
			DesyncGridsStatus();
		}
	}

private:
	inline void ResetSharedResources()
	{
		FilePtr.Reset();
		GridsPtr.reset();
		FileMetaPtr.reset();
	}

	inline bool AreChangesPending()
	{
		return !AreAllGridsInSync || !IsFileMetaInSync;
	}

	//Setting sync states is separated out to prevent weird bugs that might be caused by, for example, stomping out a previous sync status
	inline void SyncGridsAndFileMetaStatus()
	{
		AreAllGridsInSync = true;
		IsFileMetaInSync = true;
	}

	inline void SyncGridsStatus()
	{
		AreAllGridsInSync = true;
	}

	inline void SyncFileMetaStatus()
	{
		IsFileMetaInSync = true;
	}

	inline void DesyncGridsAndFileMetaStatus()
	{
		AreAllGridsInSync = false;
		IsFileMetaInSync = false;
	}

	inline void DesyncGridsStatus()
	{
		AreAllGridsInSync = false;
		if (EnableGridStats)
		{
			//Grid stats are stored in file meta therefore changes to grids could change file meta
			IsFileMetaInSync = false;
		}
	}

	inline void DesyncFileMetaStatus()
	{
		IsFileMetaInSync = false;
	}

	inline void SetAsyncIOIsRunning(bool isAsyncIORunning)
	{
		check(!AsyncIO.IsAsyncIORunning);
		AsyncIO.IsAsyncIORunning = isAsyncIORunning;
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

	inline bool SetAndVerifyConfiguration(const FString &filePath,
		const bool enableGridStats,
		const bool enableDelayLoad,
		const bool discardUnwrittenChanges = false,
		const bool forceOverwriteExistingFile = false)
	{
		//Safe to write the new file if:
		//A) The path is valid (e.g. does not contain disallowed characters).
		//B) The filename is not empty.
		//C) No unwritten changes exist OR we are allowed to discard unwritten changes.
		//D) The file does not exist OR we are allowed to overwrite an existing file.
		FText PathNotValidReason;
		const FString fileName = FPaths::GetCleanFilename(filePath);
		const bool isPathValid = FPaths::ValidatePath(filePath, &PathNotValidReason);
		const bool isFileValid = !fileName.IsEmpty();
		const bool arePreviousChangesPending = FilePtr.IsValid() && !IsFileMetaInSync;
		const bool doesFileExist = FPaths::FileExists(filePath);
		const bool canDiscardAnyPending = !arePreviousChangesPending || discardUnwrittenChanges || filePath != UTF8_TO_TCHAR(FilePtr->filename().c_str());
		const bool canOverwriteAnyExisting = !doesFileExist || forceOverwriteExistingFile;
		if (!(isPathValid && isFileValid && canDiscardAnyPending && canOverwriteAnyExisting))
		{
			FString fileIOErrorMessage = TEXT("Cannot initialize the VDB file state because:");
			if (!isPathValid)
			{
				fileIOErrorMessage += FString::Printf(TEXT("\nInvalid path [%s]"), *filePath);
			}
			if (!isFileValid)
			{
				fileIOErrorMessage += FString::Printf(TEXT("\nInvalid filename [%s]"), *fileName);
			}
			if (!canDiscardAnyPending)
			{
				fileIOErrorMessage += FString::Printf(TEXT("\nThere are unwritten changes but the VDB is not configured to discard unwritten changes [%s]"), *fileName);
			}
			if (!canOverwriteAnyExisting)
			{
				fileIOErrorMessage += FString::Printf(TEXT("\nFile already exists but the VDB is not configured to overwrite existing files [%s]"), *fileName);
			}
			UE_LOG(LogOpenVDBModule, Fatal, TEXT("%s"), *fileIOErrorMessage);
			return false;
		}

		//Write unwritten changes to file if configured to do so
		if (FilePtr.IsValid() && !discardUnwrittenChanges)
		{
			//Determine if the current state is different from the previous state.
			//To determine if file-level meta is changed, just use openvdb::MetaMap::operator== (which compares sorted metamaps).
			//To determine if grids are changed, first just check if grid vectors are of different length. If the same length,
			//then check the mesh ops which contain a "changed" flag.
			OpenFileGuard();
			openvdb::MetaMap::Ptr previousFileMetaPtr = FilePtr->getMetadata();
			openvdb::GridPtrVecPtr previousGridsPtr = FilePtr->readAllGridMetadata();
			const bool isFileMetaValid = FileMetaPtr != nullptr;
			const bool isPreviousFileMetaValid = previousFileMetaPtr != nullptr;
			const bool isFileMetaChanged = (isFileMetaValid != isPreviousFileMetaValid) || (isFileMetaValid && *FileMetaPtr != *FilePtr->getMetadata());
			const bool isGridsPtrValid = GridsPtr != nullptr;
			const bool isPreviousGridsPtrValid = previousGridsPtr != nullptr;
			const size_t numGrids = isGridsPtrValid ? GridsPtr->size() : 0;
			const size_t numPreviousGrids = isPreviousGridsPtrValid ? previousGridsPtr->size() : 0;
			
			//TODO: Ability to check for changed grid without finding the grid in every type of mesh op
			//Initially do the easy check for change in number of grids. If the same, then check for changed mesh op. TODO: Check flags instead?
			bool isAnyGridChanged = numGrids != numPreviousGrids;
			for (auto i = GridsPtr->cbegin(); i != GridsPtr->cend() && !isAnyGridChanged; ++i)
			{
				const FString gridName = UTF8_TO_TCHAR((*i)->getName().c_str());
				auto cubesMeshOp = CubesMeshOps.Find(gridName);
				check(cubesMeshOp);
				auto marchingCubesMeshOp = MarchingCubesMeshOps.Find(gridName);
				check(marchingCubesMeshOp);
				//TODO: Need to worry about case when the grid does not have a mesh op?
				check(cubesMeshOp != nullptr);
				check(marchingCubesMeshOp != nullptr);
				if ((*cubesMeshOp)->isChanged || (*marchingCubesMeshOp)->isChanged)
				{
					isAnyGridChanged = true;
				}
			}

			//Write the pending changes
			if (isFileMetaChanged || isAnyGridChanged)
			{
				CloseFileGuard(); //openvdb::io::File must be closed in order to write
								  //openvdb::io::File can write only grids or both grids and file meta but can't write only file meta
				if (isAnyGridChanged && FileMetaPtr == nullptr)
				{
					check(GridsPtr);
					UE_LOG(LogOpenVDBModule, Display, TEXT("Writing pending grid changes to [%s]"), UTF8_TO_TCHAR(FilePtr->filename().c_str()));
					FilePtr->write(*GridsPtr);
				}
				else
				{
					check(GridsPtr);
					check(FileMetaPtr);
					UE_LOG(LogOpenVDBModule, Display, TEXT("Writing pending grid and file meta changes to [%s]"), UTF8_TO_TCHAR(FilePtr->filename().c_str()));
					FilePtr->write(*GridsPtr, *FileMetaPtr);
				}
				FilePtr.Reset();
			}
		}
		return true;
	}

	inline bool CreateOrVerifyDatabase(const FString &filePath,
		const bool enableGridStats,
		const bool enableDelayLoad,
		const bool discardUnwrittenChanges = false,
		const bool forceOverwriteExistingFile = false)
	{
		if (!SetAndVerifyConfiguration(filePath, enableGridStats, enableDelayLoad, discardUnwrittenChanges, forceOverwriteExistingFile))
		{
			UE_LOG(LogOpenVDBModule, Error, TEXT("Failed to configure voxel database [%s] grid_stats=%d, delay_load=%d, discard_unwritten=%d, overwrite_existing_file=%d"), *filePath, enableGridStats, enableDelayLoad, discardUnwrittenChanges, forceOverwriteExistingFile);
			return false;
		}

		const FString PreviousFilePath = UTF8_TO_TCHAR(FilePtr->filename().c_str());
		EnableGridStats = enableGridStats;
		EnableDelayLoad = enableDelayLoad;
		UE_LOG(LogOpenVDBModule, Display, TEXT("Configuring voxel database [%s] grid_stats=%d, delay_load=%d, discard_unwritten=%d, overwrite_existing_file=%d"), *filePath, enableGridStats, enableDelayLoad, discardUnwrittenChanges, forceOverwriteExistingFile);

		if (PreviousFilePath != filePath)
		{
			UE_LOG(LogOpenVDBModule, Display, TEXT("Changing voxel database path from [%s] to [%s]"), *PreviousFilePath, *filePath);
			FilePtr = TSharedPtr<openvdb::io::File>(new openvdb::io::File(TCHAR_TO_UTF8(*(filePath))));
			check(FilePtr.IsValid());
		}
		check(FilePtr->filename() == std::string(TCHAR_TO_UTF8(*filePath)));
		FilePtr->setGridStatsMetadataEnabled(EnableGridStats);

		//TODO: Ensure path exists before writing file?
		if (!FPaths::FileExists(filePath))
		{
			//Create an empty vdb file
			UE_LOG(LogOpenVDBModule, Display, TEXT("Creating voxel database [%s]"), *filePath);
			FilePtr->write(openvdb::GridCPtrVec(), openvdb::MetaMap());
		}
		check(FPaths::FileExists(filePath)); //TODO: Error handling when unable to create file. For now assume the file exists

		//Initially read only file-level metadata and metadata for each grid (do not read tree data values for grids yet)
		OpenFileGuard();
		UE_LOG(LogOpenVDBModule, Display, TEXT("Reading grid metadata and file metadata from voxel database [%s]"), *filePath);
		GridsPtr = FilePtr->readAllGridMetadata(); //grid-level metadata for all grids
		FileMetaPtr = FilePtr->getMetadata(); //file-level metadata
		CachedGrid = GridsPtr->end();
		CubesMeshOps.Empty();
		MarchingCubesMeshOps.Empty();

		//Start in a clean state
		SyncGridsAndFileMetaStatus();
		return true;
	}

	inline void GetFirstActiveCoord(GridType &grid, const openvdb::CoordBBox &activeIndexBBox, openvdb::Coord &firstActive)
	{
		check(!activeIndexBBox.empty());
		//Start from the center x,y and highest z of the active bounding box and return immediately when an active voxel is found.
		const openvdb::Coord &min = activeIndexBBox.min();
		const openvdb::Coord &max = activeIndexBBox.max();
		const openvdb::Coord centerCoord(max.x()-(activeIndexBBox.dim().x()/2), max.y()-(activeIndexBBox.dim().y()/2), max.z());
		GridType::ConstAccessor acc = grid.getConstAccessor();
		if (acc.isValueOn(centerCoord))
		{
			firstActive = centerCoord;
			return;
		}

		TArray<int32> coordsChecked;
		coordsChecked.SetNumZeroed(activeIndexBBox.volume());

		for (int32 dx = 0; dx < centerCoord.x(); ++dx)
		{
			for (int32 dy = 0; dy < centerCoord.y(); ++dy)
			{
				for (int32 dz = 0; dz < activeIndexBBox.dim().z(); ++dz)
				{
					const openvdb::Coord first = centerCoord.offsetBy(dx, dy, -dz);
					const int32 firstidx = (first.x()-min.x()) + activeIndexBBox.dim().x() * ((first.y()-min.y()) + activeIndexBBox.dim().y() * (first.z()-min.z()));
					check(coordsChecked[firstidx] == 0);
					coordsChecked[firstidx] = 1;
					if (acc.isValueOn(first))
					{
						firstActive = first;
						return;
					}
					const openvdb::Coord second = centerCoord.offsetBy(-dx, -dy, -dz);
					if (second == first)
					{
						continue;
					}
					const int32 secondidx = (second.x()-min.x()) + activeIndexBBox.dim().x() * ((second.y()-min.y()) + activeIndexBBox.dim().y() * (second.z()-min.z()));
					check(coordsChecked[secondidx] == 0);
					coordsChecked[secondidx] = 1;
					if (acc.isValueOn(second))
					{
						firstActive = second;
						return;
					}
				}
			}
		}
		//Should never get here since as a precondition the active bounding box is not empty!
		check(false);
	}
};

typedef VdbHandlePrivate<TreeType, Vdb::GridOps::IndexTreeType, openvdb::math::ScaleMap> VdbHandlePrivateType;

struct AsyncIONotifier //TODO: Add critical section?
{
	AsyncIONotifier(VdbHandlePrivateType &vdb)
		: VoxelDatabase(vdb)
	{
		IsAsyncIORunning = false;
		IsFinalChanges = false;
	}

	typedef tbb::concurrent_hash_map<openvdb::io::Queue::Id, std::string> FilenameMap;
	bool IsAsyncIORunning;
	bool IsFinalChanges;
	FilenameMap filenames;
	VdbHandlePrivateType &VoxelDatabase;
	openvdb::GridPtrVec OutGrids;

	void Clear()
	{
		check(!IsAsyncIORunning);
		OutGrids.clear();
	}

	// Callback function that prints the status of a completed task.
	void Callback(openvdb::io::Queue::Id id, openvdb::io::Queue::Status status)
	{
		const bool succeeded = (status == openvdb::io::Queue::SUCCEEDED);
		FilenameMap::accessor acc;
		if (filenames.find(acc, id))
		{
			if (succeeded)
			{
				UE_LOG(LogOpenVDBModule, Verbose, TEXT("AsyncIONotifier: Wrote [%s]"), UTF8_TO_TCHAR(acc->second.c_str()));
				VoxelDatabase.SyncFileMetaStatus();
				if (IsFinalChanges)
				{
					VoxelDatabase.ResetSharedResources();
				}
			}
			else
			{
				UE_LOG(LogOpenVDBModule, Verbose, TEXT("AsyncIONotifier: Failed to write [%s]"), UTF8_TO_TCHAR(acc->second.c_str()));
			}
			filenames.erase(acc);
		}
		IsAsyncIORunning = false;
	}
};