#include "OvdbTypes.h"
#include "OvdbVolume.h"
#include "OvdbNoise.h"
#include <unordered_map>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string.hpp>

using namespace ovdb; //TODO: Remove 'using' to match UE4 coding standards
using namespace ovdb::meshing; //TODO: Remove 'using' to match UE4 coding standards
using namespace ovdb::tools;

//Helper struct to hold the associated grid meshing info
typedef struct _RegionMesh
{
	_RegionMesh() {}
	_RegionMesh(openvdb::FloatGrid::Ptr grid)
		: gridPtr(grid), maskPtr(openvdb::BoolGrid::create()), mesher(grid) {}
	openvdb::FloatGrid::Ptr gridPtr;
	openvdb::BoolGrid::Ptr maskPtr;
	OvdbVoxelVolume<openvdb::FloatTree> mesher;
} RegionMesh;

typedef openvdb::TypedMetadata<openvdb::BBoxd> BBoxdMetadata;
typedef openvdb::TypedMetadata<std::string> RegionNamesMetadata;
boost::shared_ptr<Ovdb> shared_instance;
boost::shared_ptr<openvdb::io::File> instance_file = nullptr;
std::unordered_map<std::string, RegionMesh> regionPtrMap;
openvdb::GridPtrVecPtr gridPtrVecPtr;
void GridInitializer(const char * const gridName);
void GridOutputInitializer();
void GridInputInitializer();
void WriteVDB(openvdb::GridPtrVecPtr grids, openvdb::MetaMap::Ptr meta);
std::string ConstructMetaNameRegionID(const std::string &gridName, const std::string &regionName);
std::string ParseRegionGridName(const std::string &regionID);
std::vector<std::string> ParseRegionIDs(const std::string &regionNameMeta);
std::string ConstructMetaRegionIDStr(const std::string &regionIDsStr, const std::string &additionalID);
openvdb::BBoxd GetRegionBBoxd(const std::string &regionID);
std::unordered_map<std::string, RegionMesh>::iterator GetRegionIter(const std::string &regionMetaName);
openvdb::BBoxd ReadGridRegion(const std::string &regionID);

Ovdb * IOvdb::GetIOvdbInstance(const char * vdbFilename, const char * const gridName)
{
	if (shared_instance != nullptr && std::string(vdbFilename) != instance_file->filename())
	{
		//Filename changed - start a new instance
		shared_instance.reset();
	}
	if (shared_instance == nullptr)
	{
		//Create the instance 
		shared_instance = boost::shared_ptr<Ovdb>(new Ovdb(vdbFilename, gridName));
	}
	return shared_instance.get();
}

template<typename ValueType>
struct SetValueOp
{
	const ValueType val;
	SetValueOp(const ValueType& v) : val(v) {}
	inline void operator()(ValueType& v) const { v = val; }
};

//Operator to fill a grid with noise from a perlin noise source
typedef struct _PerlinNoiseFillOp
{
	openvdb::FloatGrid::Ptr gridPtr;
	openvdb::FloatGrid::Accessor densityAcc;
	noise::module::Perlin perlin;

	_PerlinNoiseFillOp(openvdb::FloatGrid::Ptr grid, double frequency, double lacunarity, double persistence, int octaveCount)
		: gridPtr(grid), densityAcc(grid->getAccessor())
	{
		perlin.SetFrequency(frequency);
		perlin.SetLacunarity(lacunarity);
		perlin.SetPersistence(persistence);
		perlin.SetOctaveCount(octaveCount);
	}

    void operator()(const openvdb::BoolGrid::ValueOnCIter& iter)
    {
		_PerlinNoiseFillOp::doOp(iter, *gridPtr, densityAcc, perlin);
    }

	void operator()(const openvdb::BoolGrid::ValueOnCIter& iter) const
	{
		_PerlinNoiseFillOp::doOp(iter, *gridPtr, gridPtr->getAccessor(), perlin);
	}

	static void doOp(const openvdb::BoolGrid::ValueOnCIter& iter, openvdb::FloatGrid &grid, openvdb::FloatGrid::Accessor &acc, const noise::module::Perlin &perlin)
	{
		const openvdb::Coord coord = iter.getCoord();
		const openvdb::Vec3d vec = grid.indexToWorld(coord);
		//Subtract the z component in order to define a central flat plane from which density values extend
		double density = -vec.z();
		density += perlin.GetValue(vec.x(), vec.y(), vec.z());
		acc.setValueOnly(coord, (float)density);
	}
} PerlinNoiseFillOp;

//Operator to fill a grid with noise from a perlin noise source
typedef struct _ExtractSurfaceOp
{
	openvdb::FloatGrid::Ptr gridPtr;
	openvdb::FloatGrid::Accessor densityAcc;
	const float &surfaceValue;

	_ExtractSurfaceOp(openvdb::FloatGrid::Ptr grid, const float &isovalue)
		: gridPtr(grid), densityAcc(grid->getAccessor()), surfaceValue(isovalue)
	{
	}

	void operator()(const openvdb::BoolGrid::ValueOnCIter& iter)
	{
		_ExtractSurfaceOp::doOp(iter, densityAcc, surfaceValue);
	}

	void operator()(const openvdb::BoolGrid::ValueOnCIter& iter) const
	{
		_ExtractSurfaceOp::doOp(iter, gridPtr->getAccessor(), surfaceValue);
	}

	static void doOp(const openvdb::BoolGrid::ValueOnCIter& iter, openvdb::FloatGrid::Accessor &acc, const float &surfaceValue)
	{
		const openvdb::Coord coord = iter.getCoord();
		uint8_t insideBits = 0;
		//For each neighboring value set a bit if it is inside the surface (inside = positive value)
		if (acc.getValue(coord) > surfaceValue) { insideBits |= 1; }
		if (acc.getValue(coord.offsetBy(1, 0, 0)) > surfaceValue) { insideBits |= 2; }
		if (acc.getValue(coord.offsetBy(0, 1, 0)) > surfaceValue) { insideBits |= 4; }
		if (acc.getValue(coord.offsetBy(0, 0, 1)) > surfaceValue) { insideBits |= 8; }
		if (acc.getValue(coord.offsetBy(1, 1, 0)) > surfaceValue) { insideBits |= 16; }
		if (acc.getValue(coord.offsetBy(1, 0, 1)) > surfaceValue) { insideBits |= 32; }
		if (acc.getValue(coord.offsetBy(0, 1, 1)) > surfaceValue) { insideBits |= 64; }
		if (acc.getValue(coord.offsetBy(1, 1, 1)) > surfaceValue) { insideBits |= 128; }
		if (insideBits > 0 && insideBits < 255)
		{
			//At least one vertex, but not all, is/are on the other side of the surface from the others so activate this voxel for meshing
			acc.setActiveState(coord, iter.getValue());
		}
		else
		{
			acc.setActiveState(coord, false);
		}
	}
} ExtractSurfaceOp;

void GridInitializer(const char * const gridName)
{
	//If necessary create the file then create the grid if it does not exist
	//TODO: Handle openvdb exceptions (especially IO exceptions)
	if (!boost::filesystem::exists(instance_file->filename()))
	{
		//Initialize an empty vdb file
		if (gridPtrVecPtr != nullptr)
		{
			gridPtrVecPtr->clear();
			gridPtrVecPtr.reset();
		}
		instance_file->setGridStatsMetadataEnabled(true);
		instance_file->write(openvdb::GridPtrVec());
	}
	//TODO: Error handling when unable to create file. For now assume the file exists
	assert(boost::filesystem::exists(instance_file->filename()));
	
	GridInputInitializer();
	gridPtrVecPtr = instance_file->getGrids();
	bool isChanged = false;
	if (!instance_file->hasGrid(gridName))
	{
		openvdb::FloatGrid::Ptr grid = openvdb::FloatGrid::create();
		gridPtrVecPtr->push_back(grid);
		grid->setName(gridName);
		isChanged = true;
	}
	openvdb::MetaMap::Ptr meta = instance_file->getMetadata();
	assert(meta != nullptr);
	RegionNamesMetadata::Ptr regionNamesMeta = meta->getMetadata<RegionNamesMetadata>("RegionIDs");
	if (regionNamesMeta == nullptr)
	{
		meta->insertMeta("RegionIDs", RegionNamesMetadata(""));
		isChanged = true;
	}
	if (isChanged)
	{
		//TODO: Handle openvdb IO exceptions
		WriteVDB(gridPtrVecPtr, meta);
	}
}

void GridOutputInitializer()
{
	//Close the file to allow writing
	if (instance_file->isOpen())
	{
		instance_file->close();
	}
	assert(!instance_file->isOpen()); //TODO: Handle openvdb IO exceptions
}

void GridInputInitializer()
{
	//Open the file to allow reading
	if (!instance_file->isOpen())
	{
		instance_file->open();
	}
	assert(instance_file->isOpen()); //TODO: Handle openvdb IO exceptions
}

void WriteVDB(openvdb::GridPtrVecPtr grids, openvdb::MetaMap::Ptr meta)
{
	GridOutputInitializer();
	//Write the grids and metadata
	instance_file->write(*grids, *meta); //TODO: Handle openvdb IO exceptions
}

std::string ConstructMetaNameRegionID(const std::string &gridName, const std::string &regionName)
{
	//TODO: Somewhere enforce grid name and region names to be disallowed '/'
	return std::string(gridName) + "/" + std::string(regionName);
}

std::string ParseRegionGridName(const std::string &regionID)
{
	//TODO: Somewhere enforce grid name and region names to be disallowed '/'
	std::vector<std::string> splitStrs;
	boost::split(splitStrs, regionID, boost::is_any_of("/")); //First item before the forward slash is the grid name'
	assert(splitStrs.size() > 0);
	return splitStrs.front();
}

std::string ConstructMetaRegionIDStr(const std::string &regionIDsStr, const std::string &additionalID)
{
	if (regionIDsStr.empty())
	{
		return additionalID;
	}
	return regionIDsStr + "][" + additionalID;
}

std::vector<std::string> ParseRegionIDs(const std::string &regionNamesMeta)
{
	std::vector<std::string> splitStrs;
	if (!regionNamesMeta.empty())
	{
		boost::split(splitStrs, regionNamesMeta, boost::is_any_of("][")); //First item before the forward slash is the grid name'
		assert(splitStrs.size() > 0);
	}
	return splitStrs;
}

Ovdb::Ovdb(const char * vdbFilename, const char * gridName)
{
	assert(shared_instance == nullptr);
	assert(instance_file == nullptr);
	assert(gridPtrVecPtr == nullptr);
	openvdb::initialize();
	instance_file = boost::shared_ptr<openvdb::io::File>(new openvdb::io::File(vdbFilename));
	if (!BBoxdMetadata::isRegisteredType())
	{
		BBoxdMetadata::registerType();
	}
	if (!RegionNamesMetadata::isRegisteredType())
	{
		RegionNamesMetadata::registerType();
	}
	GridInitializer(gridName);
}

Ovdb::~Ovdb()
{
	openvdb::uninitialize();
	if (instance_file != nullptr && instance_file->isOpen())
	{
		instance_file->close();
	}
	instance_file.reset();
	shared_instance.reset();
	if (gridPtrVecPtr != nullptr)
	{
		gridPtrVecPtr->clear();
	}
	gridPtrVecPtr.reset();
}

int Ovdb::DefineRegion(const char * const gridName, const char * const regionName, int x0, int y0, int z0, int x1, int y1, int z1, bool commitChanges)
{
	//Insert the new meta / replace an existing meta value of the same name
	GridInputInitializer();
	const std::string regionMetaID = ConstructMetaNameRegionID(gridName, regionName);
	openvdb::MetaMap::Ptr meta = instance_file->getMetadata();
	assert(meta);
	RegionNamesMetadata::Ptr regionNamesMeta = meta->getMetadata<RegionNamesMetadata>("RegionIDs");
	assert(regionNamesMeta);

	//Add the ID to the region ID list if it doesn't yet exist
	std::string &regionIDsStr = regionNamesMeta->value();
	std::vector<std::string> regionIDs = ParseRegionIDs(regionIDsStr);
	int regionCount = regionIDs.size();
	if (std::find(regionIDs.begin(), regionIDs.end(), regionMetaID) == regionIDs.end())
	{
		//Add the new region ID and pack the region IDs back into a single string for saving to meta
		regionNamesMeta->setValue(ConstructMetaRegionIDStr(regionIDsStr, regionMetaID));
		regionCount++;
	}

	//Insert the region bbox (note that insert meta will replace an existing meta item of the same name)
	meta->insertMeta(regionMetaID, BBoxdMetadata(openvdb::BBoxd(openvdb::Vec3d(x0, y0, z0), openvdb::Vec3d(x1, y1, z1))));
	if (commitChanges)
	{
		WriteVDB(gridPtrVecPtr, meta);
	}
	return regionCount;
}

size_t Ovdb::ReadMetaGridRegionCount(const char * const gridName)
{
	GridInputInitializer();
	openvdb::MetaMap::Ptr meta = instance_file->getMetadata();
	assert(meta);
	RegionNamesMetadata::Ptr regionNamesMeta = meta->getMetadata<RegionNamesMetadata>("RegionIDs");
	assert(regionNamesMeta);

	//Return the count of all defined regions if no grid name supplied, or count of regions under the specified grid
	size_t count = 0;
	std::vector<std::string> regionIDs = ParseRegionIDs(regionNamesMeta->value());
	if (gridName == nullptr || std::string(gridName).empty())
	{
		count = regionIDs.size();
	}
	else
	{
		//Only count the region if it belongs to the specified grid
		for (auto i = regionIDs.begin(); i != regionIDs.end(); ++i)
		{
			if (gridName == ParseRegionGridName(*i))
			{
				count++;
			}
		}
	}
	return count;
}

int Ovdb::ReadMetaRegionIDs(char ** regionIDList, size_t regionCount, size_t strMaxLen)
{
	//Return a c-style array of c-style ID strings
	GridInputInitializer();
	openvdb::MetaMap::Ptr meta = instance_file->getMetadata();
	assert(meta);
	RegionNamesMetadata::Ptr regionNamesMeta = meta->getMetadata<RegionNamesMetadata>("RegionIDs");
	assert(regionNamesMeta);
	const std::vector<std::string> regionIDs = ParseRegionIDs(regionNamesMeta->value());
	size_t numIDs = 0;
	for (auto i = regionIDs.begin(); i != regionIDs.end() && numIDs < regionCount; ++i)
	{
		char * strStart = regionIDList[sizeof(char*)*(numIDs++)];
		size_t size = regionIDs.size() > strMaxLen ? strMaxLen : regionIDs.size();
		strcpy_s(strStart, size, i->c_str());
	}
	return numIDs;
}

openvdb::BBoxd GetMetaRegionBBoxd(const std::string &regionID)
{
	openvdb::MetaMap::Ptr meta = instance_file->getMetadata();
	BBoxdMetadata::Ptr regionMeta = meta->getMetadata<BBoxdMetadata>(regionID);
	openvdb::BBoxd regionBBox;
	if (regionMeta)
	{
		regionBBox = regionMeta->value();
	}
	return regionBBox;
}

int Ovdb::ReadRegion(const char * const regionID, int &x0, int &y0, int &z0, int &x1, int &y1, int &z1)
{
	GridInputInitializer();
	const openvdb::BBoxd regionBBoxd = GetMetaRegionBBoxd(regionID);
	x0 = (int)regionBBoxd.min().x();
	y0 = (int)regionBBoxd.min().y();
	z0 = (int)regionBBoxd.min().z();
	x1 = (int)regionBBoxd.max().x();
	y1 = (int)regionBBoxd.max().y();
	z1 = (int)regionBBoxd.max().z();
	return regionBBoxd.empty() ? 1 : 0;
}

openvdb::BBoxd ReadGridRegion(const std::string &regionID)
{
	const openvdb::BBoxd regionBBoxd = GetMetaRegionBBoxd(regionID);
	auto regionIter = regionPtrMap.find(regionID);
	if (!regionBBoxd.empty())
	{
		if (regionIter == regionPtrMap.end())
		{
			openvdb::FloatGrid::Ptr grid = openvdb::gridPtrCast<openvdb::FloatGrid>(instance_file->readGrid(ParseRegionGridName(regionID), regionBBoxd));
			grid->setName(regionID);
			gridPtrVecPtr->push_back(grid);
			regionPtrMap[regionID] = RegionMesh(grid);
		}
	}
	return regionBBoxd;
}

int Ovdb::LoadRegion(const char * const regionID)
{
	GridInputInitializer();
	ReadGridRegion(regionID);
	return 0;
}

int Ovdb::WriteChanges()
{
	//TODO: Handle openvdb IO exceptions
	GridInputInitializer();
	openvdb::MetaMap::Ptr meta = instance_file->getMetadata();
	assert(meta);
	assert(gridPtrVecPtr);
	WriteVDB(gridPtrVecPtr, meta);
	return 0;
}

int Ovdb::MeshRegion(const char * const regionID, float surfaceValue)
{
	//Be sure to make calls in proper order InitializeGrid->DefineRegion->LoadRegion->(fill with values e.g. PopulateRegionDensityPerlin)->MeshRegion
	//Extract a surface mesh from the density grid among voxels corresponding to on+true values from the mask grid
	auto regionIter = regionPtrMap.find(regionID);
	if (regionIter != regionPtrMap.end())
	{
		ExtractSurfaceOp extractSurfaceOp(regionIter->second.gridPtr, surfaceValue);
		openvdb::tools::foreach(regionIter->second.maskPtr->cbeginValueOn(), extractSurfaceOp);
		regionPtrMap[regionID].mesher.initializeRegion();
		regionIter->second.mesher.doPrimitiveCubesMesh();
	}
	return 0;
}

bool Ovdb::YieldVertex(const char * const regionID, double &vx, double &vy, double &vz)
{
	static std::unordered_map<std::string, RegionMesh>::iterator regionIter = regionPtrMap.end();
	regionIter = regionPtrMap.find(regionID);
	bool hasVertices = false;
	if (regionIter != regionPtrMap.end())
	{
		hasVertices = regionIter->second.mesher.nextVertex(vx, vy, vz);
	}
	return hasVertices;
}

bool Ovdb::YieldPolygon(const char * const regionID, uint32_t &i1, uint32_t &i2, uint32_t &i3)
{
	static std::unordered_map<std::string, RegionMesh>::iterator regionIter = regionPtrMap.end();
	regionIter = regionPtrMap.find(regionID);
	bool hasPolygons = false;
	if (regionIter != regionPtrMap.end())
	{
		hasPolygons = regionIter->second.mesher.nextPolygon(i1, i2, i3);
	}
	return hasPolygons;
}

bool Ovdb::YieldNormal(const char * const regionID, double &nx, double &ny, double &nz)
{
	static std::unordered_map<std::string, RegionMesh>::iterator regionIter = regionPtrMap.end();
	regionIter = regionPtrMap.find(regionID);
	bool hasNormals = false;
	if (regionIter != regionPtrMap.end())
	{
		hasNormals = regionIter->second.mesher.nextNormal(nx, ny, nz);
	}
	return hasNormals;
}

int Ovdb::PopulateRegionDensityPerlin(const char * const regionID, double scaleXYZ, double frequency, double lacunarity, double persistence, int octaveCount)
{
	openvdb::MetaMap::Ptr meta = instance_file->getMetadata();
	assert(meta);
	BBoxdMetadata::Ptr regionMeta = meta->getMetadata<BBoxdMetadata>(regionID);
	assert(regionMeta);
	const openvdb::BBoxd &regionBBoxd = regionMeta->value();

	openvdb::DoubleMetadata::Ptr scaleXYZMeta = meta->getMetadata<openvdb::DoubleMetadata>("scaleXYZ");
	openvdb::DoubleMetadata::Ptr frequencyMeta = meta->getMetadata<openvdb::DoubleMetadata>("frequency");
	openvdb::DoubleMetadata::Ptr lacunarityMeta = meta->getMetadata<openvdb::DoubleMetadata>("lacunarity");
	openvdb::DoubleMetadata::Ptr persistenceMeta = meta->getMetadata<openvdb::DoubleMetadata>("persistence");
	openvdb::Int32Metadata::Ptr octaveCountMeta = meta->getMetadata<openvdb::Int32Metadata>("octaves");
	
	//If no Perlin noise parameters changed then we're done! TODO: Provide parameter to set perlin noise seed
	if (scaleXYZMeta == nullptr || !openvdb::math::isApproxEqual(scaleXYZ, scaleXYZMeta->value()) ||
		frequencyMeta == nullptr || !openvdb::math::isApproxEqual(frequency, frequencyMeta->value()) ||
		lacunarityMeta == nullptr || !openvdb::math::isApproxEqual(lacunarity, lacunarityMeta->value()) ||
		persistenceMeta == nullptr || !openvdb::math::isApproxEqual(persistence, persistenceMeta->value()) ||
		octaveCountMeta == nullptr || octaveCount != octaveCountMeta->value())
	{
		regionPtrMap[regionID].gridPtr->setTransform(openvdb::math::Transform::Ptr(new openvdb::math::Transform(openvdb::math::MapBase::Ptr(new openvdb::math::ScaleMap(openvdb::Vec3d(scaleXYZ))))));
		regionPtrMap[regionID].maskPtr = openvdb::tools::clip_internal::convertToBoolMaskGrid(*regionPtrMap[regionID].gridPtr);

		//Initialize all mask voxels to on that are within the region
		const openvdb::Vec3d &bboxMin = regionBBoxd.min();
		const openvdb::Vec3d &bboxMax = regionBBoxd.max();
		const int maxXWithPad = 1 + (int)bboxMax.x();
		const int maxYWithPad = 1 + (int)bboxMax.y();
		const int maxZWithPad = 1 + (int)bboxMax.z();
		for (int x = (int)bboxMin.x(); x <= maxXWithPad; ++x)
		{
			for (int y = (int)bboxMin.y(); y <= maxYWithPad; ++y)
			{
				for (int z = (int)bboxMin.z(); z <= maxZWithPad; ++z)
				{
					//Setting active this way because according to comments in ValueTransformer.h, modifyValue functions are "typically significantly faster than calling getValue() followed by setValue()"
					if (x == maxXWithPad || y == maxYWithPad || z == maxZWithPad)
					{
						//The padding prevents the neighbor-value lookup from fetching a background value 0 and inadvertently meshing all the voxels along the far-boundary.
						//i.e. this voxel will be on for purposes of iterating, but will have a false value such that it is always skipped for meshing.
						regionPtrMap[regionID].maskPtr->tree().modifyValue(openvdb::Coord(x, y, z), SetValueOp<bool>(false));
					}
					else
					{
						//Set this voxel to on for purposes of iterating and set value to true to denote that it is meshable
						regionPtrMap[regionID].maskPtr->tree().modifyValue(openvdb::Coord(x, y, z), SetValueOp<bool>(true));
					}
				}
			}
		}

		//Among all active mask voxels set the corresponding density grid value to the Perlin noise value (voxel states will be inactive)
		PerlinNoiseFillOp perlinNoiseFillOp(regionPtrMap[regionID].gridPtr, frequency, lacunarity, persistence, octaveCount);
		openvdb::tools::foreach(regionPtrMap[regionID].maskPtr->tree().cbeginValueOn(), perlinNoiseFillOp);
	}
	return 0;
}