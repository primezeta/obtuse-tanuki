#include "OvdbTypes.h"
#include "OvdbVolume.h"
#include "OvdbNoise.h"
#include <unordered_map>
#include <boost/filesystem.hpp>

using namespace ovdb; //TODO: Remove 'using' to match UE4 coding standards
using namespace ovdb::meshing; //TODO: Remove 'using' to match UE4 coding standards
using namespace ovdb::tools;

typedef openvdb::TypedMetadata<openvdb::BBoxd> BBoxdMetadata;
boost::shared_ptr<IOvdb> shared_instance = nullptr;
boost::shared_ptr<openvdb::io::File> instance_file = nullptr;
std::string grid_name;
std::unordered_map<std::string, openvdb::FloatGrid::Ptr> regionPtrMap;
std::unordered_map<std::string, openvdb::BoolGrid::Ptr> regionMaskPtrMap;
std::unordered_map<std::string, OvdbVoxelVolume<openvdb::FloatTree>> regionMeshMap;

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

IOvdb * GetIOvdbInstance(const char * vdbFilename)
{
	if (shared_instance == nullptr)
	{
		shared_instance = boost::shared_ptr<IOvdb>(new IOvdb(vdbFilename));
	}
	return shared_instance.get();
}

IOvdb::IOvdb(const char * vdbFilename)
{
	openvdb::initialize();
	BBoxdMetadata::registerType();
	//openvdb::Metadata::registerType("BBoxd", BBoxdMetadata::createMetadata);
	instance_file = boost::shared_ptr<openvdb::io::File>(new openvdb::io::File(vdbFilename));	
}

IOvdb::~IOvdb()
{
	openvdb::uninitialize();
	if (instance_file->isOpen())
	{
		instance_file->close();
	}
	shared_instance.reset();
	instance_file.reset();
}

int IOvdb::InitializeGrid(const char * const gridName)
{
	if (!boost::filesystem::exists(instance_file->filename()))
	{
		openvdb::GridPtrVec grids;
		//openvdb::FloatGrid::Ptr initialGrid = openvdb::FloatGrid::create();
		//initialGrid->setName(gridName);
		//grids.push_back(initialGrid);
		instance_file->write(grids);
		instance_file->setGridStatsMetadataEnabled(true);
	}

	//Open the grid for delayed loading
	instance_file->open();
	if (instance_file->isOpen())
	{
		if (!instance_file->hasGrid(gridName))
		{
			openvdb::GridPtrVecPtr grids = instance_file->getGrids();
			openvdb::FloatGrid::Ptr additionalGrid = openvdb::FloatGrid::create();
			grids->push_back(additionalGrid);
			additionalGrid->setName(gridName);
			instance_file->close();
			instance_file->write(*grids);
			instance_file->open();
		}
		grid_name = gridName;
	}
	return 0;
}

int IOvdb::DefineRegion(int x0, int y0, int z0, int x1, int y1, int z1, char * regionStr, size_t regionStrSize)
{
	if (instance_file->isOpen())
	{
		char regionMetaName[256];
		sprintf(regionMetaName, "%s.%d.%d.%d.%d.%d.%d", grid_name.c_str(), x0, y0, z0, x1, y1, z1);
		std::strcpy(regionStr, regionMetaName);
		openvdb::MetaMap::Ptr meta = instance_file->getMetadata();
		BBoxdMetadata::Ptr regionMeta = meta->getMetadata<BBoxdMetadata>(regionStr);
		//Insert the region meta data associated with the region name, potentially overwriting an existing entry of the same name
		openvdb::BBoxd regionBBoxd(openvdb::Vec3d(x0, y0, z0), openvdb::Vec3d(x1, y1, z1));
		meta->insertMeta(regionStr, BBoxdMetadata(regionBBoxd));
		openvdb::GridPtrVecPtr grids = instance_file->getGrids();
		instance_file->close();
		instance_file->write(*grids, *meta);
		instance_file->open();
	}
	return 0;
}

int IOvdb::LoadRegion(const char * const regionName)
{
	if (instance_file->isOpen())
	{
		openvdb::MetaMap::Ptr meta = instance_file->getMetadata();
		BBoxdMetadata::Ptr regionMeta = meta->getMetadata<BBoxdMetadata>(regionName);
		if (regionMeta)
		{
			const openvdb::BBoxd &regionBBoxd = regionMeta->value();
			regionPtrMap[regionName] = openvdb::gridPtrCast<openvdb::FloatGrid>(instance_file->readGrid(grid_name, regionBBoxd));
			regionMeshMap[regionName] = OvdbVoxelVolume<openvdb::FloatTree>(regionPtrMap[regionName]);
		}
	}
	return 0;
}

int IOvdb::MeshRegion(const char * const regionName, float surfaceValue)
{
	//Note: No check for prior existence of map items!
	//Be sure to make calls in proper order InitializeGrid->DefineRegion->LoadRegion->(fill with values e.g. PopulateRegionDensityPerlin)->MeshRegion
	//Extract a surface mesh from the density grid among voxels corresponding to on+true values from the mask grid
	ExtractSurfaceOp extractSurfaceOp(regionPtrMap[regionName], surfaceValue);
	openvdb::tools::foreach(regionMaskPtrMap[regionName]->cbeginValueOn(), extractSurfaceOp);
	regionMeshMap[regionName].initializeRegion();
	regionMeshMap[regionName].doPrimitiveCubesMesh();
	return 0;
}

bool IOvdb::YieldVertex(const char * const regionName, double &vx, double &vy, double &vz)
{
	auto i = regionMeshMap.find(regionName);
	bool hasVertices = false;
	if (i != regionMeshMap.end())
	{
		hasVertices = i->second.nextVertex(vx, vy, vz);
	}
	return hasVertices;
}

bool IOvdb::YieldPolygon(const char * const regionName, uint32_t &i1, uint32_t &i2, uint32_t &i3)
{
	auto i = regionMeshMap.find(regionName);
	bool hasPolygons = false;
	if (i != regionMeshMap.end())
	{
		hasPolygons = i->second.nextPolygon(i1, i2, i3);
	}
	return hasPolygons;
}

bool IOvdb::YieldNormal(const char * const regionName, double &nx, double &ny, double &nz)
{
	auto i = regionMeshMap.find(regionName);
	bool hasNormals = false;
	if (i != regionMeshMap.end())
	{
		hasNormals = i->second.nextNormal(nx, ny, nz);
	}
	return hasNormals;
}

int IOvdb::PopulateRegionDensityPerlin(const char * const regionName, double scaleXYZ, double frequency, double lacunarity, double persistence, int octaveCount)
{
	openvdb::MetaMap::Ptr meta = instance_file->getMetadata();
	BBoxdMetadata::Ptr regionMeta = meta->getMetadata<BBoxdMetadata>(regionName);
	openvdb::BBoxd regionBBoxd;
	if (regionMeta)
	{
		regionBBoxd = regionMeta->value();
	}

	openvdb::FloatGrid::Ptr densityGrid = regionPtrMap[regionName];
	densityGrid->setTransform(openvdb::math::Transform::Ptr(new openvdb::math::Transform(openvdb::math::MapBase::Ptr(new openvdb::math::ScaleMap(openvdb::Vec3d(scaleXYZ))))));
	openvdb::BoolGrid::Ptr maskGrid = openvdb::tools::clip_internal::convertToBoolMaskGrid(*densityGrid);
	regionMaskPtrMap[regionName] = maskGrid;

	//Initialize all mask voxels to on that are within the region
	const openvdb::Vec3d &bboxMin = regionBBoxd.min();
	const openvdb::Vec3d &bboxMax = regionBBoxd.max();
	const int maxXWithPad = 1 + (int)bboxMax.x();
	const int maxYWithPad = 1 + (int)bboxMax.y();
	const int maxZWithPad = 1 + (int)bboxMax.z();
	auto &maskGridTree = maskGrid->tree();
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
					maskGridTree.modifyValue(openvdb::Coord(x, y, z), SetValueOp<bool>(false));
				}
				else
				{
					//Set this voxel to on for purposes of iterating and set value to true to denote that it is meshable
					maskGridTree.modifyValue(openvdb::Coord(x, y, z), SetValueOp<bool>(true));
				}
			}
		}
	}

	//Among all active mask voxels set the corresponding density grid value to the Perlin noise value (voxel states will be inactive)
	PerlinNoiseFillOp perlinNoiseFillOp(densityGrid, frequency, lacunarity, persistence, octaveCount);
	openvdb::tools::foreach(maskGridTree.cbeginValueOn(), perlinNoiseFillOp);
	return 0;
}