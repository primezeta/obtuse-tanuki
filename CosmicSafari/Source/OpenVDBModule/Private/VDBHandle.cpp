#include "OpenVDBModule.h"
#pragma warning(push)
#pragma warning(1:4211 4800 4503 4146)
#include "VDBHandle.h"
#include "openvdb/openvdb.h"
#include "GridOps.h"
#include "GridMetadata.h"

////The standard OpenVDB tree configuration in which leaf nodes contain ValueType
//template<typename ValueType>
//using TreeType = openvdb::tree::Tree4<ValueType, 5, 4, 3>::Type;

DEFINE_LOG_CATEGORY(LogVDBHandle)

template<typename TreeType, typename... MetadataTypes>
class VdbHandlePrivate
{
public:
	typedef Vdb::GridOps::BasicMesher<TreeType> MesherOpType;
	typedef openvdb::Grid<TreeType> GridType;

	const FString FilePath;
	const bool DelayLoadIsEnabled;
	const bool GridStatsIsEnabled;
	TSharedPtr<openvdb::io::File> FilePtr;
	TSharedPtr<openvdb::GridPtrVec> GridsPtr;
	TSharedPtr<openvdb::MetaMap> FileMetaPtr;

	VdbHandlePrivate(const FString &path, bool enableDelayLoad, bool enableGridStats)
		: FilePath(path), DelayLoadIsEnabled(enableDelayLoad), GridStatsIsEnabled(enableGridStats)
	{
		//Initialize OpenVDB, our metadata types, and the vdb file
		openvdb::initialize();
		InitializeMetadataTypes<MetadataTypes...>();
		FilePtr = TSharedPtr<openvdb::io::File>(new openvdb::io::File(TCHAR_TO_UTF8(*path)));
		check(FilePtr.IsValid());
		FilePtr->setGridStatsMetadataEnabled(GridStatsIsEnabled);

		//Create the vdb file if it does not exist
		if (!FPaths::FileExists(path))
		{
			FilePtr->write(openvdb::GridPtrVec());
			UE_LOG(LogVDBHandle, Verbose, TEXT("IVdb: Created %s"), *path);
		}
		check(FPaths::FileExists(path)); //TODO: Error handling when unable to create file. For now assume the file exists

		OpenFileGuard();
		GridsPtr = TSharedPtr<openvdb::GridPtrVec>(FilePtr->readAllGridMetadata().get());
		FileMetaPtr = TSharedPtr<openvdb::MetaMap>(FilePtr->getMetadata().get());
		check(GridsPtr.IsValid());
		check(FileMetaPtr.IsValid());
	}

	~VdbHandlePrivate()
	{
		//TODO: Write changes if they exist?
		openvdb::uninitialize();
	}

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
			FilePtr->open(DelayLoadIsEnabled);
		}
	}

	void CloseFileGuard()
	{
		if (FilePtr.IsValid() && FilePtr->isOpen())
		{
			FilePtr->close();
		}
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
	void FillGrid_PerlinDensity(const FString &gridName, double frequency, double lacunarity, double persistence, int octaveCount)
	{
		TSharedPtr<GridType> gridPtr = GetGridPtr<TreeType>(gridName);
		if (gridPtr.IsValid())
		{
			//Noise module parameters are at the grid-level metadata
			openvdb::DoubleMetadata::Ptr frequencyMeta = gridPtr->getMetadata<openvdb::DoubleMetadata>("frequency");
			openvdb::DoubleMetadata::Ptr lacunarityMeta = gridPtr->getMetadata<openvdb::DoubleMetadata>("lacunarity");
			openvdb::DoubleMetadata::Ptr persistenceMeta = gridPtr->getMetadata<openvdb::DoubleMetadata>("persistence");
			openvdb::Int32Metadata::Ptr octaveCountMeta = gridPtr->getMetadata<openvdb::Int32Metadata>("octaveCount");
			if (frequencyMeta == nullptr || !openvdb::math::isApproxEqual(frequency, frequencyMeta->value()) ||
				lacunarityMeta == nullptr || !openvdb::math::isApproxEqual(lacunarity, lacunarityMeta->value()) ||
				persistenceMeta == nullptr || !openvdb::math::isApproxEqual(persistence, persistenceMeta->value()) ||
				octaveCountMeta == nullptr || !openvdb::math::isExactlyEqual(octaveCount, octaveCountMeta->value()))
			{
				//Update the Perlin noise parameters
				gridPtr->insertMeta("frequency", openvdb::DoubleMetadata(frequency));
				gridPtr->insertMeta("lacunarity", openvdb::DoubleMetadata(lacunarity));
				gridPtr->insertMeta("persistence", openvdb::DoubleMetadata(persistence));
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
};

typedef openvdb::FloatTree TreeType;
typedef VdbHandlePrivate<TreeType, Vdb::Metadata::RegionMetadata> VdbHandlePrivateType;
TSharedPtr<VdbHandlePrivateType> VDBPrivatePtr;

UVDBHandle::UVDBHandle(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UVDBHandle::Initialize(const FString &path, bool enableDelayLoad, bool enableGridStats)
{
	if (!VDBPrivatePtr.IsValid())
	{
		VDBPrivatePtr = TSharedPtr<VdbHandlePrivateType>(new VdbHandlePrivateType(path, enableDelayLoad, enableGridStats));
	}
	else if (VDBPrivatePtr->FilePath != path ||
			 VDBPrivatePtr->DelayLoadIsEnabled != enableDelayLoad ||
			 VDBPrivatePtr->GridStatsIsEnabled != enableGridStats)
	{
		VDBPrivatePtr->WriteChanges();
		VDBPrivatePtr = TSharedPtr<VdbHandlePrivateType>(new VdbHandlePrivateType(path, enableDelayLoad, enableGridStats));
	}
	VDBPrivatePtr->InsertFileMeta<std::string>("WorldName", TCHAR_TO_UTF8(*WorldName));
}

FString UVDBHandle::AddGrid(const FString &gridName, const FIntVector &indexStart, const FIntVector &indexEnd)
{
	const Vdb::Metadata::RegionMetadata regionMetaValue(WorldName, gridName, openvdb::CoordBBox(openvdb::Coord(indexStart.X, indexStart.Y, indexStart.Z), openvdb::Coord(indexEnd.X, indexEnd.Y, indexEnd.Z)));
	VDBPrivatePtr->InsertFileMeta<Vdb::Metadata::RegionMetadata>(regionMetaValue.ID(), regionMetaValue);
	return regionMetaValue.ID();
}

void UVDBHandle::RemoveGrid(const FString &gridID)
{
	VDBPrivatePtr->RemoveFileMeta(gridID);
	//TODO: Remove the grid from file
}

void UVDBHandle::ReadGridTree(const FString &gridID, FIntVector &indexStart, FIntVector &indexEnd)
{
	VDBPrivatePtr->ReadGridTree<TreeType, Vdb::Metadata::RegionMetadata>(gridID, indexStart, indexEnd);
}

void UVDBHandle::MeshGrid(const FString &gridID, float surfaceValue, TArray<FVector> &vertexBuffer, TArray<int32> &polygonBuffer, TArray<FVector> &normalBuffer)
{
	VDBPrivatePtr->MeshRegion<TreeType, Vdb::Metadata::RegionMetadata>(gridID, surfaceValue, vertexBuffer, polygonBuffer, normalBuffer);
}

void UVDBHandle::ReadGridIndexBounds(const FString &gridID, FIntVector &indexStart, FIntVector &indexEnd)
{
	VDBPrivatePtr->ReadGridIndexBounds<TreeType, Vdb::Metadata::RegionMetadata>(gridID, indexStart, indexEnd);
}

SIZE_T UVDBHandle::ReadGridCount()
{
	return 0;//TODO
}

void UVDBHandle::PopulateGridDensity_Perlin(const FString &gridID, double frequency, double lacunarity, double persistence, int octaveCount)
{
	VDBPrivatePtr->FillGrid_PerlinDensity<TreeType, Vdb::Metadata::RegionMetadata>(gridID, frequency, lacunarity, persistence, octaveCount);
}