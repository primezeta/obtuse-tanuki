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

template<typename TreeType, typename IndexTreeType, typename MetadataTypeA>
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

	const FString FilePath;
	const bool EnableGridStats;
	const bool EnableDelayLoad;
	
	static FString MetaName_WorldName() { return TEXT("WorldName"); }
	static FString MetaName_RegionScale() { return TEXT("RegionScale"); }

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
			VertexSectionBuffer->Add(TArray<FVector>());
			PolygonSectionBuffer->Add(TArray<int32>());
			NormalSectionBuffer->Add(TArray<FVector>());
			GridTypePtr grid = openvdb::gridPtrCast<GridType>(*i);
			check(grid != nullptr);
			MeshOps.Add(gridName, TSharedPtr<MesherOpType>(new MesherOpType(grid, (*VertexSectionBuffer)[sectionIndex], (*PolygonSectionBuffer)[sectionIndex], (*NormalSectionBuffer)[sectionIndex])));
			sectionIndex++;
		}
	}

	template<typename TreeType>
	GridTypePtr GetGridPtr(const FString &gridName)
	{
		GridTypePtr grid = GridType::create();
		grid->setName(TCHAR_TO_UTF8(*gridName));
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
		if (i == GridsPtr->end())
		{
			int32 sectionIndex = GridsPtr->size();
			GridsPtr->push_back(grid);
			WriteChanges();
			
			VertexSectionBuffer->Add(TArray<FVector>());
			PolygonSectionBuffer->Add(TArray<int32>());
			NormalSectionBuffer->Add(TArray<FVector>());
			MeshOps.Add(gridName, TSharedPtr<MesherOpType>(new MesherOpType(grid, (*VertexSectionBuffer)[sectionIndex], (*PolygonSectionBuffer)[sectionIndex], (*NormalSectionBuffer)[sectionIndex])));
		}
		return grid;
	}

	template<typename TreeType>
	void AddGrid(const FString &gridName)
	{
		GridTypePtr gridPtr = GetGridPtr<TreeType>(gridName);
		check(gridPtr != nullptr);
		openvdb::math::UniformScaleMap::Ptr map = openvdb::math::UniformScaleMap::Ptr(new openvdb::math::UniformScaleMap(0.1));
		openvdb::math::Transform::Ptr xform = openvdb::math::Transform::Ptr(new openvdb::math::Transform(map));
		gridPtr->setTransform(xform->copy());
	}

	template<typename TreeType>
	GridTypePtr ReadGridTree(const FString &gridName, FIntVector &activeStart, FIntVector &activeEnd)
	{
		GridTypePtr grid = GetGridPtr<TreeType>(gridName);
		if (grid->activeVoxelCount() == 0)
		{
			OpenFileGuard();
			GridTypePtr activeGrid = openvdb::gridPtrCast<GridType>(FilePtr->readGrid(TCHAR_TO_UTF8(*gridName)));
			check(activeGrid != nullptr);
			grid.swap(activeGrid);
		}
		openvdb::CoordBBox activeBBox = grid->evalActiveVoxelBoundingBox();
		activeStart.X = activeBBox.min().x();
		activeStart.Y = activeBBox.min().y();
		activeStart.Z = activeBBox.min().z();
		activeEnd.X = activeBBox.max().x();
		activeEnd.Y = activeBBox.max().y();
		activeEnd.Z = activeBBox.max().z();
		return grid;
	}

	template<typename TreeType>
	GridTypePtr ReadGridTree(const FString &gridName, FVector &activeStart, FVector &activeEnd)
	{
		GridTypePtr grid = GetGridPtr<TreeType>(gridName);
		if (grid->activeVoxelCount() == 0)
		{
			OpenFileGuard();
			GridTypePtr activeGrid = openvdb::gridPtrCast<GridType>(FilePtr->readGrid(TCHAR_TO_UTF8(*gridName)));
			check(activeGrid != nullptr);
			grid.swap(activeGrid);
		}
		openvdb::CoordBBox activeBBox = grid->indexToWorld(grid->evalActiveVoxelBoundingBox());
		activeStart.X = activeBBoxd.min().x();
		activeStart.Y = activeBBoxd.min().y();
		activeStart.Z = activeBBoxd.min().z();
		activeEnd.X = activeBBoxd.max().x();
		activeEnd.Y = activeBBoxd.max().y();
		activeEnd.Z = activeBBoxd.max().z();
		return grid;
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

	template<typename TreeType, typename MetadataType>
	typename openvdb::TypedMetadata<MetadataType>::Ptr GetGridMetaValue(const FString &gridName, const FString &metaName) const
	{
		openvdb::TypedMetadata<MetadataType>::Ptr metaValuePtr;
		GridTypePtr gridPtr = GetGridPtr<TreeType>(gridName);
		metaValuePtr = gridPtr->getMetadata<openvdb::TypedMetadata<MetadataType>>(TCHAR_TO_UTF8(*gridName));
		return metaValuePtr;
	}

	template<typename TreeType, typename MetadataType>
	void InsertGridMeta(const FString &gridName, const FString &metaName, const MetadataType &metaValue)
	{
		GridTypePtr gridPtr = GetGridPtr<TreeType>(gridName);
		gridPtr->insertMeta<openvdb::TypedMetadata<MetadataType>>(TCHAR_TO_UTF8(*metaName), openvdb::TypedMetadata<MetadataType>(metaValue));
	}

	template<typename TreeType, typename MetadataType>
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

		openvdb::GridPtrVec outGrids;
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
		TSharedPtr<MesherOpType> mesherOp = MeshOps.FindChecked(gridName);
		mesherOp->doActivateValuesOp(surfaceValue);
		mesherOp->doMeshOp();
	}

	template<typename TreeType>
	void FillGrid_PerlinDensity(const FString &gridName, const FIntVector &fillIndexStart, const FIntVector &fillIndexEnd, float frequency, float lacunarity, float persistence, int32 octaveCount, FIntVector &activeStart, FIntVector &activeEnd)
	{
		GridTypePtr gridPtr = GetGridPtr<TreeType>(gridName);
		const openvdb::CoordBBox fillBBox = openvdb::CoordBBox(openvdb::Coord(fillIndexStart.X, fillIndexStart.Y, fillIndexStart.Z), openvdb::Coord(fillIndexEnd.X, fillIndexEnd.Y, fillIndexEnd.Z));

		//Noise module parameters are at the grid-level metadata
		openvdb::FloatMetadata::Ptr frequencyMeta = gridPtr->getMetadata<openvdb::FloatMetadata>("frequency");
		openvdb::FloatMetadata::Ptr lacunarityMeta = gridPtr->getMetadata<openvdb::FloatMetadata>("lacunarity");
		openvdb::FloatMetadata::Ptr persistenceMeta = gridPtr->getMetadata<openvdb::FloatMetadata>("persistence");
		openvdb::Int32Metadata::Ptr octaveCountMeta = gridPtr->getMetadata<openvdb::Int32Metadata>("octaveCount");
		if (gridPtr->tree().empty() ||
			frequencyMeta == nullptr || !openvdb::math::isApproxEqual(frequency, frequencyMeta->value()) ||
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
	TMap<FString, TSharedPtr<MesherOpType>> MeshOps;
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