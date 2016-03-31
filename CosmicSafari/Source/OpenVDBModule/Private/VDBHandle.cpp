#include "VdbHandle.h"
#include "IVdb.h"
#include "GridOps.h"
#include "GridMetadata.h"

////The standard OpenVDB tree configuration in which leaf nodes contain ValueType
//template<typename ValueType>
//using TreeType = openvdb::tree::Tree4<ValueType, 5, 4, 3>::Type;

template<typename TreeType, typename LogCategory, typename... MetadataTypes>
class VdbHandlePrivate : public Vdb::IVdbInterfaceDefinition<TreeType, ...MetadataTypes>
{
public:
	typedef Vdb::IVdbInterfaceDefinition<TreeType, ...MetadataTypes> IVdbType;
	typedef Vdb::GridOps::BasicMesher<TreeType> MesherOpType;

	TSharedPtr<openvdb::io::File> FilePtr;
	TSharedPtr<openvdb::GridPtrVec> GridsPtr;
	TSharedPtr<openvdb::MetaMap> FileMetaPtr;

	VdbHandlePrivate(const FString &path, bool enableDelayLoad, bool enableGridStats)
		: IVdbInterfaceDefinition(path, enableDelayLoad, enableGridStats)
	{
		//Initialize OpenVDB, our metadata types, and the vdb file
		openvdb::initialize();
		InitializeMetadataTypes();
		FilePtr = TSharedPtr<openvdb::io::File>(new openvdb::io::File(TCHAR_TO_UTF8(*path)));
		check(FilePtr.IsValid());
		FilePtr->setGridStatsMetadataEnabled(gridStatsIsEnabled);

		//Create the vdb file if it does not exist
		if (!FPaths::FileExists(path))
		{
			FilePtr->write(openvdb::GridPtrVec());
			UE_LOG(LogCategory, Verbose, TEXT("IVdb: Created %s"), *path);
		}
		check(FPaths::FileExists(path)); //TODO: Error handling when unable to create file. For now assume the file exists

		OpenFileGuard();
		GridsPtr = TSharedPtr<openvdb::GridPtrVec>(FilePtr->readAllGridMetadata().get());
		FileMetaPtr = TSharedPtr<openvdb::MetaMap>(FilePtr->getMetadata().get());
		check(GridsPtr.IsValid());
		check(FileMetaPtr.IsValid());
	}

	IVdbType::~IVdb()
	{
		//TODO: Write changes if they exist?
		openvdb::uninitialize();
	}

	template<typename MetadataType>
	void InitializeMetadata() override
	{
		if (!openvdb::TypedMetadata<MetadataType>::isRegisteredType())
		{
			TypedMetadata<MetadataType>::registerType();
		}
	}

	void OpenFileGuard() override
	{
		if (FilePtr.IsValid() && !FilePtr->isOpen())
		{
			FilePtr->open(DelayLoadIsEnabled);
		}
	}

	void CloseFileGuard() override
	{
		if (FilePtr.IsValid() && FilePtr->isOpen())
		{
			FilePtr->close();
		}
	}

	TSharedPtr<openvdb::MetaMap> GetGridMeta(const FString &gridName) override
	{
		TSharedPtr<openvdb::MetaMap> gridMetaPtr;
		TSharedPtr<IVdbGridType> gridPtr = GetGridPtr(gridName);
		if (gridPtr.isValid())
		{
			gridMetaPtr = TSharedPtr<openvdb::MetaMap>(gridPtr->copyMeta().get());
		}
		return gridMetaPtr;
	}

	TSharedPtr<IVdbGridType> ReadGridTree(const FString &gridName, FVector &start, FVector &end) override
	{
		OpenFileGuard();
		for (openvdb::GridPtrVec::iterator i = GridsPtr->begin(); i != GridsPtr->end(); ++i)
		{
			if (*i != nullptr && gridName == UTF8_TO_TCHAR(i->getName().c_str()) && i->tree().empty())
			{
				const openvdb::BBoxd bboxd(openvdb::Vec3d(start.X, start.Y, start.Z), openvdb::Vec3d(end.X, end.Y, end.Z));
				return TSharedPtr<IVdbGridType>(openvdb::gridPtrCast<IVdbGridType>(FilePtr->readGrid(TCHAR_TO_UTF8(*gridName), bboxd).get());
			}
		}
		return TSharedPtr<IVdbGridType>();
	}

	TSharedPtr<IVdbGridType> ReadGridTree(const FString &gridName, FIntVector &indexStart, FIntVector &indexEnd) override
	{
		OpenFileGuard();
		for (openvdb::GridPtrVec::iterator i = GridsPtr->begin(); i != GridsPtr->end(); ++i)
		{
			if (*i != nullptr && gridName == UTF8_TO_TCHAR(i->getName().c_str()) && i->tree().empty())
			{
				openvdb::CoordBBox bbox(openvdb::Coord(indexStart.X, indexStart.Y, indexStart.Z), openvdb::Coord(indexEnd.X, indexEnd.Y, indexEnd.Z));
				return TSharedPtr<IVdbGridType>(openvdb::gridPtrCast<IVdbGridType>(FilePtr->readGrid(TCHAR_TO_UTF8(*gridName), bbox).get());
			}
		}
		return TSharedPtr<IVdbGridType>();
	}

	template<typename FileMetaType>
	TSharedPtr<openvdb::TypedMetadata<FileMetaType>> GetFileMetaValue(const FString &metaName, const FileMetaType &metaValue) override
	{
		OpenFileGuard();
		return TSharedPtr<openvdb::TypedMetadata<FileMetaType>>(FileMetaPtr->getMetadata<FileMetaType>(TCHAR_TO_UTF8(*metaName).get());
	}

	template<typename FileMetaType>
	void InsertFileMeta(const FString &metaName, const FileMetaType &metaValue) override
	{
		CloseFileGuard();
		FileMetaPtr->insertMeta(TCHAR_TO_UTF8(*metaName), openvdb::TypedMetadata<FileMetaType>(metaValue));
	}

	void RemoveFileMeta(const FString &metaName) override
	{
		CloseFileGuard();
		FileMetaPtr->removeMeta(TCHAR_TO_UTF8(*metaName));
	}

	TSharedPtr<IVdbGridType> GetGridPtr(const FString &gridName)
	{
		for (openvdb::GridPtrVec::iterator i = GridsPtr->begin(); i != GridsPtr->end(); ++i)
		{
			if (gridName == UTF8_TO_TCHAR(i->getName().c_str()))
			{
				return TSharedPtr<IVdbGridType>(openvdb::gridPtrCast<IVdbGridType>(*i));
			}
		}
		return TSharedPtr<IVdbGridType>();
	}

	template<typename MetadataType>
	TSharedPtr<openvdb::TypedMetadata<MetadataType>> GetGridMetaValue(const FString &gridName, const FString &metaName) override
	{
		TSharedPtr<openvdb::TypedMetadata<MetadataType>> metaValuePtr;
		TSharedPtr<IVdbGridType> gridPtr = GetGridPtr(gridName);
		if (gridPtr.IsValid())
		{
			metaValuePtr = TSharedPtr<openvdb::TypedMetadata<MetadataType>>(gridPtr->getMetadata<openvdb::TypedMetadata<MetadataType>>(TCHAR_TO_UTF8(*gridName)).get());
		}
		return metaValuePtr;
	}

	template<typename MetadataType>
	void InsertGridMeta(const FString &gridName, const FString &metaName, const MetadataType &metaValue) override
	{
		TSharedPtr<IVdbGridType> gridPtr = GetGridPtr(gridName);
		if (gridPtr.IsValid())
		{
			gridPtr->insertMeta<openvdb::TypedMetadata<MetadataType>>(TCHAR_TO_UTF8(*metaName), openvdb::TypedMetadata<MetadataType>(metaValue));
		}
	}

	template<typename MetadataType>
	void RemoveGridMeta(const FString &gridName, const FString &metaName) override
	{
		TSharedPtr<IVdbGridType> gridPtr = GetGridPtr(gridName);
		if (gridPtr.IsValid())
		{
			gridPtr->removeMeta(TCHAR_TO_UTF8(*metaName));
		}
	}

	void WriteChanges() override
	{
		OpenFileGuard();
		FilePtr->write(*GridsPtr, *FileMetaPtr);
	}

	void MeshRegion(const FString &gridName, float surfaceValue, TArray<FVector> &vertexBuffer, TArray<int32> &polygonBuffer, TArray<FVector> &normalBuffer)
	{
		TSharedPtr<IVdbGridType> gridPtr = GetGridPtr(gridName);
		if (gridPtr.isValid())
		{
			TSharedPtr<MesherOpType> mesherOpPtr = TSharedPtr<MesherOpType>(new MesherOpType(gridPtr.get(), vertexBuffer, polygonBuffer, normalBuffer));
			mesherOpPtr->doActivateValuesOp(surfaceValue);
			mesherOpPtr->doMeshOp(true);
		}
	}

	void ReadGridIndexBounds(const FString &gridName, FIntVector &indexStart, FIntVector &indexEnd)
	{
		TSharedPtr<IVdbGridType> gridPtr = GetGridPtr(gridName);
		if (gridPtr.isValid())
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

	void FillGrid_PerlinDensity(const FString &gridName, double frequency, double lacunarity, double persistence, int octaveCount)
	{
		TSharedPtr<IVdbGridType> gridPtr = GetGridPtr(gridName);
		if (gridPtr.isValid())
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
				openvdb::CoordBBox bboxPadded = bbox;
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

typedef VdbHandlePrivate<openvdb::FloatTree, LogOpenVDBModule, Vdb::Metadata::RegionMetadata> VdbHandlePrivateType;
TSharedPtr<VdbHandlePrivateType> VDBPrivatePtr;

UVdbHandle(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UVdbHandle::Initialize_Implementation(const FString &path, bool enableDelayLoad, bool enableGridStats)
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
	VDBPrivatePtr->InsertFileMeta<openvdb::StringMetadata>("WorldName", WorldName);
}

FString UVdbHandle::AddGrid_Implementation(const FString &gridName, const FIntVector &indexStart, const FIntVector &indexEnd)
{
	const Vdb::Metadata::RegionMetadata regionMetaValue(gridName, openvdb::CoordBBox(openvdb::Coord(indexStart.X, indexStart.Y, indexStart.Z), openvdb::Coord(indexEnd.X, indexEnd.Y, indexEnd.Z)));
	VDBPrivatePtr->InsertFileMeta<Vdb::Metadata::RegionMetadata>(regionMetaValue.ID(), regionMetaValue);
	return  regionMeta.ID();
}

void UVdbHandle::RemoveGrid_Implementation(const FString &gridID)
{
	VDBPrivatePtr->RemoveFileMeta(gridID);
	//TODO: Remove the grid from file
}

void UVdbHandle::ReadGridTree_Implementation(const FString &gridID, const FIntVector &indexStart, const FIntVector &indexEnd, TArray<FVector> &vertexBuffer, TArray<int32> &polygonBuffer, TArray<FVector> &normalBuffer)
{
	VDBPrivatePtr->ReadGridTree(gridID, indexStart, indexEnd, vertexBuffer, polygonBuffer, normalBuffer);
}

void UVdbHandle::MeshGrid_Implementation(const FString &gridID, float surfaceValue)
{
	VDBPrivatePtr->MeshRegion(gridID, surfaceValue);
}

void UVdbHandle::ReadGridIndexBounds_Implementation(const FString &gridID, FIntVector &indexStart, FIntVector &indexEnd)
{
	VDBPrivatePtr->ReadGridIndexBounds(gridID, indexStart, indexEnd);
}

SIZE_T UVdbHandle::ReadGridCount_Implementation()
{
	return 0;//TODO
}

void UVdbHandle::PopulateRegionDensity_Perlin_Implementation(const FString &regionID, double frequency, double lacunarity, double persistence, int octaveCount)
{
	VDBPrivatePtr->FillGrid_PerlinDensity(gridID, frequency, lacunarity, persistence, octaveCount);
}