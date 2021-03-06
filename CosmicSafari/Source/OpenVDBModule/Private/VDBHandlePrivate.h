#pragma once
#include "OpenVDBModule.h"
#pragma warning(push)
#pragma warning(1:4211 4800 4503 4146)
#include <openvdb/openvdb.h>
#include <openvdb/io/Queue.h>
#include <openvdb/io/Stream.h>
#include <openvdb/tools/Prune.h>
#include <boost/interprocess/file_mapping.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/stream.hpp>
#include "GridOps.h"
#include "GridMetadata.h"

PRAGMA_DISABLE_OPTIMIZATION

DECLARE_LOG_CATEGORY_EXTERN(LogOpenVDBModule, Log, All)

//5,4,3 is the standard openvdb tree configuration
typedef openvdb::tree::Tree4<FVoxelData, 5, 4, 3>::Type TreeType;
struct AsyncIONotifier;
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
	const FString HandleName;
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
	openvdb::io::Queue::Id RunningJobID;
	friend struct AsyncIOJob;

public:

	VdbHandlePrivate(const FString &uniqueName, AsyncIONotifierType &asyncIO) :
		HandleName(uniqueName),
		EnableGridStats(false),
		EnableDelayLoad(false),
		AreAllGridsInSync(true),
		IsFileMetaInSync(true),
		AsyncIO(asyncIO)
	{
	}

	~VdbHandlePrivate()
	{
		if (AreChangesPending())
		{
			AsyncIO.StartNextJob(RunningJobID);
		}
		ResetSharedResources();

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

	bool GetDatabasePath(FString &FilePath)
	{
		//Return the path to the vdb file and if the file is valid
		bool isFileValid = false;
		if (FilePtr.IsValid())
		{
			FilePath = UTF8_TO_TCHAR(FilePtr->filename().c_str());
			isFileValid = FPaths::FileExists(FilePath);
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

	bool InitializeDatabase(const FString &validatedFullPathName, const bool &enableGridStats, const bool &enableDelayLoad)
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

		const bool isInitialized = CreateAndVerifyDatabase(validatedFullPathName, enableGridStats, enableDelayLoad);
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
		check(GridsPtr != nullptr);
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
		check(GridsPtr != nullptr);
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
		check(GridsPtr != nullptr);
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
		check(FileMetaPtr != nullptr);
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
		check(FileMetaPtr != nullptr);
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
		check(FileMetaPtr != nullptr);
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
		check(GridsPtr != nullptr);
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
		check(GridsPtr != nullptr);
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
		check(GridsPtr != nullptr);
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
		check(GridsPtr != nullptr);
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

	bool FillGrid_PerlinDensity(const FString &gridName, bool threaded, const FIntVector &fillIndexStart, const FIntVector &fillIndexEnd, int32 seed, float frequency, float lacunarity, float persistence, int32 octaveCount)
	{
		check(GridsPtr != nullptr);
		bool isChanged = false;
		GridType &grid = GetGrid(gridName);

		//Noise module parameters are at the grid-level metadata TODO: Should they be at file level?
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
		check(GridsPtr != nullptr);
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
		check(GridsPtr != nullptr);
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
		check(GridsPtr != nullptr);
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
		check(GridsPtr != nullptr);
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
		check(GridsPtr != nullptr);
		GridType &grid = GetGrid(gridName);
		UE_LOG(LogOpenVDBModule, Verbose, TEXT("Running basic cubes mesher for [%s]"), UTF8_TO_TCHAR(grid.getName().c_str()));
		const bool threaded = true;
		check(CubesMeshOps.Contains(gridName));
		CubesMeshOps[gridName]->doMeshOp(threaded);
	}

	void MeshRegionMarchingCubes(const FString &gridName)
	{
		check(GridsPtr != nullptr);
		GridType &grid = GetGrid(gridName);
		UE_LOG(LogOpenVDBModule, Verbose, TEXT("Running marching cubes mesher for [%s]"), UTF8_TO_TCHAR(grid.getName().c_str()));
		const bool threaded = true;
		check(MarchingCubesMeshOps.Contains(gridName));
		MarchingCubesMeshOps[gridName]->doMeshOp(threaded);
	}

	void GetAllGridIDs(TArray<FString> &OutGridIDs) const
	{
		check(GridsPtr != nullptr);
		for (auto i = GridsPtr->begin(); i != GridsPtr->end(); ++i)
		{
			OutGridIDs.Add(FString(UTF8_TO_TCHAR((*i)->getName().c_str())));
		}
	}

	bool GetGridDimensions(const FString &gridName, FVector &startLocation)
	{
		check(GridsPtr != nullptr);
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
		check(GridsPtr != nullptr);
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
		check(GridsPtr != nullptr);
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
		check(GridsPtr != nullptr);
		GridType &grid = GetGrid(gridName);
		//worldToIndex returns a Vec3d, so floor it to get the index coords
		const openvdb::Coord coord = openvdb::Coord::floor(grid.worldToIndex(openvdb::Vec3d(location.X, location.Y, location.Z)));
		outIndexCoord.X = coord.x(); //TODO: Create the class FIntBox
		outIndexCoord.Y = coord.y();
		outIndexCoord.Z = coord.z();
	}

	inline void GetVoxelValue(const FString &gridName, const FIntVector &indexCoord, typename GridType::ValueType &outValue)
	{
		check(GridsPtr != nullptr);
		GridType &grid = GetGrid(gridName);
		const openvdb::Coord coord(indexCoord.X, indexCoord.Y, indexCoord.Z);
		GridType::ConstAccessor cacc = grid.getConstAccessor();
		outValue = cacc.getValue(coord);
	}

	inline bool GetVoxelActiveState(const FString &gridName, const FIntVector &indexCoord)
	{
		check(GridsPtr != nullptr);
		GridType &grid = GetGrid(gridName);
		const openvdb::Coord coord(indexCoord.X, indexCoord.Y, indexCoord.Z);
		GridType::ConstAccessor cacc = grid.getConstAccessor();
		return cacc.isValueOn(coord);
	}

	inline bool GetVoxelValueAndActiveState(const FString &gridName, const FIntVector &indexCoord, typename GridType::ValueType &outValue)
	{
		check(GridsPtr != nullptr);
		GridType &grid = GetGrid(gridName);
		const openvdb::Coord coord(indexCoord.X, indexCoord.Y, indexCoord.Z);
		GridType::ConstAccessor cacc = grid.getConstAccessor();
		outValue = cacc.getValue(coord);
		return cacc.isValueOn(coord);
	}

	inline void SetVoxelValue(const FString &gridName, const FIntVector &indexCoord, const typename GridType::ValueType &value)
	{
		check(GridsPtr != nullptr);
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
		check(GridsPtr != nullptr);
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
		check(GridsPtr != nullptr);
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

	inline openvdb::io::Queue::Id GetRunningJobID()
	{
		return RunningJobID;
	}

	inline bool IsWriteJobRunning()
	{
		return RunningJobID != UINT32_MAX;
	}

	inline openvdb::io::Queue::Id AddAsyncWriteJob(bool isFinal)
	{
		const openvdb::io::Queue::Id jobID = AsyncIO.AddJob(this, isFinal);
		//check(AsyncIO.IOJobs.size() > 0); TODO
		return jobID;
	}

	inline TFunction<void(void)>&& GetWriteChangesAsyncTimerCallback(openvdb::io::Queue::Id jobID)
	{
		return [&]() {
			AsyncIO.StartNextJob(jobID);
		};
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

private:

	inline bool AllocateFileResource(const std::string &pathname)
	{
		bool isAllocated = false;
		FilePtr = TSharedPtr<openvdb::io::File>(new openvdb::io::File(pathname));
		if (!FilePtr.IsValid())
		{
			isAllocated = true;
			UE_LOG(LogOpenVDBModule, Error, TEXT("%s: Failed to allocate file"), *HandleName)
		}
		return isAllocated;
	}

	inline bool AllocateGridsResource()
	{
		bool isAllocated = false;
		if (FilePtr.IsValid())
		{
			try
			{
				GridsPtr = FilePtr->readAllGridMetadata();
				if (GridsPtr != nullptr)
				{
					isAllocated = true;
				}
			}
			catch (openvdb::IoError &e) { UE_LOG(LogOpenVDBModule, Error, TEXT("%s: %s"), *HandleName, UTF8_TO_TCHAR(e.what())); }
			catch (openvdb::Exception &e) { UE_LOG(LogOpenVDBModule, Error, TEXT("%s: %s"), *HandleName, UTF8_TO_TCHAR(e.what())); }
			catch (std::bad_alloc &e) { UE_LOG(LogOpenVDBModule, Error, TEXT("%s: %s"), *HandleName, UTF8_TO_TCHAR(e.what())); }
			catch (std::bad_weak_ptr &e) { UE_LOG(LogOpenVDBModule, Error, TEXT("%s: %s"), *HandleName, UTF8_TO_TCHAR(e.what())); }
			catch (std::exception &e) { UE_LOG(LogOpenVDBModule, Error, TEXT("%s: %s"), *HandleName, UTF8_TO_TCHAR(e.what())); }

			if (!isAllocated)
			{
				UE_LOG(LogOpenVDBModule, Error, TEXT("%s: Failed to allocate grids"), *HandleName);
			}
		}
		else
		{
			UE_LOG(LogOpenVDBModule, Error, TEXT("%s: Unable to allocate grids resource (File is invalid)"), *HandleName);
		}
		return isAllocated;
	}

	inline bool AllocateFileMetaResource()
	{
		bool isAllocated = false;
		if (FilePtr.IsValid())
		{
			try
			{
				FileMetaPtr = FilePtr->getMetadata();
				if (FileMetaPtr != nullptr)
				{
					isAllocated = true;
				}
			}
			catch (openvdb::IoError &e) { UE_LOG(LogOpenVDBModule, Error, TEXT("%s: %s"), *HandleName, UTF8_TO_TCHAR(e.what())); }
			catch (openvdb::Exception &e) { UE_LOG(LogOpenVDBModule, Error, TEXT("%s: %s"), *HandleName, UTF8_TO_TCHAR(e.what())); }
			catch (std::bad_alloc &e) { UE_LOG(LogOpenVDBModule, Error, TEXT("%s: %s"), *HandleName, UTF8_TO_TCHAR(e.what())); }
			catch (std::bad_weak_ptr &e) { UE_LOG(LogOpenVDBModule, Error, TEXT("%s: %s"), *HandleName, UTF8_TO_TCHAR(e.what())); }
			catch (std::exception &e) { UE_LOG(LogOpenVDBModule, Error, TEXT("%s: %s"), *HandleName, UTF8_TO_TCHAR(e.what())); }
		}
		else
		{
			UE_LOG(LogOpenVDBModule, Error, TEXT("%s: Unable to allocate file-meta resource (File is invalid)"), *HandleName);
		}
		return isAllocated;
	}

	inline bool ResetSharedResources()
	{
		bool isFileResetSuccessful = !FilePtr.IsValid(); //Success if not valid (if there is nothing to reset then that's ok)
		if (FilePtr.IsValid())
		{
			FilePtr.Reset();
			isFileResetSuccessful = true;
			UE_LOG(LogOpenVDBModule, Verbose, TEXT("%s: File resource released"), *HandleName);
		}

		bool isGridsResetSuccessful = GridsPtr == nullptr; //Success if not valid (if there is nothing to reset then that's ok)
		if (GridsPtr != nullptr)
		{
			try
			{
				GridsPtr.reset();
				isGridsResetSuccessful = true;
				UE_LOG(LogOpenVDBModule, Verbose, TEXT("%s: Grids resource released"), *HandleName);
			}
			catch (std::bad_alloc &e)
			{
				UE_LOG(LogOpenVDBModule, Error, TEXT("%s: Grids resource reset exception (%s)"), UTF8_TO_TCHAR(e.what()));
			}
			catch (std::exception &e)
			{
				UE_LOG(LogOpenVDBModule, Error, TEXT("%s: Grids resource reset exception (%s)"), UTF8_TO_TCHAR(e.what()));
			}
		}

		bool isMetaResetSuccessful = FileMetaPtr == nullptr; //Success if not valid (if there is nothing to reset then that's ok)
		if (FileMetaPtr != nullptr)
		{
			try
			{
				FileMetaPtr.reset();
				isMetaResetSuccessful = true;
				UE_LOG(LogOpenVDBModule, Verbose, TEXT("%s: Meta resource released"), *HandleName);
			}
			catch (std::bad_alloc &e)
			{
				UE_LOG(LogOpenVDBModule, Error, TEXT("%s: Meta resource reset exception (%s)"), *HandleName, UTF8_TO_TCHAR(e.what()));
			}
			catch (std::exception &e)
			{
				UE_LOG(LogOpenVDBModule, Error, TEXT("%s: Meta resource reset exception (%s)"), *HandleName, UTF8_TO_TCHAR(e.what()));
			}
		}

		UE_LOG(LogOpenVDBModule, Display, TEXT("%s: Resources reset status: File (%s), Grids (%s), Meta (%s)"),
			*HandleName,
			isFileResetSuccessful ? TEXT("succeeded") : TEXT("failed"),
			isGridsResetSuccessful ? TEXT("succeeded") : TEXT("failed"),
			isMetaResetSuccessful ? TEXT("succeeded") : TEXT("failed"));
		return isFileResetSuccessful && isGridsResetSuccessful && isMetaResetSuccessful;
	}

	//inline bool AreChangesPending()
	bool AreChangesPending()
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

	static inline bool IsFileConfigurationValid(const FString &filePath,
		const FString &previousFilePath,
		const bool areThereUnwrittenChanges,
		const bool discardUnwrittenChanges,
		const bool makeDirIfNotExists,
		FString &errorMessage)
	{
		//Configuration is valid if:
		//A) The path and filename are valid (e.g. does not contain disallowed characters).
		//B) No unwritten changes exist OR if they do, we are allowed to discard unwritten changes.
		//C) The new file does not exist OR if it does exist, safe to write if it is the same file as previous.
		//TODO: Check if directory and file are writable
		FText PathNotValidReason;
		const FString pathName = FPaths::GetPath(filePath);
		const bool isPathValid = FPaths::ValidatePath(filePath, &PathNotValidReason);
		const bool isFileValid = !FPaths::GetBaseFilename(filePath).IsEmpty();
		const bool doesTargetPathExist = FPaths::DirectoryExists(pathName);
		const bool doesTargetFileExist = FPaths::FileExists(filePath);
		const bool canDiscardPendingChanges = !areThereUnwrittenChanges || discardUnwrittenChanges;
		const bool canWriteToFileSafely = !doesTargetFileExist || filePath == previousFilePath;
		const bool isPathWritable = doesTargetPathExist || makeDirIfNotExists;
		if (!(isPathValid && isFileValid && canDiscardPendingChanges && canWriteToFileSafely && isPathWritable))
		{
			errorMessage = TEXT("");
			if (!isPathValid)
			{
				errorMessage += FString::Printf(TEXT("\nInvalid path [%s]: %s"), *filePath, *PathNotValidReason.ToString());
			}
			if (!isFileValid)
			{
				errorMessage += FString::Printf(TEXT("\nInvalid filename [%s]"), *FPaths::GetCleanFilename(filePath));
			}
			if (!canDiscardPendingChanges)
			{
				errorMessage += FString::Printf(TEXT("\nThere are unwritten changes to [%s] but the VDB is not configured to discard unwritten changes"), *previousFilePath);
			}
			if (!canWriteToFileSafely)
			{
				errorMessage += FString::Printf(TEXT("\n[%s] already exists"), *filePath);
			}
			if (!isPathWritable)
			{
				errorMessage += FString::Printf(TEXT("\nDirectory [%s] does not exist"), *pathName);
			}
			return false;
		}
		return true;
	}

	//TODO: Use file manager IFileManager
	inline bool CreateAndVerifyDatabase(const FString &filePath,
		const bool enableGridStats,
		const bool enableDelayLoad,
		const bool makeDirIfNotExists = false,
		const bool discardUnwrittenChanges = false)
	{
		check(!discardUnwrittenChanges); //TODO: For the time being, never allow discarding of pending changes
		const bool isPreviousFileValid = FilePtr.IsValid();
		const FString previousFilePath = isPreviousFileValid ? UTF8_TO_TCHAR(FilePtr->filename().c_str()) : filePath;
		const bool previousEnableGridStats = isPreviousFileValid ? FilePtr->isGridStatsMetadataEnabled() : enableGridStats;
		const bool previousEnableDelayLoad = isPreviousFileValid ? FilePtr->isDelayedLoadingEnabled() : enableDelayLoad;
		const bool areThereUnwrittenChanges = isPreviousFileValid && AreChangesPending();
		FString configurationErrors;

		//Check for valid paths, if it's ok to write to the new path, etc
		if (!IsFileConfigurationValid(filePath,
			previousFilePath,
			areThereUnwrittenChanges,
			discardUnwrittenChanges,
			makeDirIfNotExists,
			configurationErrors))
		{
			UE_LOG(LogOpenVDBModule, Error, TEXT("Failed to configure voxel database [%s] path=%s GridStats=%s, DelayLoad=%s. Possible reasons: %s"), *filePath, enableGridStats, enableDelayLoad, *configurationErrors);
			return false;
		}
		
		if (isPreviousFileValid)
		{
			//Configuration is ok, but do nothing because this is actually the same database with the same configuration
			if (previousFilePath == filePath &&
				previousEnableGridStats == enableGridStats &&
				previousEnableDelayLoad == enableDelayLoad)
			{
				return true;
			}

			//Make sure there are no unwritten changes from the previous state.
			//If we are configured not to discard unwritten changes or we are just changing the
			//configuration of an existing database, then first write any existing changes.
			if (!discardUnwrittenChanges || previousFilePath == filePath)
			{
				//Write pending changes and invalidate resources only if the path is changing
				const bool isFinal = previousFilePath != filePath;
				const openvdb::io::Queue::Id jobID = AddAsyncWriteJob(isFinal);
				GetWriteChangesAsyncTimerCallback(jobID)();
			}
			CloseFileGuard();
			UE_LOG(LogOpenVDBModule, Display, TEXT("Previous voxel database config: path=%s GridStats=%s, DelayLoad=%s"), *previousFilePath, previousEnableGridStats ? TEXT("enabled") : TEXT("disabled"), previousEnableDelayLoad ? TEXT("enabled") : TEXT("disabled"));
		}

		//Log the configuration of the upcoming database
		check(!FPaths::GetPath(filePath).IsEmpty());
		check(!FPaths::GetBaseFilename(filePath).IsEmpty());
		UE_LOG(LogOpenVDBModule, Display, TEXT("Voxel database configuration: path=%s GridStats=%s, DelayLoad=%s"), *filePath, enableGridStats ? TEXT("enabled") : TEXT("disabled"), enableDelayLoad ? TEXT("enabled") : TEXT("disabled"));

		if (!FilePtr.IsValid())
		{
			//Create the database with the specified configuration
			const std::string filepathStd = TCHAR_TO_UTF8(*filePath);
			AllocateFileResource(filepathStd);
			FilePtr = TSharedPtr<openvdb::io::File>(new openvdb::io::File(filepathStd));
			check(FilePtr->filename() == filepathStd);
		}

		EnableGridStats = enableGridStats;
		EnableDelayLoad = enableDelayLoad;
		FilePtr->setGridStatsMetadataEnabled(EnableGridStats);

		if (FPaths::FileExists(filePath))
		{
			//If the IsFileConfigurationValid() works as intended and the path has changed then the file shouldn't exist.
			//Check in case it was suddenly created by the OS in the intervening time.
			if (previousFilePath != filePath)
			{
				UE_LOG(LogOpenVDBModule, Fatal, TEXT("%s unexpectedly exists! Cannot create voxel database"), *filePath);
			}
		}
		else
		{
			//Create the empty vdb file
			UE_LOG(LogOpenVDBModule, Display, TEXT("Creating empty voxel database [%s]"), *filePath);
			FilePtr->write(openvdb::GridCPtrVec(), openvdb::MetaMap());
		}
		check(FPaths::FileExists(filePath)); //TODO: Error handling when unable to create file. For now assume the file exists

		//Initially read only file-level metadata and metadata for each grid (do not read tree data values for grids yet)
		OpenFileGuard();
		UE_LOG(LogOpenVDBModule, Display, TEXT("Reading grid metadata and file metadata from voxel database [%s]"), *filePath);
		if (GridsPtr == nullptr)
		{
			if (AllocateGridsResource()) //read metadata (but not tree data) for all grids
			{
				CachedGrid = GridsPtr->end();
				CubesMeshOps.Empty();
				MarchingCubesMeshOps.Empty();
			}
		}
		if (FileMetaPtr == nullptr)
		{
			AllocateFileMetaResource();
		}

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
		: JobID(jobID),
		Database(TSharedPtr<VdbHandlePrivateType>(vbdHandlePrivatePtr)),
		OutFile(*vbdHandlePrivatePtr->FilePtr), //TODO: Need to handle filename change, or will filename change simple create a whole new vdbprivate?
		IsFinal(isFinal),
		IsRunning(false),
		IsFinished(false)
	{
		check(Database.IsValid());
		check(Database->GridsPtr != nullptr);
		check(Database->FilePtr.IsValid());
		UE_LOG(LogOpenVDBModule, Verbose, TEXT("AsyncIOJob_%d: Created"), JobID);
	}

	bool StartJob(openvdb::io::Queue &queue)
	{
		bool isJobRunning = false;
		//Must have a valid voxel database and file. It is ok to run the job with empty grid vec or empty file-meta
		check(Database.IsValid());
		check(Database->FilePtr.IsValid());
		UE_LOG(LogOpenVDBModule, Verbose, TEXT("AsyncIOJob_%d: Initializing..."), JobID);

		try
		{
			//TODO!!!!! Mutex on Database while copying grids, meta, and file?
			check(Database->FilePtr.IsValid());
			if (Database->AreChangesPending()) //No changes pending? ez lyfe
			{
				//Copy the file
				UE_LOG(LogOpenVDBModule, Verbose, TEXT("AsyncIOJob_%d: Copying file descriptor..."), JobID);
				OutFile = *(Database->FilePtr);
				UE_LOG(LogOpenVDBModule, Verbose, TEXT("AsyncIOJob_%d: File descriptor copied"), JobID);

				//Initialize grid vec to empty, then if the source has a valid grid vec, deep copy all grids
				OutGrids = openvdb::GridPtrVec();
				if (Database->GridsPtr)
				{
					UE_LOG(LogOpenVDBModule, Verbose, TEXT("AsyncIOJob_%d: Copying all grids"), JobID);
					for (auto i = Database->GridsPtr->begin(); i != Database->GridsPtr->end(); ++i)
					{
						check(*i);
						auto &grid = *(*i);
						OutGrids.push_back(grid.deepCopyGrid());
						UE_LOG(LogOpenVDBModule, Verbose, TEXT("AsyncIOJob_%d: %s copied"), UTF8_TO_TCHAR(grid.getName().c_str()));
					}
					UE_LOG(LogOpenVDBModule, Verbose, TEXT("AsyncIOJob_%d: All grids copied"), JobID);
				}
				else
				{
					UE_LOG(LogOpenVDBModule, Verbose, TEXT("AsyncIOJob_%d: No grids to copy"), JobID);
				}

				//Initialize file-meta to empty, then if the source has valid file-meta, deep copy all file-meta
				OutFileMeta = openvdb::MetaMap();
				if (Database->FileMetaPtr)
				{
					UE_LOG(LogOpenVDBModule, Verbose, TEXT("AsyncIOJob_%d: Copying file-meta"), JobID);
					OutFileMeta = *(Database->FileMetaPtr->deepCopyMeta());
					UE_LOG(LogOpenVDBModule, Verbose, TEXT("AsyncIOJob_%d: File-meta copied"), JobID);
				}
				else
				{
					UE_LOG(LogOpenVDBModule, Verbose, TEXT("AsyncIOJob_%d: No file-meta to copy"), JobID);
				}

				Database->CloseFileGuard(); //openvdb::io::File must be closed in order to write
				UE_LOG(LogOpenVDBModule, Display, TEXT("AsyncIOJob_%d: Queuing pending grid and file meta changes for async write to [%s]"), JobID, UTF8_TO_TCHAR(OutFile.filename().c_str()));
				queue.write(OutGrids, OutFile, OutFileMeta);
			}
			isJobRunning = true;
		}
		catch (const openvdb::RuntimeError &e)
		{
			UE_LOG(LogOpenVDBModule, Error, TEXT("AsyncIOJob_%d: OpenVDB runtime error: %s"), JobID, UTF8_TO_TCHAR(e.what()));
		}
		catch (const openvdb::Exception &e)
		{
			UE_LOG(LogOpenVDBModule, Error, TEXT("AsyncIOJob_%d: OpenVDB exception: %s"), JobID, UTF8_TO_TCHAR(e.what()));
		}
		isJobRunning = IsRunning;
		return IsRunning;
	}

	bool CleanupJob(bool isJobSuccessful)
	{
		bool isJobCleanedUp = false;
		bool isJobFinished = false;
		check(Database.IsValid());
		Database->RunningJobID = UINT32_MAX;
		const std::string &filename = Database->FilePtr->filename();
		UE_LOG(LogOpenVDBModule, Display, TEXT("AsyncIOJob_%d post-run clean up of [%s]..."), JobID, UTF8_TO_TCHAR(filename.c_str()));

		try
		{
			if (IsFinal)
			{
				UE_LOG(LogOpenVDBModule, Display, TEXT("AsyncIOJob_%d: Resetting shared resources"), JobID);
				isJobCleanedUp = Database->ResetSharedResources();
			}
		}
		catch (const openvdb::RuntimeError &e)
		{
			UE_LOG(LogOpenVDBModule, Error, TEXT("AsyncIOJob_%d: OpenVDB runtime error: %s"), JobID, UTF8_TO_TCHAR(e.what()));
		}
		catch (const openvdb::Exception &e)
		{
			UE_LOG(LogOpenVDBModule, Error, TEXT("AsyncIOJob_%d: OpenVDB exception: %s"), JobID, UTF8_TO_TCHAR(e.what()));
		}

		IsFinished = isJobSuccessful && isJobCleanedUp;
		if (IsFinished)
		{
			Database->SyncGridsAndFileMetaStatus();
			UE_LOG(LogOpenVDBModule, Display, TEXT("AsyncIOJob_%d: Job completed"), JobID);
		}
		else
		{
			Database->DesyncGridsAndFileMetaStatus();
			UE_LOG(LogOpenVDBModule, Error, TEXT("AsyncIOJob_%d: Job failed"), JobID);
		}
		return IsFinished;
	}

	const openvdb::io::Queue::Id JobID;
	TSharedPtr<VdbHandlePrivateType> Database;
	bool IsRunning;
	bool IsFinished;
	const bool IsFinal;
	openvdb::io::File &OutFile;
	openvdb::GridPtrVec OutGrids;
	openvdb::MetaMap OutFileMeta;
};

struct AsyncIONotifier
{
	typedef tbb::concurrent_hash_map<openvdb::io::Queue::Id, AsyncIOJob> AsyncJobs;
	AsyncJobs IOJobs;
	openvdb::io::Queue IOQueue;
	FCriticalSection GetJobIDSection;

	AsyncIONotifier()
	{
		IOQueue.addNotifier(boost::bind(&AsyncIONotifier::Callback, this, _1, _2));
	}

	openvdb::io::Queue::Id AddJob(VdbHandlePrivateType * vdbHandlePrivatePtr, bool isFinal)
	{
		static openvdb::io::Queue::Id LastUsedJobID = 0;

		openvdb::io::Queue::Id jobID = UINT32_MAX;
		{
			FScopeLock jobIDSection(&GetJobIDSection);
			AsyncJobs::const_accessor cacc;
			do
			{
				jobID = LastUsedJobID++;
			} while (IOJobs.find(cacc, jobID) && jobID != UINT32_MAX);
		}
		
		if (jobID != UINT32_MAX)
		{
			check(vdbHandlePrivatePtr);
			IOJobs.insert(std::pair<openvdb::io::Queue::Id, AsyncIOJob>(jobID, AsyncIOJob(jobID, vdbHandlePrivatePtr, isFinal)));
		}
		return jobID;
	}

	bool StartNextJob(openvdb::io::Queue::Id nextJobID)
	{
		bool isJobStarted = false;
		if (nextJobID == UINT32_MAX)
		{
			AsyncJobs::accessor acc;
			const bool isJobFound = IOJobs.find(acc, nextJobID);
			if (isJobFound)
			{
				isJobStarted = acc->second.StartJob(IOQueue);
			}
		}
		return isJobStarted;
	}

	// Callback function called when an AsyncIOJob is completed.
	void Callback(openvdb::io::Queue::Id id, openvdb::io::Queue::Status status)
	{
		const bool succeeded = (status == openvdb::io::Queue::SUCCEEDED);
		AsyncJobs::accessor acc;
		if (IOJobs.find(acc, id))
		{
			acc->second.CleanupJob(succeeded);
			IOJobs.erase(acc);
		}
	}
};

//Operator to find a grid from GridPtrVec
struct GridNameIs
{
	GridNameIs(const std::string &nameToFind) : NameToFind(nameToFind) {}
	inline bool operator()(const openvdb::GridBase::Ptr &gridPtr)
	{
		return gridPtr->getName() == NameToFind;
	}
	const std::string &NameToFind;
};