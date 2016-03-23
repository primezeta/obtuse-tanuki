#include "OpenVDBModule.h"
#pragma warning(push)
#pragma warning(1:4211 4800 4503 4146)
#include <openvdb/openvdb.h>
#include "GridOps.h"
#include "GridMetadata.h"

DEFINE_LOG_CATEGORY(LogOpenVDBModule)

typedef openvdb::TypedMetadata<Vdb::Metadata::RegionMetadata> RegionMetadataType;
//template<typename TransformType> openvdb::math::Transform CreateTransform(const typename TransformType &xform);

template<typename DataType>
class VdbHandle : public Vdb::VdbHandleBase
{
public:
	typedef typename openvdb::tree::Tree4<typename DataType, 5, 4, 3>::Type TreeType;
	typedef typename TreeType::Ptr TreeTypePtr;
	typedef typename openvdb::Grid<typename TreeType> GridType;
	typedef typename GridType::Ptr GridTypePtr;
	typedef typename Vdb::GridOps::BasicMesher<typename TreeType> BasicMesherType;
	typedef typename BasicMesherType::Ptr GridMesherTypePtr;

	VdbHandle(const FString &path, bool enableGridStats) : VdbHandleBase(path, FGuid::NewGuid(), enableGridStats)
	{
		openvdb::initialize();
		if (!RegionMetadataType::isRegisteredType())
		{
			RegionMetadataType::registerType();
		}
		FilePtr = TSharedPtr<openvdb::io::File>(new openvdb::io::File(TCHAR_TO_UTF8(*path)));
		check(FilePtr.IsValid());
		if (!FPaths::FileExists(path))
		{
			FilePtr->setGridStatsMetadataEnabled(gridStatsIsEnabled);
			FilePtr->write(openvdb::GridPtrVec());
		}
		check(FPaths::FileExists(path)); //TODO: Error handling when unable to create file. For now assume the file exists
		VdbHandle::InputInitializer(*FilePtr, gridStatsIsEnabled);
		Grids = FilePtr->getGrids();
		check(Grids != nullptr);
		GridMeta = FilePtr->getMetadata();
		check(GridMeta != nullptr);
	}

	~VdbHandle()
	{
		openvdb::uninitialize();
		FlushChanges();
	}

	void FlushChanges()
	{
		if (Grids)
		{
			if (GridMeta)
			{
				WriteChanges(*FilePtr, *Grids, *GridMeta);
			}
			else
			{
				WriteChanges(*FilePtr, *Grids);
			}
		}
		else if (FilePtr->isOpen())
		{
			FilePtr->close();
		}
	}

	void InsertGrid(const FString &name, const openvdb::math::Transform &xform)
	{
		GridTypePtr gridPtr = GridType::create(TreeTypePtr(new TreeType()));
		check(gridPtr != nullptr);
		VdbHandle::InputInitializer(*FilePtr, gridStatsIsEnabled);
		const std::string gridName = TCHAR_TO_UTF8(*name);
		gridPtr->setName(gridName);
		gridPtr->setTransform(openvdb::math::Transform::Ptr(new openvdb::math::Transform(xform)));
		if (FilePtr->hasGrid(gridName))
		{
			//Replace the existing grid
			auto i = Grids->begin();
			for (; i != Grids->end() && (*i)->getName() != gridName; ++i);
			if (i != Grids->end())
			{
				Grids->erase(i);
			}
		}
		Grids->push_back(gridPtr);
		FlushChanges();
	}

	void ReadRegionTree(const FString &regionID, TArray<FVector> &vertexBuffer, TArray<int32> &polygonBuffer, TArray<FVector> &normalBuffer)
	{
		check(GridMeta != nullptr);
		RegionMetadataType::Ptr regionMetaPtr = GridMeta->getMetadata<RegionMetadataType>(TCHAR_TO_UTF8(*regionID));
		if (regionMetaPtr && !MeshRegions.Contains(regionID))
		{
			//Open the vdb file without delayed loading so that the region is fully loaded (at least, I think that's how delayed loading works)
			VdbHandle::InputInitializer(*FilePtr, gridStatsIsEnabled, false);
			
			const std::string parentGridName = TCHAR_TO_UTF8(*regionMetaPtr->value().GetParentGridName());
			auto i = Grids->begin();
			for (; i != Grids->end() && (*i)->getName() != parentGridName; ++i);
			check(i != Grids->end());
			GridTypePtr parentGrid = openvdb::gridPtrCast<GridType>(*i);
			const openvdb::BBoxd bboxd = parentGrid->transform().indexToWorld(regionMetaPtr->value().GetRegionBBox());

			GridTypePtr regionGridPtr = openvdb::gridPtrCast<GridType>(FilePtr->readGrid(parentGridName, bboxd));
			if (regionGridPtr)
			{
				regionGridPtr->setName(TCHAR_TO_UTF8(*regionID));
				MeshRegions.Emplace(regionID, GridMesherTypePtr(new BasicMesherType(regionGridPtr, vertexBuffer, polygonBuffer, normalBuffer)));
			}
		}
	}

	void RemoveRegion(const FString &regionID)
	{
		GridMeta->removeMeta(TCHAR_TO_UTF8(*regionID));
		MeshRegions.Remove(regionID);
	}

	void FillRegion(const FString &regionID, openvdb::BoolGrid::Ptr regionMask, double frequency, double lacunarity, double persistence, int octaveCount)
	{
		auto regionMeshPtr = MeshRegions.Find(regionID);
		if (regionMeshPtr != nullptr)
		{
			Vdb::GridOps::PerlinNoiseFillOp<TreeType> noiseFillOp((*regionMeshPtr)->gridPtr->transform(), frequency, lacunarity, persistence, octaveCount);
			openvdb::tools::transformValues(regionMask->cbeginValueOn(), *(*regionMeshPtr)->gridPtr, noiseFillOp);
		}
	}

	void MeshSurface(const FString &regionID, float surfaceValue)
	{
		auto regionMeshPtr = MeshRegions.Find(regionID);
		if (regionMeshPtr != nullptr)
		{
			//Run the surface extraction operation
			(*regionMeshPtr)->doActivateValuesOp(surfaceValue);
			//Mesh the extracted surface
			(*regionMeshPtr)->doMeshOp(true);
			//Now that meshing is done the region values are no longer needed. Merge the region grid into the parent grid, destroying the region grid in the process
			auto regionMeta = GridMeta->getMetadata<RegionMetadataType>(TCHAR_TO_UTF8(*regionID));
			check(regionMeta != nullptr);
			auto i = Grids->begin();
			const std::string parentGridName = TCHAR_TO_UTF8(*(regionMeta->value().GetParentGridName()));
			for (; i != Grids->end() && (*i)->getName() != parentGridName; ++i);
			check(i != Grids->end());
			GridTypePtr parentGrid = openvdb::gridPtrCast<GridType>(*i);
			check(parentGrid != nullptr);
			parentGrid->merge(*((*regionMeshPtr)->gridPtr));
		}
	}

	static void InputInitializer(openvdb::io::File &vdb, bool gridStatsIsEnabled, bool delayLoad = true)
	{
		//Open the file to allow reading
		if (!vdb.isOpen())
		{
			vdb.open(delayLoad);
		}
		check(vdb.isOpen()); //TODO: Handle openvdb IO exceptions
		check(vdb.isGridStatsMetadataEnabled() == gridStatsIsEnabled);
	}

	static void WriteChanges(openvdb::io::File &vdb, const openvdb::GridPtrVec &outGrids)
	{
		//OpenVDB files must be closed prior to writing
		if (vdb.isOpen())
		{
			vdb.close();
		}
		check(!vdb.isOpen()); //TODO: Handle openvdb IO exceptions
		vdb.write(outGrids);
	}

	static void WriteChanges(openvdb::io::File &vdb, const openvdb::GridPtrVec &outGrids, const openvdb::MetaMap &outMeta)
	{
		//OpenVDB files must be closed prior to writing
		if (vdb.isOpen())
		{
			vdb.close();
		}
		check(!vdb.isOpen()); //TODO: Handle openvdb IO exceptions
		vdb.write(outGrids, outMeta);
	}

	openvdb::MetaMap::Ptr GridMeta;
	openvdb::GridPtrVecPtr Grids;
private:
	TSharedPtr<openvdb::io::File> FilePtr;
	TMap<FString, typename GridMesherTypePtr> MeshRegions;
};

template<typename DataType>
inline TSharedRef<VdbHandle<typename DataType>> GetAndCheckHandle(TMap<FGuid, Vdb::HandleType> &handles, Vdb::HandleType &baseHandle)
{
	return StaticCastSharedRef<VdbHandle<DataType>, Vdb::VdbHandleBase>(handles[baseHandle->GUID]);
}

Vdb::HandleType FOpenVDBModule::CreateVDB(const FString &path, bool enableGridStats)
{
	for (auto i = VdbHandles.CreateIterator(); i; ++i)
	{
		if (i->Value->Path == path)
		{
			return GetAndCheckHandle<GridDataType>(VdbHandles, i->Value);
		}
	}
	Vdb::HandleType newHandle(new VdbHandle<GridDataType>(path, enableGridStats));
	return VdbHandles.Add(newHandle->GUID, newHandle);
}

void FOpenVDBModule::InitializeGrid(Vdb::HandleType handle, const FString &gridName, const GridTransformType &xform)
{
	TSharedRef<VdbHandle<GridDataType>> checkedHandle = GetAndCheckHandle<GridDataType>(VdbHandles, handle);
	checkedHandle->InsertGrid(gridName, openvdb::math::Transform(openvdb::math::ScaleMap::Ptr(new openvdb::math::ScaleMap(openvdb::Vec3d(xform.Scale, xform.Scale, xform.Scale)))));
}

FString FOpenVDBModule::AddRegionDefinition(Vdb::HandleType handle, const FString &gridName, const FString &regionName, const FIntVector &indexStart, const FIntVector &indexEnd)
{
	FString regionID;
	TSharedRef<VdbHandle<GridDataType>> checkedHandle = GetAndCheckHandle<GridDataType>(VdbHandles, handle);
	const Vdb::Metadata::RegionMetadata regionMeta(gridName, regionName, openvdb::CoordBBox(openvdb::Coord(indexStart.X, indexStart.Y, indexStart.Z), openvdb::Coord(indexEnd.X, indexEnd.Y, indexEnd.Z)));
	regionID = regionMeta.ID();
	checkedHandle->GridMeta->insertMeta(TCHAR_TO_UTF8(*regionID), RegionMetadataType(regionMeta));
	return regionID;
}

FString FOpenVDBModule::AddRegion(Vdb::HandleType handle, const FString &gridName, const FString &regionName, const FIntVector &indexStart, const FIntVector &indexEnd, TArray<FVector> &vertexBuffer, TArray<int32> &polygonBuffer, TArray<FVector> &normalBuffer)
{
	FString regionID;
	TSharedRef<VdbHandle<GridDataType>> checkedHandle = GetAndCheckHandle<GridDataType>(VdbHandles, handle);
	const Vdb::Metadata::RegionMetadata regionMeta(gridName, regionName, openvdb::CoordBBox(openvdb::Coord(indexStart.X, indexStart.Y, indexStart.Z), openvdb::Coord(indexEnd.X, indexEnd.Y, indexEnd.Z)));
	regionID = regionMeta.ID();
	checkedHandle->GridMeta->insertMeta(TCHAR_TO_UTF8(*regionID), RegionMetadataType(regionMeta));
	checkedHandle->ReadRegionTree(regionID, vertexBuffer, polygonBuffer, normalBuffer);
	return regionID;
}

void FOpenVDBModule::RemoveRegion(Vdb::HandleType handle, const FString &regionID)
{
	TSharedRef<VdbHandle<GridDataType>> checkedHandle = GetAndCheckHandle<GridDataType>(VdbHandles, handle);
	checkedHandle->RemoveRegion(regionID);
}

void FOpenVDBModule::LoadRegion(Vdb::HandleType handle, const FString &regionID, TArray<FVector> &vertexBuffer, TArray<int32> &polygonBuffer, TArray<FVector> &normalBuffer)
{
	TSharedRef<VdbHandle<GridDataType>> checkedHandle = GetAndCheckHandle<GridDataType>(VdbHandles, handle);
	checkedHandle->ReadRegionTree(regionID, vertexBuffer, polygonBuffer, normalBuffer);
}

void FOpenVDBModule::MeshRegion(Vdb::HandleType handle, const FString &regionID, float surfaceValue)
{
	TSharedRef<VdbHandle<GridDataType>> checkedHandle = GetAndCheckHandle<GridDataType>(VdbHandles, handle);
	checkedHandle->MeshSurface(regionID, surfaceValue);
}

void FOpenVDBModule::ReadRegionIndexBounds(Vdb::HandleType handle, const FString &regionID, FIntVector &indexStart, FIntVector &indexEnd)
{
	TSharedRef<VdbHandle<GridDataType>> checkedHandle = GetAndCheckHandle<GridDataType>(VdbHandles, handle);
	auto regionMeta = checkedHandle->GridMeta->getMetadata<RegionMetadataType>(TCHAR_TO_UTF8(*regionID));
	if (regionMeta)
	{
		//Get the index space bounding box
		const openvdb::CoordBBox bbox = regionMeta->value().GetRegionBBox();
		indexStart[0] = bbox.min().x();
		indexStart[1] = bbox.min().y();
		indexStart[2] = bbox.min().z();
		indexEnd[0] = bbox.max().x();
		indexEnd[1] = bbox.max().y();
		indexEnd[2] = bbox.max().z();
	}
}

SIZE_T FOpenVDBModule::ReadMetaRegionCount(Vdb::HandleType handle)
{
	SIZE_T count = 0;
	TSharedRef<VdbHandle<GridDataType>> checkedHandle = GetAndCheckHandle<GridDataType>(VdbHandles, handle);
	//Count all region metadata items
	for (auto i = checkedHandle->GridMeta->beginMeta(); i != checkedHandle->GridMeta->endMeta(); ++i)
	{
		if (i->second->typeName() == RegionMetadataType::staticTypeName())
		{
			count++;
		}
	}
	return count;
}

void FOpenVDBModule::PopulateRegionDensity_Perlin(Vdb::HandleType handle, const FString &regionID, double frequency, double lacunarity, double persistence, int octaveCount)
{
	TSharedRef<VdbHandle<GridDataType>> checkedHandle = GetAndCheckHandle<GridDataType>(VdbHandles, handle);
	openvdb::DoubleMetadata::Ptr frequencyMeta = checkedHandle->GridMeta->getMetadata<openvdb::DoubleMetadata>("frequency");
	openvdb::DoubleMetadata::Ptr lacunarityMeta = checkedHandle->GridMeta->getMetadata<openvdb::DoubleMetadata>("lacunarity");
	openvdb::DoubleMetadata::Ptr persistenceMeta = checkedHandle->GridMeta->getMetadata<openvdb::DoubleMetadata>("persistence");
	openvdb::Int32Metadata::Ptr octaveCountMeta = checkedHandle->GridMeta->getMetadata<openvdb::Int32Metadata>("octaveCount");
	auto regionMeta = checkedHandle->GridMeta->getMetadata<RegionMetadataType>(TCHAR_TO_UTF8(*regionID));
	if (regionMeta &&
		(frequencyMeta == nullptr || !openvdb::math::isApproxEqual(frequency, frequencyMeta->value()) ||
			lacunarityMeta == nullptr || !openvdb::math::isApproxEqual(lacunarity, lacunarityMeta->value()) ||
			persistenceMeta == nullptr || !openvdb::math::isApproxEqual(persistence, persistenceMeta->value()) ||
			octaveCountMeta == nullptr || !openvdb::math::isExactlyEqual(octaveCount, octaveCountMeta->value())))
	{
		//Update the Perlin noise parameters
		checkedHandle->GridMeta->insertMeta("frequency", openvdb::DoubleMetadata(frequency));
		checkedHandle->GridMeta->insertMeta("lacunarity", openvdb::DoubleMetadata(lacunarity));
		checkedHandle->GridMeta->insertMeta("persistence", openvdb::DoubleMetadata(persistence));
		checkedHandle->GridMeta->insertMeta("octaveCount", openvdb::Int32Metadata(octaveCount));

		//Activate mask values such that there is a single padded region along the outer edge with
		//values on and false and all other values within the padded region have values on and true.
		openvdb::CoordBBox bbox = regionMeta->value().GetRegionBBox();
		check(!bbox.empty());
		openvdb::CoordBBox bboxPadded = bbox;
		bboxPadded.expand(1);

		//Create a mask enclosing the region such that the outer edge voxels are on but false
		openvdb::BoolGrid::Ptr mask = openvdb::BoolGrid::create(false);
		mask->fill(bboxPadded, /*value*/false, /*state*/true);
		mask->fill(bbox, /*value*/true, /*state*/true);
		checkedHandle->FillRegion(regionID, mask, frequency, lacunarity, persistence, octaveCount);
	}
}

//template<> openvdb::math::Transform CreateTransform<Vdb::ScaleTransformType>(Vdb::ScaleTransformType &xform)
//{
//	return openvdb::math::Transform(openvdb::math::ScaleMap::Ptr(new openvdb::math::ScaleMap(openvdb::Vec3d(xform.Scale.X, xform.Scale.Y, xform.Scale.Z))));
//}
//template<> openvdb::math::Transform CreateTransform<Vdb::TranslateTransformType>(const Vdb::TranslateTransformType &xform)
//{
//	return openvdb::math::Transform(openvdb::math::TranslationMap::Ptr(new openvdb::math::TranslationMap(openvdb::Vec3d(xform.Translation.X, xform.Translation.Y, xform.Translation.Z))));
//}
//template<> openvdb::math::Transform CreateTransform<>(Vdb::ScaleTranslateTransformType)
//{
//}
//template<> openvdb::math::Transform CreateTransform<>(Vdb::UniformScaleTransformType)
//{
//}
//template<> openvdb::math::Transform CreateTransform<>(Vdb::UniformScaleTranslateTransformType)
//{
//}
//template<> openvdb::math::Transform CreateTransform<>(Vdb::AffineTransformType)
//{
//}
//template<> openvdb::math::Transform CreateTransform<>(Vdb::UnitaryTransformType)
//{
//}
//template<> openvdb::math::Transform CreateTransform<>(Vdb::NonlinearFrustumTransformType)
//{
//}

IMPLEMENT_GAME_MODULE(FOpenVDBModule, OpenVDBModule);