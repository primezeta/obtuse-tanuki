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

	inline GridType& GetGrid(const FString &gridName)
	{
		bool isNewGrid = false;
		return GetOrCreateGrid(gridName, isNewGrid);
	}

	inline GridType& GetOrCreateGrid(const FString &gridName, bool &isNewGrid, GridTypePtr gridPtr = nullptr)
	{
		isNewGrid = false;
		if (CachedGrid == GridsPtr->end() || gridName != UTF8_TO_TCHAR((*CachedGrid)->getName().c_str()))
		{
			CachedGrid = GridsPtr->begin();
			for (; CachedGrid != GridsPtr->end() && gridName != UTF8_TO_TCHAR((*CachedGrid)->getName().c_str()); ++CachedGrid);
		}
		if (CachedGrid == GridsPtr->end())
		{
			gridPtr = GridType::create();
			GridsPtr->push_back(gridPtr);
			CachedGrid = GridsPtr->end();
			CachedGrid--;
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

		bool isNewGrid = false;
		GridType &grid = GetOrCreateGrid(gridName, isNewGrid);
		if (isNewGrid)
		{
			grid.setName(TCHAR_TO_UTF8(*gridName));
			grid.setTransform(xformPtr);
			CubesMeshOps.Emplace(gridName, TSharedRef<Vdb::GridOps::CubeMesher<GridTreeType>>(new Vdb::GridOps::CubeMesher<GridTreeType>(grid, sectionBuffers)));
			MarchingCubesMeshOps.Emplace(gridName, TSharedRef<Vdb::GridOps::MarchingCubesMesher<GridTreeType>>(new Vdb::GridOps::MarchingCubesMesher<GridTreeType>(grid, sectionBuffers)));
			SetIsFileInSync(false);
		}
		else if (grid.transform() != *xformPtr)
		{
			grid.setTransform(xformPtr);
			CubesMeshOps.Emplace(gridName, TSharedRef<Vdb::GridOps::CubeMesher<GridTreeType>>(new Vdb::GridOps::CubeMesher<GridTreeType>(grid, sectionBuffers)));
			MarchingCubesMeshOps.Emplace(gridName, TSharedRef<Vdb::GridOps::MarchingCubesMesher<GridTreeType>>(new Vdb::GridOps::MarchingCubesMesher<GridTreeType>(grid, sectionBuffers)));
			SetIsFileInSync(false);
		}

		const openvdb::Vec3IMetadata::Ptr currentIndexStartMeta = grid.getMetadata<openvdb::Vec3IMetadata>(TCHAR_TO_UTF8(*MetaName_RegionIndexStart()));
		//TODO
		//const openvdb::Vec3i indexStartVec(indexStart.X, indexStart.Y, indexStart.Z);
		const openvdb::Vec3i indexStartVec(0, 0, 0);
		if (currentIndexStartMeta == nullptr || !openvdb::math::isExactlyEqual(currentIndexStartMeta->value(), indexStartVec))
		{
			grid.insertMeta(TCHAR_TO_UTF8(*MetaName_RegionIndexStart()), openvdb::Vec3IMetadata(indexStartVec));
			grid.insertMeta(TCHAR_TO_UTF8(*MetaName_RegionStart()), openvdb::Vec3DMetadata(map->applyMap(openvdb::Vec3d(indexStartVec.x(), indexStartVec.y(), indexStartVec.z()))));
			SetIsFileInSync(false);
		}

		const openvdb::Vec3IMetadata::Ptr currentIndexEndMeta = grid.getMetadata<openvdb::Vec3IMetadata>(TCHAR_TO_UTF8(*MetaName_RegionIndexEnd()));
		//TODO
		//const openvdb::Vec3i indexEndVec(indexEnd.X, indexEnd.Y, indexEnd.Z);
		const openvdb::Vec3i indexEndVec(gridDimensions.X, gridDimensions.Y, gridDimensions.Z);
		if (currentIndexEndMeta == nullptr || !openvdb::math::isExactlyEqual(currentIndexEndMeta->value(), indexEndVec))
		{
			grid.insertMeta(TCHAR_TO_UTF8(*MetaName_RegionIndexEnd()), openvdb::Vec3IMetadata(indexEndVec));
			grid.insertMeta(TCHAR_TO_UTF8(*MetaName_RegionEnd()), openvdb::Vec3DMetadata(map->applyMap(openvdb::Vec3d(indexEndVec.x(), indexEndVec.y(), indexEndVec.z()))));
			SetIsFileInSync(false);
		}
	}

	GridType& ReadGridTree(const FString &gridName, FIntVector &indexStart, FIntVector &indexEnd)
	{
		GridType &grid = GetGrid(gridName);
		if (grid.activeVoxelCount() == 0)
		{
			//This grid may not have been written to file yet
			if (FilePtr->hasGrid(TCHAR_TO_UTF8(*gridName)))
			{
				//It has been written to file already so swap the grid tree to the grid tree that was read from file
				OpenFileGuard();
				GridTypePtr activeGrid = openvdb::gridPtrCast<GridType>(FilePtr->readGrid(TCHAR_TO_UTF8(*gridName)));
				check(activeGrid != nullptr);
				check(activeGrid->treePtr() != nullptr);
				grid.setTree(activeGrid->treePtr());
			}
		}

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
	TSharedPtr<openvdb::TypedMetadata<GridMetaType>> GetGridMetaValue(const FString &gridName, const FString &metaName)
	{
		TSharedPtr<openvdb::TypedMetadata<GridMetaType>> metaDataTShared(nullptr);
		GridType &grid = GetGrid(gridName);
		openvdb::TypedMetadata<GridMetaType>::Ptr metaDataPtr = grid.getMetadata<openvdb::TypedMetadata<GridMetaType>>(TCHAR_TO_UTF8(*gridName));
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
		GridType &grid = GetGrid(gridName);
		openvdb::TypedMetadata<GridMetaType>::Ptr currentGridMeta = grid.getMetadata<openvdb::TypedMetadata<GridMetaType>>(TCHAR_TO_UTF8(*metaName));
		if (currentGridMeta == nullptr || currentGridMeta->value() != metaValue)
		{
			grid.insertMeta(TCHAR_TO_UTF8(*metaName), openvdb::TypedMetadata<GridMetaType>(metaValue));
			SetIsFileInSync(false);
		}
	}

	template<typename GridMetaType>
	void RemoveGridMeta(const FString &gridName, const FString &metaName)
	{
		GridType &grid = GetGrid(gridName);
		if (grid[TCHAR_TO_UTF8(*metaName)] != nullptr)
		{
			grid.removeMeta(TCHAR_TO_UTF8(*metaName));
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
		GridType &grid = GetGrid(gridName);

		//Noise module parameters are at the grid-level metadata
		openvdb::Int32Metadata::Ptr seedMeta = grid.getMetadata<openvdb::Int32Metadata>("seed");
		openvdb::FloatMetadata::Ptr frequencyMeta = grid.getMetadata<openvdb::FloatMetadata>("frequency");
		openvdb::FloatMetadata::Ptr lacunarityMeta = grid.getMetadata<openvdb::FloatMetadata>("lacunarity");
		openvdb::FloatMetadata::Ptr persistenceMeta = grid.getMetadata<openvdb::FloatMetadata>("persistence");
		openvdb::Int32Metadata::Ptr octaveCountMeta = grid.getMetadata<openvdb::Int32Metadata>("octaveCount");
		bool isEmpty = grid.tree().empty();
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
			grid.insertMeta("seed", openvdb::Int32Metadata(seed));
			grid.insertMeta("frequency", openvdb::FloatMetadata(frequency));
			grid.insertMeta("lacunarity", openvdb::FloatMetadata(lacunarity));
			grid.insertMeta("persistence", openvdb::FloatMetadata(persistence));
			grid.insertMeta("octaveCount", openvdb::Int32Metadata(octaveCount));

			CubesMeshOps[gridName]->markChanged();
			MarchingCubesMeshOps[gridName]->markChanged();

			typedef typename Vdb::GridOps::PerlinNoiseFillOp<GridTreeType, GridTreeType::ValueOnIter> NoiseFillOpType;
			typedef typename openvdb::tools::valxform::SharedOpApplier<GridTreeType::ValueOnIter, NoiseFillOpType> NoiseFillProcessor;
			const openvdb::Coord startFill(0, 0, 0);
			const openvdb::Coord endFill(fillIndexEnd.X - fillIndexStart.X, fillIndexEnd.Y - fillIndexStart.Y, fillIndexEnd.Z - fillIndexStart.Z);
			openvdb::CoordBBox fillBBox = openvdb::CoordBBox(startFill, endFill);
			check(!fillBBox.empty());
			NoiseFillOpType noiseFillOp(grid, fillBBox, seed, frequency, lacunarity, persistence, octaveCount);
			NoiseFillProcessor NoiseFillProc(grid.beginValueOn(), noiseFillOp);
			NoiseFillProc.process(threaded);
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
		UE_LOG(LogOpenVDBModule, Display, TEXT("%s %d active voxels"), UTF8_TO_TCHAR(grid.getName().c_str()), grid.activeVoxelCount());
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
		UE_LOG(LogOpenVDBModule, Display, TEXT("%s %d active voxels"), UTF8_TO_TCHAR(grid.getName().c_str()), grid.activeVoxelCount());
	}

	void CalculateGradient(const FString &gridName, bool threaded)
	{
		typedef typename Vdb::GridOps::CalcGradientOp_FVoxelData<GridTreeType, GridTreeType::ValueOnCIter, openvdb::Vec3fTree> CalcGradientOp_FVoxelDataType;
		typedef typename Vdb::GridOps::ISGradient_FVoxelData<openvdb::math::CD_2ND, CalcGradientOp_FVoxelDataType::SourceAccessorType>::Vec3Type VecType;
		typedef typename openvdb::TreeAdapter<openvdb::Grid<openvdb::Vec3fTree>> Adapter;
		typedef typename openvdb::tools::valxform::SharedOpTransformer<GridTreeType::ValueOnCIter, Adapter::TreeType, CalcGradientOp_FVoxelDataType> CalcGradientOp_FVoxelDataProcessor;
		GridType &grid = GetGrid(gridName);
		CalcGradientOp_FVoxelDataType CalcGradientOp_FVoxelDataOp(grid);
		CalcGradientOp_FVoxelDataProcessor CalcGradientProc(grid.cbeginValueOn(), Adapter::tree(MarchingCubesMeshOps[gridName]->Gradient), CalcGradientOp_FVoxelDataOp, openvdb::MERGE_ACTIVE_STATES);
		CalcGradientProc.process(threaded);
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
		FString msg = FString::Printf(TEXT("%s active voxel types are"), UTF8_TO_TCHAR(grid.getName().c_str()));
		for (auto i = sectionVoxelTypes.CreateConstIterator(); i; ++i)
		{
			msg += FString::Printf(TEXT(" %s"), *Enum->GetDisplayNameText((int32)i->GetValue()).ToString());
		}
		UE_LOG(LogOpenVDBModule, Display, TEXT("%s"), *msg);
	}

	void MeshRegionCubes(const FString &gridName)
	{
		GridType &grid = GetGrid(gridName);
		const bool threaded = true;
		check(CubesMeshOps.Contains(gridName));
		CubesMeshOps[gridName]->doMeshOp(threaded);
	}

	void MeshRegionMarchingCubes(const FString &gridName)
	{
		GridType &grid = GetGrid(gridName);
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

	bool GetGridDimensions(const FString &gridName, FBox &worldBounds, FVector &firstActive)
	{
		GridType &grid = GetGrid(gridName);
		openvdb::Coord firstActiveCoord(0, 0, 0);
		openvdb::CoordBBox activeIndexBBox = grid.evalActiveVoxelBoundingBox();
		const bool hasActiveVoxels = !activeIndexBBox.empty();
		if (hasActiveVoxels)
		{
			GetFirstActiveCoord(grid, activeIndexBBox, firstActiveCoord);
		}
		else
		{
			//If the grid has no active values then provide the bounds of the entire volume
			const auto metaMin = grid.getMetadata<openvdb::Vec3IMetadata>(TCHAR_TO_UTF8(*MetaName_RegionIndexStart()));
			const auto metaMax = grid.getMetadata<openvdb::Vec3IMetadata>(TCHAR_TO_UTF8(*MetaName_RegionIndexEnd()));
			activeIndexBBox = openvdb::CoordBBox(openvdb::Coord(metaMin->value()), openvdb::Coord(metaMax->value()));
		}
		const openvdb::BBoxd worldBBox = grid.transform().indexToWorld(activeIndexBBox);
		const openvdb::Vec3d firstActiveWorld = grid.indexToWorld(firstActiveCoord);
		const openvdb::Vec3d voxelSize = grid.transform().voxelSize();
		worldBounds.Min = FVector(worldBBox.min().x(), worldBBox.min().y(), worldBBox.min().z());
		worldBounds.Max = FVector(worldBBox.max().x(), worldBBox.max().y(), worldBBox.max().z());
		firstActive.X = firstActiveWorld.x() + voxelSize.x()*0.5;
		firstActive.Y = firstActiveWorld.y() + voxelSize.y()*0.5;
		firstActive.Z = firstActiveWorld.z() + voxelSize.z();
		return hasActiveVoxels;
	}

	void GetIndexCoord(const FString &gridName, const FVector &location, FIntVector &outIndexCoord)
	{
		GridType &grid = GetGrid(gridName);
		const openvdb::Coord coord = openvdb::Coord::floor(grid.worldToIndex(openvdb::Vec3d(location.X, location.Y, location.Z)));
		outIndexCoord.X = coord.x();
		outIndexCoord.Y = coord.y();
		outIndexCoord.Z = coord.z();
	}

	void GetVoxelValue(const FString &gridName, const FIntVector &indexCoord, typename GridType::ValueType &outValue)
	{
		GridType &grid = GetGrid(gridName);
		const openvdb::Coord coord(indexCoord.X, indexCoord.Y, indexCoord.Z);
		GridType::ConstAccessor cacc = grid.getConstAccessor();
		outValue = cacc.getValue(coord);
	}

	bool GetVoxelActiveState(const FString &gridName, const FIntVector &indexCoord)
	{
		GridType &grid = GetGrid(gridName);
		const openvdb::Coord coord(indexCoord.X, indexCoord.Y, indexCoord.Z);
		GridType::ConstAccessor cacc = grid.getConstAccessor();
		return cacc.isValueOn(coord);
	}

	bool GetVoxelValueAndActiveState(const FString &gridName, const FIntVector &indexCoord, typename GridType::ValueType &outValue)
	{
		GridType &grid = GetGrid(gridName);
		const openvdb::Coord coord(indexCoord.X, indexCoord.Y, indexCoord.Z);
		GridType::ConstAccessor cacc = grid.getConstAccessor();
		outValue = cacc.getValue(coord);
		return cacc.isValueOn(coord);
	}

	void SetVoxelValue(const FString &gridName, const FIntVector &indexCoord, const typename GridType::ValueType &value)
	{
		GridType &grid = GetGrid(gridName);
		const openvdb::Coord coord(indexCoord.X, indexCoord.Y, indexCoord.Z);
		GridType::Accessor acc = grid.getAccessor();
		acc.modifyValueAndActiveState<Vdb::GridOps::BasicModifyOp<typename GridType::ValueType>>(coord, Vdb::GridOps::BasicModifyOp<typename GridType::ValueType>>(value));
		CubesMeshOps.FindChecked(gridName)->markChanged();
	}

	void SetVoxelActiveState(const FString &gridName, const FIntVector &indexCoord, const bool &isActive)
	{
		GridType &grid = GetGrid(gridName);
		const openvdb::Coord coord(indexCoord.X, indexCoord.Y, indexCoord.Z);
		GridType::Accessor acc = grid.getAccessor();
		acc.setActiveState(coord, isActive);
		CubesMeshOps.FindChecked(gridName)->markChanged();
	}

	void SetVoxelValueAndActiveState(const FString &gridName, const FIntVector &indexCoord, const typename GridType::ValueType &value, const bool &isActive)
	{
		GridType &grid = GetGrid(gridName);
		const openvdb::Coord coord(indexCoord.X, indexCoord.Y, indexCoord.Z);
		GridType::Accessor acc = grid.getAccessor();
		acc.modifyValueAndActiveState<Vdb::GridOps::BasicModifyActiveOp<typename GridType::ValueType>>(coord, Vdb::GridOps::BasicModifyActiveOp<typename GridType::ValueType>>(value, isActive));
		CubesMeshOps.FindChecked(gridName)->markChanged();
	}

private:
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

	inline void GetFirstActiveCoord(GridType &grid, openvdb::CoordBBox &activeIndexBBox, openvdb::Coord &firstActive)
	{
		activeIndexBBox = grid.evalActiveVoxelBoundingBox();
		for (auto i = grid.cbeginValueOn(); i; ++i)
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