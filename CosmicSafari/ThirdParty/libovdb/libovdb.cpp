#include "OvdbTypes.h"
#include "OvdbVolume.h"
#include "OvdbNoise.h"
#include <unordered_map>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string.hpp>
#pragma warning(disable: 4503)

boost::shared_ptr<Ovdb> shared_instance = nullptr;
boost::shared_ptr<openvdb::io::File> instance_file = nullptr;
openvdb::MetaMap::Ptr instance_meta = nullptr;
openvdb::GridPtrVecPtr instance_grids = nullptr;
boost::shared_ptr<std::unordered_map<std::string, openvdb::GridBase::Ptr>> instance_grid_map = nullptr;

Ovdb * IOvdb::GetIOvdbInstance(const char * vdbFilename)
{
	//Connect the shared instance with the specified vdb file
	if (instance_file != nullptr && instance_file->filename() != std::string(vdbFilename))
	{
		//Filename changed - shut down the current instance
		shared_instance.reset();
	}
	if (shared_instance == nullptr)
	{
		//Create the instance 
		shared_instance = boost::shared_ptr<Ovdb>(new Ovdb(vdbFilename));
	}
	return shared_instance.get();
}

using namespace ovdb; //TODO: Remove 'using' to match UE4 coding standards
using namespace ovdb::meshing; //TODO: Remove 'using' to match UE4 coding standards
using namespace ovdb::tools;

typedef openvdb::FloatTree ValueTree;
typedef openvdb::Grid<ValueTree> ValueGrid;
typedef openvdb::TypedMetadata<openvdb::BBoxd> BBoxdMetadata;
typedef openvdb::TypedMetadata<std::string> RegionNamesMetadata;

void GridInputInitializer(bool delayLoad);
std::string ConstructMetaRecordStr(std::vector<std::string> strs);
std::vector<std::string> ParseMetaRecordStr(const std::string &metaRecordStr);
std::string ConstructRegionID(const std::string &gridName, const std::string &regionName);
openvdb::BBoxd ReadRegionBBoxd(const std::string &regionID);

template<typename ValueType>
struct BasicModifyOp
{
	const ValueType val;
	BasicModifyOp(const ValueType& v) : val(v) {}
	virtual inline void operator()(ValueType& v) const { v = val; }
};

template<typename ValueType>
struct BasicModifyActiveOp
{
	const ValueType val;
	const bool isActive;
	BasicModifyActiveOp(const ValueType& v, const bool &active) : val(v), isActive(active) {}
	virtual inline void operator()(ValueType& v, bool &activeState) const { v = val; activeState = isActive; }
};

template<typename InIterType, typename OutTreeType>
class TransformOp
{
public:
	typedef typename InIterType IterType;
	typedef typename InIterType::ValueT InValueType;
	typedef typename OutTreeType::ValueType OutValueType;
	typedef typename openvdb::tree::ValueAccessor<OutTreeType> OutAccessorType;
	virtual void operator()(const IterType &iter, OutAccessorType& acc) = 0;
};

//Operator to create a grid of vectors from a scalar
template <typename OutTreeType, typename InTreeType = openvdb::BoolTree>
class PerlinNoiseFillOp : public TransformOp<typename InTreeType::ValueOnCIter, OutTreeType>
{
public:
	typedef typename BasicModifyActiveOp<OutValueType> ModifyOpType;
	PerlinNoiseFillOp(openvdb::math::Transform &xform, double frequency, double lacunarity, double persistence, int octaveCount)
		: inTreeXform(xform)
	{
		valueSource.SetFrequency(frequency);
		valueSource.SetLacunarity(lacunarity);
		valueSource.SetPersistence(persistence);
		valueSource.SetOctaveCount(octaveCount);
	}
	void operator()(const IterType& iter, OutAccessorType& acc) override
	{
		const bool isInsideBoundary = iter.getValue();
		if (iter.isVoxelValue())
		{
			const openvdb::Coord &coord = iter.getCoord();
			//Set the density value from the noise source and set on if the voxel is in the active boundary
			const openvdb::Vec3d vec = inTreeXform.indexToWorld(coord);
			acc.modifyValueAndActiveState<ModifyOpType>(coord, ModifyOpType((OutValueType)(valueSource.GetValue(vec.x(), vec.y(), vec.z()) - vec.z()), isInsideBoundary));
		}
		else
		{
			openvdb::CoordBBox bbox;
			iter.getBoundingBox(bbox);
			openvdb::Coord coord;
			for (auto x = bbox.min().x(); x <= bbox.max().x(); ++x)
			{
				coord.setX(x);
				for (auto y = bbox.min().y(); y <= bbox.max().y(); ++y)
				{
					coord.setY(y);
					for (auto z = bbox.min().z(); z <= bbox.max().z(); ++z)
					{
						coord.setZ(z);
						const openvdb::Vec3d vec = inTreeXform.indexToWorld(coord);
						acc.modifyValueAndActiveState<ModifyOpType>(coord, ModifyOpType((OutValueType)(valueSource.GetValue(vec.x(), vec.y(), vec.z()) - vec.z()), isInsideBoundary));
					}
				}
			}
		}
	}
private:
	const openvdb::math::Transform &inTreeXform;
	noise::module::Perlin valueSource;
};

//Operator to extract an isosurface from a grid
template <typename OutTreeType, typename InTreeType = OutTreeType>
class ExtractSurfaceOp : public TransformOp<typename InTreeType::ValueOnIter, OutTreeType>
{
public:
	typedef typename boost::shared_ptr<ExtractSurfaceOp<OutTreeType>> Ptr;
	void SetSurfaceValue(InValueType isovalue)
	{
		surfaceValue = isovalue;
	}
	void operator()(const IterType& iter, OutAccessorType& acc) override
	{
		const openvdb::Coord &coord = iter.getCoord();
		uint8_t insideBits = 0;
		//For each neighboring value set a bit if it is inside the surface (inside = positive value)
		if (iter.getValue() > surfaceValue) { insideBits |= 1; }
		if (acc.getValue(coord.offsetBy(1, 0, 0)) > surfaceValue) { insideBits |= 2; }
		if (acc.getValue(coord.offsetBy(0, 1, 0)) > surfaceValue) { insideBits |= 4; }
		if (acc.getValue(coord.offsetBy(0, 0, 1)) > surfaceValue) { insideBits |= 8; }
		if (acc.getValue(coord.offsetBy(1, 1, 0)) > surfaceValue) { insideBits |= 16; }
		if (acc.getValue(coord.offsetBy(1, 0, 1)) > surfaceValue) { insideBits |= 32; }
		if (acc.getValue(coord.offsetBy(0, 1, 1)) > surfaceValue) { insideBits |= 64; }
		if (acc.getValue(coord.offsetBy(1, 1, 1)) > surfaceValue) { insideBits |= 128; }
		//If all vertices are inside the surface or all are outside the surface then set off in order to not mesh this voxel
		iter.setActiveState(insideBits > 0 && insideBits < 255);
	}
private:
	InValueType surfaceValue;
};

//Helper struct to hold the associated grid meshing info
template <typename TreeType>
class BasicMesher
{
public:
	typedef typename boost::shared_ptr<BasicMesher<TreeType>> Ptr;
	typedef typename ExtractSurfaceOp<TreeType> ActivateValuesOpType;
	typedef typename OvdbVoxelVolume<TreeType> MeshOpType;
	typedef typename openvdb::Grid<TreeType> GridType;
	typedef typename GridType::Ptr GridPtrType;
	const GridPtrType gridPtr;
	const typename MeshOpType::Ptr meshOp;
	BasicMesher(GridPtrType grid) : gridPtr(grid), meshOp(new MeshOpType(grid)), activateValuesOp(new ActivateValuesOpType()) {}
	void doActivateValuesOp(typename ActivateValuesOpType::InValueType isovalue)
	{
		activateValuesOp->SetSurfaceValue(isovalue);
		openvdb::tools::transformValues(gridPtr->beginValueOn(), *gridPtr, *activateValuesOp);
	}
	void doMeshOp()
	{
		meshOp->initializeRegion();
		meshOp->doPrimitiveCubesMesh();
	}
private:
	typename ActivateValuesOpType::Ptr activateValuesOp;
};
typedef BasicMesher<ValueTree> RegionMeshType;
std::unordered_map<std::string, RegionMeshType::Ptr> regionPtrMap;

void GridInputInitializer(bool delayLoad)
{
	//Open the file to allow reading
	if (!instance_file->isOpen())
	{
		instance_file->open(delayLoad);
	}
	//openvdb::FloatGrid::Accessor a;
	//openvdb::FloatGrid::ValueOnIter i;i.getTree()->
	assert(instance_file->isOpen()); //TODO: Handle openvdb IO exceptions
}

std::string ConstructMetaRecordStr(std::vector<std::string> strs)
{
	//\x1B is the escape sequence char
	//\x1e is the record seperator char
	//First escape these two special chars then add a record seperator between grid and region names
	std::ostringstream recordStr;
	for (auto i = strs.begin(); i != strs.end(); ++i)
	{
		boost::replace_all(*i, "\x1B", "\x1B\x1B"); //Escape all escape chars
		boost::replace_all(*i, "\x1e", "\x1B\x1e"); //Escape all record seperator chars
		recordStr << *i << "\x1e"; //Insert a record seperator after the record
	}
	return recordStr.str();
}

std::vector<std::string> ParseMetaRecordStr(const std::string &metaRecordStr)
{
	std::vector<std::string> strs;
	boost::split(strs, metaRecordStr, boost::is_any_of("\x1e"));
	return strs;
}

std::string ConstructRegionID(const std::string &gridName, const std::string &regionName)
{
	std::vector<std::string> metaNameStrs;
	metaNameStrs.push_back(gridName);
	metaNameStrs.push_back("region");
	metaNameStrs.push_back(regionName);
	return ConstructMetaRecordStr(metaNameStrs);
}

std::string ParseRegionName(const std::string &regionID)
{
	std::string regionName;
	const std::vector<std::string> metaStrs = ParseMetaRecordStr(regionID);
	//Count all regions except the entire grid (which has an empty region name)
	if (metaStrs.size() > 2 && metaStrs[1] == "region")
	{
		regionName = metaStrs[2];
	}
	return regionName;
}

Ovdb::Ovdb(const char * vdbFilename)
{
	assert(shared_instance == nullptr);
	assert(instance_file == nullptr);
	assert(instance_grid_map == nullptr);
	assert(instance_grids == nullptr);
	openvdb::initialize();
	if (!BBoxdMetadata::isRegisteredType())
	{
		BBoxdMetadata::registerType();
	}
	if (!RegionNamesMetadata::isRegisteredType())
	{
		RegionNamesMetadata::registerType();
	}

	//Open the file or create it if it does not exist
	//TODO: Handle openvdb exceptions (especially IO exceptions)
	instance_file = boost::shared_ptr<openvdb::io::File>(new openvdb::io::File(vdbFilename));
	if (!boost::filesystem::exists(instance_file->filename()))
	{
		instance_file->setGridStatsMetadataEnabled(true);
		instance_file->write(openvdb::GridPtrVec());
	}
	//TODO: Error handling when unable to create file. For now assume the file exists
	assert(boost::filesystem::exists(instance_file->filename()));
	//TODO: Throw error when file was made with a different library version
	//std::string fileVersion = instance_file->version();
	//std::string libVersion = openvdb::getLibraryVersionString();
	GridInputInitializer(true);
	instance_grids = instance_file->getGrids();
	assert(instance_grids);
	instance_meta = instance_file->getMetadata();
	assert(instance_meta);
	instance_grid_map = boost::shared_ptr<std::unordered_map<std::string, openvdb::GridBase::Ptr>>(new std::unordered_map<std::string, openvdb::GridBase::Ptr>());
	assert(instance_grid_map);
}

Ovdb::~Ovdb()
{
	openvdb::uninitialize();
	if (instance_file != nullptr && instance_file->isOpen())
	{
		instance_file->close();
	}
	instance_file.reset();
	instance_grid_map.reset();
	instance_grids.reset();
}

int Ovdb::DefineGrid(const char * const gridName, double sx, double sy, double sz, int x0, int y0, int z0, int x1, int y1, int z1)
{
	//Initialize a grid with the specified name, scale transform, and bounding box and write any and all changes to the vdb file
	GridInputInitializer(true);
	ValueGrid::Ptr grid;
	if (!instance_file->hasGrid(gridName))
	{
		//Initialize a default grid with the specified name
		grid = ValueGrid::create(ValueTree::Ptr(new ValueTree()));
		grid->setName(gridName);
		instance_grids->push_back(grid);
	}
	else
	{
		auto i = instance_grids->begin();
		for (; i != instance_grids->end() && (*i)->getName() != gridName; ++i);
		assert(i != instance_grids->end());
		grid = openvdb::gridPtrCast<ValueGrid>(*i);
	}
	assert(grid);
	instance_grid_map->emplace(gridName, grid);

	//Compare the bounding box and transform from the file with the specified bounding box and transform
	const openvdb::Vec3d scaleVec(sx, sy, sz);
	openvdb::math::Transform::Ptr gridXform = grid->transformPtr();
	openvdb::math::Transform::Ptr scaleXform = openvdb::math::Transform::Ptr(new openvdb::math::Transform(openvdb::math::ScaleMap::Ptr(new openvdb::math::ScaleMap(scaleVec))));
	const std::string metaNameBBox = ConstructRegionID(gridName, "");
	auto meta = instance_meta->getMetadata<BBoxdMetadata>(metaNameBBox);
	const openvdb::BBoxd &gridBBoxd = meta ? meta->value() : openvdb::BBoxd();
	const openvdb::Coord bboxStart(openvdb::Coord(x0, y0, z0));
	const openvdb::Coord bboxEnd(openvdb::Coord(x1, y1, z1));
	const openvdb::BBoxd bboxd = openvdb::BBoxd(scaleXform->indexToWorld(bboxStart), scaleXform->indexToWorld(bboxEnd));
	
	//If the bounding box or transform changed update the grid
	if(gridBBoxd.empty() || gridBBoxd != bboxd || gridXform != scaleXform)
	{
		grid->setTransform(scaleXform);
		//Check specifically for bbox change since it could be an expensive change
		if (bboxd != gridBBoxd)
		{
			//TODO: What if the scale is changed in such a way that the CoordBBox is the same yet the set value op is different due to a changed scale?
			instance_meta->insertMeta(metaNameBBox, BBoxdMetadata(bboxd));
			if (!gridBBoxd.isInside(bboxd))
			{
				grid->clear();
			}
		}
	}
	WriteChanges();
	return 0;
}

int Ovdb::DefineRegion(const char * const gridName, const char * const regionName, int x0, int y0, int z0, int x1, int y1, int z1, char * regionIDStr, size_t maxStrLen)
{
	//Insert the new region ID or replace an existing region ID of the same name
	GridInputInitializer(true);
	const std::string metaRegionID = ConstructRegionID(gridName, regionName);
	const std::string regionID = metaRegionID.substr(0, maxStrLen < metaRegionID.size() ? maxStrLen : metaRegionID.size());
	std::strcpy(regionIDStr, regionID.c_str());

	openvdb::math::Transform::Ptr xform = instance_grid_map->at(gridName)->transformPtr();
	auto bboxdMeta = instance_meta->getMetadata<BBoxdMetadata>(regionID);
	const openvdb::CoordBBox bbox(openvdb::Coord(x0, y0, z0), openvdb::Coord(x1, y1, z1));
	openvdb::CoordBBox regionBBox;
	openvdb::BBoxd metaBBoxd;
	if (bboxdMeta)
	{
		metaBBoxd = bboxdMeta->value();
		regionBBox = xform->worldToIndexNodeCentered(metaBBoxd);
	}
	if (regionBBox.empty() || regionBBox != bbox)
	{
		metaBBoxd = openvdb::BBoxd(xform->indexToWorld(bbox));
		instance_meta->insertMeta(regionID, BBoxdMetadata(metaBBoxd));
		WriteChanges();
	}

	GridInputInitializer(true);
	ValueGrid::Ptr grid = openvdb::gridPtrCast<ValueGrid>(instance_file->readGrid(gridName, metaBBoxd));
	grid->setName(regionID);
	instance_grids->push_back(grid);
	return 0;
}

int Ovdb::RemoveRegion(const char * const gridName, const char * const regionName)
{
	GridInputInitializer(true);
	const std::string regionID = ConstructRegionID(gridName, regionName);
	auto regionIter = regionPtrMap.find(regionID);
	if (regionIter != regionPtrMap.end())
	{
		regionPtrMap.erase(regionID);
	}
	if (instance_meta->getMetadata<BBoxdMetadata>(regionID))
	{
		instance_meta->removeMeta(regionID);
		WriteChanges();
	}
	return 0;
}

int Ovdb::ReadRegionBounds(const char * const gridName, const char * const regionName, int &x0, int &y0, int &z0, int &x1, int &y1, int &z1)
{
	GridInputInitializer(true);
	const openvdb::BBoxd regionBBoxd = ReadRegionBBoxd(ConstructRegionID(gridName, regionName));
	const openvdb::math::Transform::Ptr xform = instance_grid_map->at(gridName)->transformPtr();
	assert(xform);
	const openvdb::CoordBBox bbox = xform->worldToIndexNodeCentered(regionBBoxd);
	x0 = bbox.min().x();
	y0 = bbox.min().y();
	z0 = bbox.min().z();
	x1 = bbox.max().x();
	y1 = bbox.max().y();
	z1 = bbox.max().z();
	return regionBBoxd.empty() ? 1 : 0;
}

size_t Ovdb::ReadMetaRegionCount()
{
	GridInputInitializer(true);
	size_t count = 0;
	for (auto i = instance_meta->beginMeta(); i != instance_meta->endMeta(); ++i)
	{
		//Count all regions except the entire grid (which has an empty region name)
		if (!ParseRegionName(i->first).empty())
		{
			count++;
		}
	}
	return count;
}

int Ovdb::WriteChanges()
{
	//TODO: Handle openvdb IO exceptions
	//Close the file to allow writing
	if (instance_file->isOpen())
	{
		instance_file->close();
	}
	assert(!instance_file->isOpen()); //TODO: Handle openvdb IO exceptions
	instance_file->write(*instance_grids, *instance_meta);
	return 0;
}

int Ovdb::LoadRegion(const char * const regionID)
{
	GridInputInitializer(false);
	const openvdb::BBoxd regionBBoxd = ReadRegionBBoxd(regionID);
	if (!regionBBoxd.empty())
	{
		auto regionIter = regionPtrMap.find(regionID);
		if (regionIter == regionPtrMap.end())
		{
			ValueGrid::Ptr grid;
			if (instance_file->hasGrid(regionID))
			{
				grid = openvdb::gridPtrCast<ValueGrid>(instance_file->readGrid(regionID));
				regionPtrMap.emplace(regionID, RegionMeshType::Ptr(new RegionMeshType(grid)));
			}
		}
	}
	return regionBBoxd.empty() ? 1 : 0;
}

int Ovdb::ReadMetaRegionIDs(char ** regionIDList, size_t regionCount, size_t strMaxLen)
{
	//Return a c-style array of c-style ID strings
	GridInputInitializer(true);
	size_t numIDs = 0;
	for (auto i = instance_meta->beginMeta(); i != instance_meta->endMeta(); ++i)
	{
		const std::string regionName = ParseRegionName(i->first);
		if (!regionName.empty())
		{
			const std::string &regionID = i->first;
			if (regionID.size() <= strMaxLen) //TODO: Log an error if the region ID string is too large?
			{
				char * strStart = regionIDList[sizeof(char*)*(numIDs++)];
				strcpy_s(strStart, regionID.size(), regionID.c_str());
			}
		}
	}
	return numIDs;
}

openvdb::BBoxd ReadRegionBBoxd(const std::string &regionID)
{
	GridInputInitializer(true);
	openvdb::BBoxd regionBBoxd;
	BBoxdMetadata::Ptr meta = instance_meta->getMetadata<BBoxdMetadata>(regionID);
	if (meta)
	{
		regionBBoxd = meta->value();
	}
	return regionBBoxd;
}

int Ovdb::MeshRegion(const char * const regionID, float surfaceValue)
{
	//Be sure to make calls in proper order InitializeGrid->DefineRegion->LoadRegion->(fill with values e.g. PopulateRegionDensityPerlin)->MeshRegion
	//Extract a surface mesh from the density grid among voxels corresponding to on+true values from the mask grid
	auto regionIter = regionPtrMap.find(regionID);
	if (regionIter != regionPtrMap.end())
	{
		regionIter->second->doActivateValuesOp(surfaceValue);
		regionIter->second->doMeshOp();
	}
	return 0;
}

size_t Ovdb::VertexCount(const char * const regionID)
{
	size_t count = 0;
	static auto regionIter = regionPtrMap.end();
	if (regionIter == regionPtrMap.end() || regionIter->first != regionID)
	{
		regionIter = regionPtrMap.find(regionID);
	}
	if (regionIter != regionPtrMap.end())
	{
		count = regionIter->second->meshOp->vertexCount();
	}
	return count;
}

size_t Ovdb::PolygonCount(const char * const regionID)
{
	size_t count = 0;
	static auto regionIter = regionPtrMap.end();
	if (regionIter == regionPtrMap.end() || regionIter->first != regionID)
	{
		regionIter = regionPtrMap.find(regionID);
	}
	if (regionIter != regionPtrMap.end())
	{
		count = regionIter->second->meshOp->polygonCount();
	}
	return count;
}

size_t Ovdb::NormalCount(const char * const regionID)
{
	size_t count = 0;
	static auto regionIter = regionPtrMap.end();
	if (regionIter == regionPtrMap.end() || regionIter->first != regionID)
	{
		regionIter = regionPtrMap.find(regionID);
	}
	if (regionIter != regionPtrMap.end())
	{
		count = regionIter->second->meshOp->normalCount();
	}
	return count;
}

bool Ovdb::YieldVertex(const char * const regionID, double &vx, double &vy, double &vz)
{
	static auto regionIter = regionPtrMap.end();
	if (regionIter == regionPtrMap.end() || regionIter->first != regionID)
	{
		regionIter = regionPtrMap.find(regionID);
	}
	bool hasVertices = false;
	if (regionIter != regionPtrMap.end())
	{
		hasVertices = regionIter->second->meshOp->nextVertex(vx, vy, vz);
	}
	return hasVertices;
}

bool Ovdb::YieldPolygon(const char * const regionID, uint32_t &i1, uint32_t &i2, uint32_t &i3)
{
	static auto regionIter = regionPtrMap.end();
	if (regionIter == regionPtrMap.end() || regionIter->first != regionID)
	{
		regionIter = regionPtrMap.find(regionID);
	}
	bool hasPolygons = false;
	if (regionIter != regionPtrMap.end())
	{
		hasPolygons = regionIter->second->meshOp->nextPolygon(i1, i2, i3);
	}
	return hasPolygons;
}

bool Ovdb::YieldNormal(const char * const regionID, double &nx, double &ny, double &nz)
{
	static auto regionIter = regionPtrMap.end();
	if (regionIter == regionPtrMap.end() || regionIter->first != regionID)
	{
		regionIter = regionPtrMap.find(regionID);
	}
	bool hasNormals = false;
	if (regionIter != regionPtrMap.end())
	{
		hasNormals = regionIter->second->meshOp->nextNormal(nx, ny, nz);
	}
	return hasNormals;
}

int Ovdb::PopulateRegionDensityPerlin(const char * const regionID, double frequency, double lacunarity, double persistence, int octaveCount)
{
	auto regionIter = regionPtrMap.find(regionID);
	assert(regionIter != regionPtrMap.end());

	ValueGrid &grid = *regionIter->second->gridPtr;
	openvdb::DoubleMetadata::Ptr frequencyMeta = instance_meta->getMetadata<openvdb::DoubleMetadata>("frequency");
	openvdb::DoubleMetadata::Ptr lacunarityMeta = instance_meta->getMetadata<openvdb::DoubleMetadata>("lacunarity");
	openvdb::DoubleMetadata::Ptr persistenceMeta = instance_meta->getMetadata<openvdb::DoubleMetadata>("persistence");
	openvdb::Int32Metadata::Ptr octaveCountMeta = instance_meta->getMetadata<openvdb::Int32Metadata>("octaveCount");
	if (grid.empty() ||
		frequencyMeta == nullptr || !openvdb::math::isApproxEqual(frequency, frequencyMeta->value()) ||
		lacunarityMeta == nullptr || !openvdb::math::isApproxEqual(lacunarity, lacunarityMeta->value()) ||
		persistenceMeta == nullptr || !openvdb::math::isApproxEqual(persistence, persistenceMeta->value()) ||
		octaveCountMeta == nullptr || !openvdb::math::isExactlyEqual(octaveCount, octaveCountMeta->value()))
	{
		//Update the Perlin noise values
		instance_meta->insertMeta("frequency", openvdb::DoubleMetadata(frequency));
		instance_meta->insertMeta("lacunarity", openvdb::DoubleMetadata(lacunarity));
		instance_meta->insertMeta("persistence", openvdb::DoubleMetadata(persistence));
		instance_meta->insertMeta("octaveCount", openvdb::Int32Metadata(octaveCount));

		//Activate mask values such that there is a single padded region along the outer edge with
		//values on and false and all other values within the padded region have values on and true.
		BBoxdMetadata::Ptr gridBBoxdMeta = instance_meta->getMetadata<BBoxdMetadata>(ConstructRegionID(grid.getName(), ""));
		assert(gridBBoxdMeta);
		const openvdb::BBoxd &gridBBoxd = gridBBoxdMeta->value();
		openvdb::CoordBBox bbox = grid.transform().worldToIndexCellCentered(gridBBoxd);
		assert(!bbox.empty());
		openvdb::CoordBBox bboxPadded = bbox;
		bboxPadded.expand(1);

		//Create a mask enclosing the region such that the outer edge voxels have are on but false
		openvdb::BoolGrid::Ptr mask = openvdb::BoolGrid::create(false);
		mask->fill(bboxPadded, /*value*/false, /*state*/true);
		mask->fill(bbox, /*value*/true, /*state*/true);
		openvdb::tools::transformValues(mask->cbeginValueOn(), *regionIter->second->gridPtr, PerlinNoiseFillOp<openvdb::FloatTree>(grid.transform(), frequency, lacunarity, persistence, octaveCount));
	}
	return 0;
}