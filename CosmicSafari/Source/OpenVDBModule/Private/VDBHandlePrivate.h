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
struct GridNameIs;
typedef struct AsyncIONotifier AsyncIONotifierType;
typedef struct GridNameIs GridNameIsType;

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
	AsyncIONotifierType &AsyncIO;
	friend struct AsyncIOJob;
	openvdb::io::Queue::Id AsyncIOJobID;

public:

	VdbHandlePrivate(AsyncIONotifierType &asyncIO) :
		EnableGridStats(false),
		EnableDelayLoad(false),
		AreAllGridsInSync(true),
		IsFileMetaInSync(true),
		AsyncIO(asyncIO),
		AsyncIOJobID(INT32_MAX)
	{
	}

	~VdbHandlePrivate()
	{
		WriteChangesAsync(true);
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

	bool GetFilename(FString &filename)
	{
		//Return the file's path and if it is valid
		bool isFileValid = false;
		if (FilePtr.IsValid())
		{
			filename = UTF8_TO_TCHAR(FilePtr->filename().c_str());
			isFileValid = true;
		}
		return isFileValid;
	}

	bool GetFilename(std::string &filename)
	{
		//Return the file's path and if it is valid
		bool isFileValid = false;
		if (FilePtr.IsValid())
		{
			filename = FilePtr->filename();
			isFileValid = true;
		}
		return isFileValid;
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

	inline GridType& GetGrid(const FString &gridID)
	{
		bool isNewGrid = false; //not used
		return GetOrCreateGrid(gridID, isNewGrid);
	}

	inline GridType& GetOrCreateGrid(const FString &gridID, bool &isNewGrid, GridTypePtr gridPtr = nullptr)
	{
		//Find the grid using the cached iterator. If not found create a new grid and point the cached iterator to it.
		const std::string gridIDStd = TCHAR_TO_UTF8(*gridID);
		isNewGrid = false;
		//TODO: Maybe don't need the cached iterator. Should do performance testing
		CachedGrid = std::find_if(GridsPtr->begin(), GridsPtr->end(), GridNameIsType(gridIDStd));
		if (CachedGrid == GridsPtr->end())
		{
			//Grid not found - create and cache it
			UE_LOG(LogOpenVDBModule, Display, TEXT("Creating grid [%s]"), *gridID);
			gridPtr = GridType::create();
			gridPtr->setName(TCHAR_TO_UTF8(*gridID));
			GridsPtr->push_back(gridPtr);
			CachedGrid = GridsPtr->end();
			CachedGrid--;
			check(CachedGrid != GridsPtr->end());
			isNewGrid = true;
		}
		else
		{
			//Grid found
			UE_LOG(LogOpenVDBModule, Verbose, TEXT("Found existing grid [%s]"), *gridID);
			gridPtr = openvdb::gridPtrCast<GridType>(*CachedGrid);
		}
		check(gridPtr != nullptr);
		return *gridPtr;
	}

	void AddGrid(const FString &gridID,
		const FIntVector &gridDimensions,
		const FVector &worldOffset,
		const FVector &voxelSize,
		TArray<FProcMeshSection> &sectionBuffers)
	{
		//All grids are indexed from [0, size of dimension - 1] along x,y,z dimensions.
		//World coordinates are given by the scale factor translated by the world-space offset.
		const openvdb::Vec3d voxelScale(voxelSize.X, voxelSize.Y, voxelSize.Z);
		const openvdb::Vec3d worldOffsetVec3d(worldOffset.X, worldOffset.Y, worldOffset.Z);
		const openvdb::math::ScaleTranslateMap::Ptr map = openvdb::math::ScaleTranslateMap::Ptr(new openvdb::math::ScaleTranslateMap(voxelScale, worldOffsetVec3d));
		const openvdb::math::Transform::Ptr xformPtr = openvdb::math::Transform::Ptr(new openvdb::math::Transform(map));

		//The file is flagged as needing to be written if any or all of:
		//The grid has been created as new.
		//The grid's transform has changed (in which case the resulting mesh will probably have a different appearance so treat the grid as new).
		//The grid's start or ending index coords have changed (NOTE and TODO: the logic was changed so that all grids start indexed from 0,0,0!)

		//Grid created as new or transform is changed?
		bool isNewGrid = false;
		GridType &grid = GetOrCreateGrid(gridID, isNewGrid);
		if (isNewGrid || grid.transform() != *xformPtr)
		{
			UE_LOG(LogOpenVDBModule, Verbose, TEXT("Setting grid [%s] transform to [%s]"), UTF8_TO_TCHAR(grid.getName().c_str()), UTF8_TO_TCHAR(map->str().c_str()));
			grid.setTransform(xformPtr);
			CubesMeshOps.Emplace(gridID, TSharedRef<Vdb::GridOps::CubeMesher<GridTreeType>>(new Vdb::GridOps::CubeMesher<GridTreeType>(grid, sectionBuffers)));
			MarchingCubesMeshOps.Emplace(gridID, TSharedRef<Vdb::GridOps::MarchingCubesMesher<GridTreeType>>(new Vdb::GridOps::MarchingCubesMesher<GridTreeType>(grid, sectionBuffers)));
			DesyncGridsStatus();
		}

		//Grid start index has changed or start index meta value does not exist?
		//TODO: Update logic to not use MetaName_RegionIndexStart since now all grids start from 0,0,0
		const openvdb::Vec3IMetadata::Ptr currentIndexStartMeta = grid.getMetadata<openvdb::Vec3IMetadata>(TCHAR_TO_UTF8(*MetaName_RegionIndexStart()));
		const openvdb::Vec3i indexStartVec(0, 0, 0);
		const openvdb::Vec3d worldStart = map->applyMap(openvdb::Vec3d(indexStartVec.x(), indexStartVec.y(), indexStartVec.z()));
		if (currentIndexStartMeta == nullptr || !openvdb::math::isExactlyEqual(currentIndexStartMeta->value(), indexStartVec))
		{
			UE_LOG(LogOpenVDBModule, Verbose, TEXT("Setting grid [%s] start to [%s] (index [%s])"), *gridID, UTF8_TO_TCHAR(worldStart.str().c_str()), UTF8_TO_TCHAR(indexStartVec.str().c_str()));
			grid.insertMeta(TCHAR_TO_UTF8(*MetaName_RegionIndexStart()), openvdb::Vec3IMetadata(indexStartVec));
			grid.insertMeta(TCHAR_TO_UTF8(*MetaName_RegionStart()), openvdb::Vec3DMetadata(worldStart));
			DesyncGridsStatus();
		}

		//Grid end index has changed or end index meta value does not exist?
		//TODO: Update logic to not use MetaName_RegionIndexStart since now all grids start from 0,0,0
		const openvdb::Vec3IMetadata::Ptr currentIndexEndMeta = grid.getMetadata<openvdb::Vec3IMetadata>(TCHAR_TO_UTF8(*MetaName_RegionIndexEnd()));
		const openvdb::Vec3i indexEndVec(gridDimensions.X, gridDimensions.Y, gridDimensions.Z);
		const openvdb::Vec3d worldEnd = map->applyMap(openvdb::Vec3d(indexEndVec.x(), indexEndVec.y(), indexEndVec.z()));
		if (currentIndexEndMeta == nullptr || !openvdb::math::isExactlyEqual(currentIndexEndMeta->value(), indexEndVec))
		{
			UE_LOG(LogOpenVDBModule, Verbose, TEXT("Setting grid [%s] end to [%s] (index [%s])"), *gridID, UTF8_TO_TCHAR(worldEnd.str().c_str()), UTF8_TO_TCHAR(indexEndVec.str().c_str()));
			grid.insertMeta(TCHAR_TO_UTF8(*MetaName_RegionIndexEnd()), openvdb::Vec3IMetadata(indexEndVec));
			grid.insertMeta(TCHAR_TO_UTF8(*MetaName_RegionEnd()), openvdb::Vec3DMetadata(worldEnd));
			DesyncGridsStatus();
		}
	}

	GridType& ReadGridTree(const FString &gridID, FIntVector &indexStart, FIntVector &indexEnd)
	{
		//Only try to read the data tree from file if the current grid has no active voxels
		//TODO: Handle case when a grid has 0 active voxels legitimately?
		GridType &grid = GetGrid(gridID);
		const openvdb::Index64 activeVoxelCount = grid.activeVoxelCount();
		if (activeVoxelCount == 0)
		{
			UE_LOG(LogOpenVDBModule, Verbose, TEXT("Grid [%s] tree is empty"), *gridID);
			if (FilePtr->hasGrid(TCHAR_TO_UTF8(*gridID)))
			{
				//Read the tree from file then swap the current grid tree to the tree from file
				UE_LOG(LogOpenVDBModule, Verbose, TEXT("Reading grid [%s] tree from file"), *gridID);
				OpenFileGuard();
				GridTypePtr activeGrid = openvdb::gridPtrCast<GridType>(FilePtr->readGrid(TCHAR_TO_UTF8(*gridID)));
				check(activeGrid != nullptr);
				check(activeGrid->treePtr() != nullptr);
				grid.setTree(activeGrid->treePtr());
			}
		}
		UE_LOG(LogOpenVDBModule, Verbose, TEXT("Grid [%s] tree has %d active voxels"), *gridID, activeVoxelCount);

		//Give the defined start/end indices for this grid
		//TODO: Update logic to not use MetaName_RegionIndexStart since now all grids start from 0,0,0
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
	bool GetFileMetaValue(const FString &metaName, FileMetaType &metaValue)
	{
		//Find and copy the file meta value and return if it was found
		bool isMetaFound = false;
		OpenFileGuard();
		openvdb::TypedMetadata<FileMetaType>::Ptr metaDataPtr = FileMetaPtr->getMetadata<openvdb::TypedMetadata<FileMetaType>>(TCHAR_TO_UTF8(*metaName));
		if (metaDataPtr != nullptr)
		{
			metaValue = metaDataPtr->value();
			isMetaFound = true;
		}
		else
		{
			UE_LOG(LogOpenVDBModule, Error, TEXT("Failed to find file meta [%s] in [%s]"), *metaName, UTF8_TO_TCHAR(FilePtr->filename().c_str()));
		}
		return isMetaFound;
	}

	template<typename FileMetaType>
	void InsertFileMeta(const FString &metaName, const FileMetaType &metaValue)
	{
		//Insert file meta and flag the file as changed if the value to be inserted is different or new
		CloseFileGuard();
		openvdb::TypedMetadata<FileMetaType>::Ptr currentFileMeta = FileMetaPtr->getMetadata<openvdb::TypedMetadata<FileMetaType>>(TCHAR_TO_UTF8(*metaName));
		UE_LOG(LogOpenVDBModule, Verbose, TEXT("Inserting file meta [%s] into [%s]"), *metaName, UTF8_TO_TCHAR(FilePtr->filename().c_str()));
		if (currentFileMeta == nullptr || !openvdb::math::isExactlyEqual(currentFileMeta->value(), metaValue))
		{
			FileMetaPtr->insertMeta(TCHAR_TO_UTF8(*metaName), openvdb::TypedMetadata<FileMetaType>(metaValue));
			DesyncFileMetaStatus();
		}
	}

	bool RemoveFileMeta(const FString &metaName)
	{
		//Find and remove the file meta and return if it was actually removed
		bool isMetaRemoved = false;
		CloseFileGuard();
		const std::string metaNameStd = TCHAR_TO_UTF8(*metaName);
		for (auto i = FileMetaPtr->beginMeta(); i != FileMetaPtr->endMeta(); ++i)
		{
			if (i->first == metaNameStd)
			{
				//Here we're iterating over the metamap twice because removeMeta() uses std::find. However, removeMeta() doesn't provide any
				//indication if the item to remove existed prior, which we need to know for setting the file status with DesyncFileMetaStatus().
				//TODO: Better way to find and remove?
				UE_LOG(LogOpenVDBModule, Display, TEXT("Removing file meta [%s] from [%s]"), *metaName, UTF8_TO_TCHAR(FilePtr->filename().c_str()));
				FileMetaPtr->removeMeta(metaNameStd);
				isMetaRemoved = true;
				DesyncFileMetaStatus();
			}
		}
		return isMetaRemoved;
	}

	bool RemoveGridFromGridVec(const FString &gridID)
	{
		//Find and remove the grid and return if it was actually removed
		bool isRemoved = false;
		const std::string gridIDStd = TCHAR_TO_UTF8(*gridID);
		auto i = std::find_if(GridsPtr->begin(), GridsPtr->end(), GridNameIsType(gridIDStd));
		if (i != GridsPtr->end())
		{
			UE_LOG(LogOpenVDBModule, Display, TEXT("Removing grid [%s]"), *gridID);
			CubesMeshOps.Remove(gridID);
			MarchingCubesMeshOps.Remove(gridID);
			i->reset();
			GridsPtr->erase(i);
			isRemoved = true;
			CachedGrid = GridsPtr->end();
			DesyncGridsStatus();
		}
		else
		{
			UE_LOG(LogOpenVDBModule, Error, TEXT("Could not find grid [%s] for removal"), *gridID);
		}
		return isRemoved;
	}

	template<typename GridMetaType>
	bool GetGridMetaValue(const FString &gridID, const FString &metaName, GridMetaType &metaValue)
	{
		//Find and copy the grid meta value and return if it was found
		bool isMetaFound = false;
		GridType &grid = GetGrid(gridID);
		openvdb::TypedMetadata<GridMetaType>::Ptr metaDataPtr = grid.getMetadata<openvdb::TypedMetadata<GridMetaType>>(TCHAR_TO_UTF8(*metaName));
		if (metaDataPtr != nullptr)
		{
			UE_LOG(LogOpenVDBModule, Verbose, TEXT("Getting grid meta [%s] from [%s]"), *metaName, *gridID);
			metaValue = metaDataPtr->value();
			isMetaFound = true;
		}
		else
		{
			UE_LOG(LogOpenVDBModule, Error, TEXT("Failed to find grid meta [%s] in [%s]"), *gridID, *metaName);
		}
		return isMetaFound;
	}

	template<typename GridMetaType>
	void InsertGridMeta(const FString &gridID, const FString &metaName, const GridMetaType &metaValue)
	{
		//Insert grid meta and flag the file as changed if the value to be inserted is different or new
		GridType &grid = GetGrid(gridID);
		openvdb::TypedMetadata<GridMetaType>::Ptr currentGridMeta = grid.getMetadata<openvdb::TypedMetadata<GridMetaType>>(TCHAR_TO_UTF8(*metaName));
		UE_LOG(LogOpenVDBModule, Verbose, TEXT("Inserting grid meta [%s] into [%s]"), *metaName, *gridID);
		if (currentGridMeta == nullptr || !openvdb::math::isExactlyEqual(currentGridMeta->value(), metaValue))
		{
			grid.insertMeta(TCHAR_TO_UTF8(*metaName), openvdb::TypedMetadata<GridMetaType>(metaValue));
			DesyncGridsStatus();
		}
	}

	template<typename GridMetaType>
	bool RemoveGridMeta(const FString &gridID, const FString &metaName)
	{
		//Find and remove the grid meta and return if it was actually removed
		bool isMetaRemoved = false;
		GridType &grid = GetGrid(gridID);
		const std::string metaNameStd = TCHAR_TO_UTF8(*metaName);
		for (auto i = grid.beginMeta(); i != grid.endMeta(); ++i)
		{
			if (i->first == metaNameStd)
			{
				//Here we're iterating over the metamap twice because removeMeta() uses std::find. However, removeMeta() doesn't provide any
				//indication if the item to remove existed prior, which we need to know for setting the file status with DesyncGridsStatus().
				//TODO: Better way to find and remove?
				UE_LOG(LogOpenVDBModule, Display, TEXT("Removing grid meta [%s] from [%s]"), *metaName, *gridID);
				grid.removeMeta(name);
				isMetaRemoved = true;
				DesyncGridsStatus();
			}
		}
		return isMetaRemoved;
	}

	bool WriteChanges(bool isFinal)
	{
		//Write any pending changes and return if the file actually changed
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
		if (AreChangesPending() && AsyncIOJobID == INT32_MAX)
		{
			isFileChanged = true; //TODO: Provide mechanism to wait on async changes to complete
			CloseFileGuard(); //openvdb::io::File must be closed in order to write
							  //openvdb::io::File can write only grids or both grids and file meta but can't write only file meta			
			//Clear the async IO grids and then deep copy all of the current grids to the async IO. TODO: Better way to do this than deep copying all? Critical section?
			AsyncIOJobID = AsyncIO.AddJob(this, isFinal); //TODO: Handle case when the same VDBHandlePrivate is being added to the async jobs
			if (isFinal)
			{
				UE_LOG(LogOpenVDBModule, Verbose, TEXT("Async changes to [%s] will be finalized"), UTF8_TO_TCHAR(FilePtr->filename().c_str()));
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
		UE_LOG(LogOpenVDBModule, Verbose, TEXT("[%s] %d active voxels"), UTF8_TO_TCHAR(grid.getName().c_str()), grid.activeVoxelCount());
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
		UE_LOG(LogOpenVDBModule, Verbose, TEXT("[%s] %d active voxels"), UTF8_TO_TCHAR(grid.getName().c_str()), grid.activeVoxelCount());
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
		UE_LOG(LogOpenVDBModule, Verbose, TEXT("%s"), *msg);
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
		UE_LOG(LogOpenVDBModule, Display, TEXT("Resetting all shared resources for [%s]"), UTF8_TO_TCHAR(FilePtr->filename().c_str()));
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
		const bool arePreviousChangesPending = FilePtr.IsValid() && AreChangesPending();
		const bool doesFileExist = FPaths::FileExists(filePath);
		const bool canDiscardAnyPending = !arePreviousChangesPending || discardUnwrittenChanges;
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
		if (FilePtr.IsValid())
		{
			if (discardUnwrittenChanges)
			{
				//Ignore unwritten changes - just ensure the file is closed
				CloseFileGuard();
			}
			else
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
				}
			}
			//Any existing unwritten changes are written to file so we're done with the file
			FilePtr.Reset();
		}
		return true;
	}

	inline bool CreateOrVerifyDatabase(const FString &filePath,
		const bool enableGridStats,
		const bool enableDelayLoad,
		const bool discardUnwrittenChanges = false,
		const bool forceOverwriteExistingFile = false)
	{
		const FString PreviousFilePath = FilePtr.IsValid() ? UTF8_TO_TCHAR(FilePtr->filename().c_str()) : filePath;
		const bool PreviousEnableGridStats = FilePtr.IsValid() ? FilePtr->isGridStatsMetadataEnabled() : enableGridStats;
		const bool PreviousEnableDelayLoad = FilePtr.IsValid() ? FilePtr->isDelayedLoadingEnabled() : enableDelayLoad;
		if (!SetAndVerifyConfiguration(filePath, enableGridStats, enableDelayLoad, discardUnwrittenChanges, forceOverwriteExistingFile))
		{
			UE_LOG(LogOpenVDBModule, Fatal, TEXT("Failed to configure voxel database [%s] grid_stats=%d, delay_load=%d, discard_unwritten=%d, overwrite_existing_file=%d"), *filePath, enableGridStats, enableDelayLoad, discardUnwrittenChanges, forceOverwriteExistingFile);
			return false;
		}
		check(!FilePtr.IsValid());

		UE_LOG(LogOpenVDBModule, Display, TEXT("Configuring voxel database [%s] grid_stats=%d, delay_load=%d, discard_unwritten=%d, overwrite_existing_file=%d"), *filePath, enableGridStats, enableDelayLoad, discardUnwrittenChanges, forceOverwriteExistingFile);
		EnableGridStats = enableGridStats;
		EnableDelayLoad = enableDelayLoad;

		UE_LOG(LogOpenVDBModule, Display, TEXT("Changing voxel database path and config from [%s] (DL%d GS%d) to [%s] (DL%d GS%d)"), *PreviousFilePath, PreviousEnableGridStats, PreviousEnableDelayLoad, *filePath, enableGridStats, enableDelayLoad);
		const std::string filepathStd = TCHAR_TO_UTF8(*filePath);
		FilePtr = TSharedPtr<openvdb::io::File>(new openvdb::io::File(filepathStd));
		check(FilePtr.IsValid());
		check(FilePtr->filename() == filepathStd);
		FilePtr->setGridStatsMetadataEnabled(EnableGridStats);

		//TODO: Ensure path exists before writing file?
		if (!FPaths::FileExists(filePath))
		{
			//Create an empty vdb file
			UE_LOG(LogOpenVDBModule, Display, TEXT("Creating empty voxel database [%s]"), *filePath);
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

struct AsyncIOJob
{
	AsyncIOJob(openvdb::io::Queue::Id jobID, VdbHandlePrivateType * vbdHandlePrivatePtr, bool isFinal)
		: JobID(jobID), Database(TSharedPtr<VdbHandlePrivateType>(vbdHandlePrivatePtr)), IsFinal(IsFinal), IsStarted(false), IsFinished(false)
	{
		check(Database.IsValid());
		check(Database->GridsPtr);
		check(Database->FilePtr.IsValid());
		UE_LOG(LogOpenVDBModule, Verbose, TEXT("AsyncIOJob_%d created"), JobID);
	}

	bool StartJob(openvdb::io::Queue &queue)
	{
		bool isComplete = false;
		check(Database.IsValid());
		check(Database->GridsPtr);
		check(Database->FilePtr.IsValid());
		check(OutGrids.empty());
		check(Database->AsyncIOJobID == JobID);
		UE_LOG(LogOpenVDBModule, Verbose, TEXT("AsyncIOJob_%d started"), JobID);

		try
		{
			//TODO: Mutex on Database while copying grids, meta, and file?
			for (auto i = Database->GridsPtr->begin(); i != Database->GridsPtr->end(); ++i)
			{
				OutGrids.push_back((*i)->deepCopyGrid());
			}

			if (Database->FileMetaPtr)
			{
				UE_LOG(LogOpenVDBModule, Display, TEXT("AsyncIOJob_%d: Queuing pending grid and file meta changes for async write to [%s]"), JobID, UTF8_TO_TCHAR(Database->FilePtr->filename().c_str()));
				queue.write(OutGrids, *(Database->FilePtr->copy()), *(Database->FileMetaPtr->deepCopyMeta()));
			}
			else
			{
				UE_LOG(LogOpenVDBModule, Display, TEXT("AsyncIOJob_%d: Queuing pending grid changes for async write to [%s]"), JobID, UTF8_TO_TCHAR(Database->FilePtr->filename().c_str()));
				queue.write(OutGrids, *(Database->FilePtr->copy()), openvdb::MetaMap());
			}
			IsStarted = true;
			isComplete = true;
		}
		catch (const openvdb::RuntimeError &e)
		{
			UE_LOG(LogOpenVDBModule, Error, TEXT("AsyncIOJob_%d: OpenVDB runtime error: %s"), JobID, UTF8_TO_TCHAR(e.what()));
		}
		catch (const openvdb::Exception &e)
		{
			UE_LOG(LogOpenVDBModule, Error, TEXT("AsyncIOJob_%d: OpenVDB exception: %s"), JobID, UTF8_TO_TCHAR(e.what()));
		}
		return isComplete;
	}

	bool FinishCleanup()
	{
		check(Database.IsValid());
		check(Database->AsyncIOJobID == JobID);
		bool isComplete = false;
		IsFinished = false;
		Database->AsyncIOJobID = INT32_MAX;
		UE_LOG(LogOpenVDBModule, Verbose, TEXT("AsyncIOJob_%d cleanup (success)"), JobID);

		try
		{
			Database->SyncGridsAndFileMetaStatus();
			const std::string filename = Database->FilePtr->filename();
			if (IsFinal)
			{
				UE_LOG(LogOpenVDBModule, Display, TEXT("AsyncIOJob_%d: Resetting shared resources of [%s]"), JobID, UTF8_TO_TCHAR(Database->FilePtr->filename().c_str()));
				Database->ResetSharedResources();
			}
			IsStarted = false;
			IsFinished = true;
			UE_LOG(LogOpenVDBModule, Display, TEXT("AsyncIOJob_%d: Successfully wrote [%s]"), JobID, UTF8_TO_TCHAR(filename.c_str()));
			isComplete = true;
		}
		catch (const openvdb::RuntimeError &e)
		{
			UE_LOG(LogOpenVDBModule, Error, TEXT("AsyncIOJob_%d: OpenVDB runtime error: %s"), JobID, UTF8_TO_TCHAR(e.what()));
		}
		catch (const openvdb::Exception &e)
		{
			UE_LOG(LogOpenVDBModule, Error, TEXT("AsyncIOJob_%d: OpenVDB exception: %s"), JobID, UTF8_TO_TCHAR(e.what()));
		}
		return isComplete;
	}

	bool FailCleanup()
	{
		check(Database.IsValid());
		check(Database->AsyncIOJobID == JobID);
		bool isComplete = false;
		IsFinished = false;
		Database->AsyncIOJobID = INT32_MAX;
		UE_LOG(LogOpenVDBModule, Verbose, TEXT("AsyncIOJob_%d cleanup (failure)"), JobID);

		try
		{
			Database->DesyncGridsAndFileMetaStatus();
			IsStarted = false;
			UE_LOG(LogOpenVDBModule, Display, TEXT("AsyncIOJob_%d: Failed to write [%s]"), JobID, UTF8_TO_TCHAR(Database->FilePtr->filename().c_str()));
			isComplete = true;
		}
		catch (const openvdb::RuntimeError &e)
		{
			UE_LOG(LogOpenVDBModule, Error, TEXT("AsyncIOJob_%d: OpenVDB runtime error: %s"), JobID, UTF8_TO_TCHAR(e.what()));
		}
		catch (const openvdb::Exception &e)
		{
			UE_LOG(LogOpenVDBModule, Error, TEXT("AsyncIOJob_%d: OpenVDB exception: %s"), JobID, UTF8_TO_TCHAR(e.what()));
		}
		return isComplete;
	}

	const openvdb::io::Queue::Id JobID;
	TSharedPtr<VdbHandlePrivateType> Database;
	bool IsStarted;
	bool IsFinished;
	const bool IsFinal;
	openvdb::GridPtrVec OutGrids;
};

struct AsyncIONotifier
{
	typedef tbb::concurrent_hash_map<openvdb::io::Queue::Id, AsyncIOJob> AsyncJobs;
	AsyncJobs IOJobs;
	openvdb::io::Queue IOQueue;

	AsyncIONotifier()
	{
		IOQueue.addNotifier(boost::bind(&AsyncIONotifier::Callback, this, _1, _2));
	}

	openvdb::io::Queue::Id AddJob(VdbHandlePrivateType * vdbHandlePrivatePtr, bool isFinal)
	{
		static openvdb::io::Queue::Id CurrentID = INT32_MIN;
		check(vdbHandlePrivatePtr);
		const openvdb::io::Queue::Id jobID = CurrentID++;
		check(jobID != INT32_MAX);
		IOJobs.insert(std::pair<openvdb::io::Queue::Id, AsyncIOJob>(jobID, AsyncIOJob(jobID, vdbHandlePrivatePtr, isFinal)));
		return jobID;
	}

	bool StartJob(openvdb::io::Queue::Id id)
	{
		AsyncJobs::accessor acc;
		bool isFound = IOJobs.find(acc, id);
		if (isFound)
		{
			acc->second.StartJob(IOQueue);
		}
		return isFound;
	}

	// Callback function called when an AsyncIOJob is completed.
	void Callback(openvdb::io::Queue::Id id, openvdb::io::Queue::Status status)
	{
		const bool succeeded = (status == openvdb::io::Queue::SUCCEEDED);
		AsyncJobs::accessor acc;
		if (IOJobs.find(acc, id))
		{
			if (succeeded)
			{
				if (!acc->second.FinishCleanup())
				{
					UE_LOG(LogOpenVDBModule, Error, TEXT("AsyncIONotifier: AsyncIOJob_%d failed to cleanup"), id);
				}
			}
			else
			{
				UE_LOG(LogOpenVDBModule, Error, TEXT("AsyncIONotifier: AsyncIOJob_%d unsucessful...attempting to cleanup"), id);
				if(!acc->second.FailCleanup())
				{
					UE_LOG(LogOpenVDBModule, Error, TEXT("AsyncIONotifier: AsyncIOJob_%d failed to cleanup"), id);
				}
			}
			IOJobs.erase(acc);
		}
	}
};

struct GridNameIs
{
	GridNameIs(const std::string &nameToFind) : NameToFind(nameToFind) {}
	inline bool operator()(const openvdb::GridBase::Ptr &gridPtr)
	{
		return gridPtr->getName() == NameToFind;
	}
	const std::string &NameToFind;
};