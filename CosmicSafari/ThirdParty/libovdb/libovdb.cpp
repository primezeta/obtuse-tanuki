#include "OvdbTypes.h"
#include "OvdbVolume.h"
#include "OvdbNoise.h"
#include <fstream>
#include <unordered_map>
//#include "sqlite3.h"

using namespace ovdb; //TODO: Remove 'using' to match UE4 coding standards
using namespace ovdb::meshing; //TODO: Remove 'using' to match UE4 coding standards
using namespace ovdb::tools;

typedef OvdbVoxelVolume<TreeVdbType> VolumeType;
typedef std::unordered_map<std::wstring, GridPtr> GridByIDType;
typedef std::unordered_map<std::wstring, VolumeType> RegionByIDType;
typedef std::unordered_multimap<std::wstring, std::wstring> MeshIDByGridID;

GridByIDType GridByID;
MeshIDByGridID MeshIDs;
RegionByIDType RegionByID;

void addGrid(GridPtr grid, const std::wstring &gridID);
GridPtr getGridByID(const std::wstring &gridID);
GridCPtr getConstGridByID(const std::wstring &gridID);
void checkGrid(const std::wstring &gridID, const GridCPtr constGrid);
void checkGridID(const std::wstring &gridID);
void checkRegionID(const std::wstring &regionID);
std::string gridNamesList(const openvdb::io::File &file);

////Operator to fill a grid with noise from a perlin noise source
//struct PerlinNoiseFill
//{
//    static void op(const openvdb::Vec3fGrid::ValueOnCIter& iter, FloatGrid::ValueAccessor& accessor)
//    {
//        if (iter.isVoxelValue()) { // set a single voxel
//            accessor.setValue(iter.getCoord(), iter->length());
//        } else { // fill an entire tile
//            CoordBBox bbox;
//            iter.getBoundingBox(bbox);
//            accessor.getTree()->fill(bbox, iter->length());
//        }
//    }
//};

boost::shared_ptr<typename IOvdb> shared_instance = nullptr;
IOvdb * GetIOvdbInstance()
{
	if (shared_instance == nullptr)
	{
		shared_instance = boost::shared_ptr<typename IOvdb>(new IOvdb());
	}
	return shared_instance.get();
}

IOvdb::IOvdb()
{
	openvdb::initialize();
}

IOvdb::~IOvdb()
{
	openvdb::uninitialize();
	shared_instance.reset();
}

void IOvdb::InitializeGrid(const wchar_t * const gridName)
{
	openvdb::FloatGrid::Ptr grid = openvdb::createGrid<openvdb::FloatGrid>();
	std::wstring name = gridName;
	std::string nameStr = std::string(name.begin(), name.end());
	grid->setName(nameStr);
	addGrid(grid, name);
}

int IOvdb::MaskRegions(const wchar_t * const gridID, int32_t regionCountX, int32_t regionCountY, int32_t regionCountZ, int32_t &regionSizeX, int32_t &regionSizeY, int32_t &regionSizeZ)
{
	try
	{
		auto grid = getGridByID(gridID);
		const openvdb::Coord gridSize = grid->evalActiveVoxelBoundingBox().dim();
		const openvdb::Coord regionSize(gridSize.x() / regionCountX, gridSize.y() / regionCountY, gridSize.z() / regionCountZ);
		regionSizeX = regionSize.x();
		regionSizeY = regionSize.y();
		regionSizeZ = regionSize.z();
		if (regionSize.x() < 1 || regionSize.y() < 1 || regionSize.z() < 1)
		{
			OPENVDB_THROW(openvdb::ValueError, "Region count must be 1 or greater per dimension");
		}

		std::wostringstream meshIDStr;
		for (int32_t rx = 0; rx < regionCountX; ++rx)
		{
			for (int32_t ry = 0; ry < regionCountY; ++ry)
			{
				for (int32_t rz = 0; rz < regionCountZ; ++rz)
				{
					const openvdb::Coord regionMin(rx*regionSize.x(), ry*regionSize.y(), rz*regionSize.z());
					const openvdb::Coord regionMax((rx+1)*regionSize.x()-1, (ry+1)*regionSize.y()-1, (rz+1)*regionSize.z()-1);
					meshIDStr << rx << "," << ry << "," << rz;
					std::wstring meshID(meshIDStr.str());
					MeshIDs.emplace(gridID, meshID);
					std::wstring regionID = gridID + meshID;
					RegionByID[regionID] = VolumeType(grid);
					RegionByID[regionID].initializeRegion(openvdb::CoordBBox(regionMin, regionMax));
				}
			}
		}
	}
	catch (openvdb::Exception &e)
	{
		std::ofstream logfile;
		logfile.open("C:\\Users\\zach\\Documents\\Unreal Projects\\obtuse-tanuki\\CosmicSafari\\ThirdParty\\exceptions.log");
		logfile << "TileizeRegions: " << e.what() << std::endl;
		logfile.close();
		return 1;
	}
	return 0;
}

int IOvdb::ReadGrid(const wchar_t * const gridID, const wchar_t * const filename)
{
	std::wstring id = gridID;
	std::wstring fname = filename;
	std::string idStr = std::string(id.begin(), id.end());
	std::string filenameStr = std::string(fname.begin(), fname.end());
	try
	{
		//A .vdb might have multiple grids, so we need the grid ID along with just the filename
		openvdb::io::File file(filenameStr);
		file.open();
		if (file.getSize() < 1)
		{
			OPENVDB_THROW(openvdb::IoError, "Could not read " + filenameStr);
		}
		std::string ids = gridNamesList(file);

		auto g = file.readGrid(idStr);
		file.close();
		if (!g)
		{
			OPENVDB_THROW(openvdb::RuntimeError, "Unable to find grid \"" + idStr + "\" in " + filenameStr);
		}
		GridPtr grid = openvdb::gridPtrCast<GridVdbType>(g);
		if (!grid)
		{
			OPENVDB_THROW(openvdb::RuntimeError, "No valid grids in " + filenameStr);
		}
		addGrid(grid, id);
	}
	catch (openvdb::Exception &e)
	{
		std::ofstream logfile;
		logfile.open("C:\\Users\\zach\\Documents\\Unreal Projects\\obtuse-tanuki\\CosmicSafari\\ThirdParty\\exceptions.log");
		logfile << "OvdbReadVdbGrid: " << e.what() << std::endl;
		logfile.close();
		return 1;
	}
	return 0;
}

int IOvdb::WriteGrid(const wchar_t * const gridID, const wchar_t * const filename)
{
	try
	{
		GridPtr grid = getGridByID(gridID);
		openvdb::GridPtrVec grids;
		grids.push_back(grid);
		//TODO: Check if directory exists and whatever else would be Good Stuff to do
		std::wstring fname = filename;
		openvdb::io::File file(std::string(fname.begin(), fname.end()));
		file.write(grids);
		file.close();
	}
	catch (openvdb::Exception &e)
	{
		std::ofstream logfile;
		logfile.open("C:\\Users\\zach\\Documents\\Unreal Projects\\obtuse-tanuki\\CosmicSafari\\ThirdParty\\exceptions.log");
		logfile << "OvdbWriteVdbGrid: " << e.what() << std::endl;
		logfile.close();
		return 1;
	}
	return 0;
}

int IOvdb::GridToMesh(const wchar_t * const gridID, OvdbMeshMethod meshMethod, float surfaceValue)
{
	std::wstring id = gridID;
	try
	{
		GridPtr grid = getGridByID(id);
		auto mesh_range = MeshIDs.equal_range(id);
		if (mesh_range.first == MeshIDs.end()) //First has the start iter and second has the end iter so it's ok to check either for end()
		{
			OPENVDB_THROW(openvdb::RuntimeError, "No region masks defined for " + std::string(id.begin(), id.end()));
		}

		//Mesh the specified regions
		for (auto i = mesh_range.first; i != mesh_range.second; ++i)
		{
			RegionToMesh(gridID, i->first.c_str(), meshMethod, surfaceValue);
		}
		//Note: Don't know of a better way to handle the logic of a multi-map since iterators don't have >= operator
		RegionToMesh(gridID, mesh_range.second->first.c_str(), meshMethod, surfaceValue);
	}
	catch (openvdb::Exception &e)
	{
		std::ofstream logfile;
		logfile.open("C:\\Users\\zach\\Documents\\Unreal Projects\\obtuse-tanuki\\CosmicSafari\\ThirdParty\\exceptions.log");
		logfile << "OvdbVolumeToMesh: " << e.what() << std::endl;
		logfile.close();
		return 1;
	}
	return 0;
}

//Note: Probably will eventually generate meshID internally (e.g. for when using a database or some other scheme to store generated terrain)
int IOvdb::RegionToMesh(const wchar_t * const gridID, const wchar_t * const meshID, OvdbMeshMethod meshMethod, float surfaceValue)
{
	try
	{
		//Note: Don't know of a better way to handle the logic of a multi-map since iterators don't have >= operator
		if (meshMethod == METHOD_PRIMITIVE_CUBES)
		{
			std::wstring regionID = std::wstring(gridID) + std::wstring(meshID);
			RegionByID[regionID].doPrimitiveCubesMesh();
		}
	}
	catch (openvdb::Exception &e)
	{
		std::ofstream logfile;
		logfile.open("C:\\Users\\zach\\Documents\\Unreal Projects\\obtuse-tanuki\\CosmicSafari\\ThirdParty\\exceptions.log");
		logfile << "OvdbVolumeToMesh: " << e.what() << std::endl;
		logfile.close();
		return 1;
	}
	return 0;
}

int IOvdb::YieldVertex(const wchar_t * const gridID, const wchar_t * const meshID, float &vx, float &vy, float &vz)
{
	static std::wstring prevID;
	static VolumeType *meshData = nullptr;
	static VolumeVerticesType::const_iterator iter;
	const std::wstring regionID = std::wstring(gridID) + std::wstring(meshID);
	if (regionID != prevID)
	{
		meshData = &(RegionByID[regionID]);
		prevID = regionID;
		iter = meshData->getVertices().begin();
	}

	if (iter != meshData->getVertices().end())
	{
		vx = float(iter->x());
		vy = float(iter->y());
		vz = float(iter->z());
		++iter;
	}
	return iter != meshData->getVertices().end();
}

int IOvdb::YieldPolygon(const wchar_t * const gridID, const wchar_t * const meshID, uint32_t &i1, uint32_t &i2, uint32_t &i3)
{
	static std::wstring prevID;
	static VolumeType *meshData = nullptr;
	static VolumePolygonsType::const_iterator iter;
	const std::wstring regionID = std::wstring(gridID) + std::wstring(meshID);
	if (regionID != prevID)
	{
		meshData = &(RegionByID[regionID]);
		prevID = regionID;
		iter = meshData->getPolygons().begin();
	}

	if (iter != meshData->getPolygons().end())
	{
		i1 = uint32_t(iter->x());
		i2 = uint32_t(iter->y());
		i3 = uint32_t(iter->z());
		++iter;
	}
	return iter != meshData->getPolygons().end();
}

int IOvdb::YieldNormal(const wchar_t * const gridID, const wchar_t * const meshID, float &nx, float &ny, float &nz)
{
	static std::wstring prevID;
	static VolumeType *meshData = nullptr;
	static VolumeNormalsType::const_iterator iter;
	const std::wstring regionID = std::wstring(gridID) + std::wstring(meshID);
	if (regionID != prevID)
	{
		meshData = &(RegionByID[regionID]);
		prevID = regionID;
		iter = meshData->getNormals().begin();
	}

	if (iter != meshData->getNormals().end())
	{
		nx = float(iter->x());
		ny = float(iter->y());
		nz = float(iter->z());
		++iter;
	}
	return iter != meshData->getNormals().end();
}

int IOvdb::CreateLibNoiseGrid(const wchar_t * const gridID, int sizeX, int sizeY, int sizeZ, float surfaceValue, double scaleXYZ, double frequency, double lacunarity, double persistence, int octaveCount)
{
	try
	{
		noise::module::Perlin perlin;
		perlin.SetFrequency(frequency);
		perlin.SetLacunarity(lacunarity * scaleXYZ);
		perlin.SetPersistence(persistence);
		perlin.SetOctaveCount(octaveCount);
		//perlin.SetFrequency(1.0);
		//perlin.SetLacunarity(2.01 * scaleXYZ);
		//perlin.SetPersistence(0.5);
		//perlin.SetOctaveCount(9);

		InitializeGrid(gridID);
		openvdb::FloatGrid::Ptr noiseGrid = getGridByID(gridID);
		openvdb::FloatGrid::Accessor acc = noiseGrid->getAccessor();
		noiseGrid->setTransform(openvdb::math::Transform::Ptr(new openvdb::math::Transform(openvdb::math::MapBase::Ptr(new openvdb::math::ScaleMap(openvdb::Vec3d(scaleXYZ))))));
		openvdb::BoolGrid::Ptr mask = openvdb::tools::clip_internal::convertToBoolMaskGrid(*noiseGrid);
		openvdb::BoolGrid::Accessor maskAcc = mask->getAccessor();

		for (int x = 0; x <= sizeX; ++x)
		{
			for (int y = 0; y <= sizeY; ++y)
			{
				for (int z = 0; z <= sizeZ; ++z)
				{
					const openvdb::Coord coord = openvdb::Coord(x, y, z);
					const openvdb::Vec3d xyz = noiseGrid->indexToWorld(coord);
					const double &vx = xyz.x();
					const double &vy = xyz.y();
					const double &vz = xyz.z();
					//Start with a plane at z = 0
					double density = -vz;
					//Add the density value to the plane
					density += perlin.GetValue(vx, vy, vz);
					acc.setValueOnly(coord, (float)density);
					if (x < sizeX && y < sizeY && z < sizeZ)
					{
						//If the voxel is not a pad voxel turn it on
						//Otherwise an activated voxel with background value 0 appears as if it is on the surface when it should not even be used at all
						maskAcc.setValueOn(coord);
					}
				}
			}
		}

		for (openvdb::BoolGrid::ValueOnIter i = mask->beginValueOn(); i; ++i)
		{
			const openvdb::Coord &coord = i.getCoord();
			const openvdb::Coord r = coord.offsetBy(1, 0, 0);
			const openvdb::Coord f = coord.offsetBy(0, 1, 0);
			const openvdb::Coord u = coord.offsetBy(0, 0, 1);
			const openvdb::Coord rf = coord.offsetBy(1, 1, 0);
			const openvdb::Coord ru = coord.offsetBy(1, 0, 1);
			const openvdb::Coord fu = coord.offsetBy(0, 1, 1);
			const openvdb::Coord rfu = coord.offsetBy(1, 1, 1);
			const float value = acc.getValue(coord);
			const float neighborValue100 = acc.getValue(r);
			const float neighborValue010 = acc.getValue(f);
			const float neighborValue001 = acc.getValue(u);
			const float neighborValue110 = acc.getValue(rf);
			const float neighborValue101 = acc.getValue(ru);
			const float neighborValue011 = acc.getValue(fu);
			const float neighborValue111 = acc.getValue(rfu);
			uint8_t insideBits = 0;
			//For each neighboring value set a bit if it is inside the surface (inside = positive value)
			if (value > surfaceValue)
			{
				insideBits |= 1;
			}
			if (neighborValue100 > surfaceValue)
			{
				insideBits |= 2;
			}
			if (neighborValue010 > surfaceValue)
			{
				insideBits |= 4;
			}
			if (neighborValue001 > surfaceValue)
			{
				insideBits |= 8;
			}
			if (neighborValue110 > surfaceValue)
			{
				insideBits |= 16;
			}
			if (neighborValue101 > surfaceValue)
			{
				insideBits |= 32;
			}
			if (neighborValue011 > surfaceValue)
			{
				insideBits |= 64;
			}
			if (neighborValue111 > surfaceValue)
			{
				insideBits |= 128;
			}
			if (insideBits > 0 && insideBits < 255)
			{
				//At least one vertex (but not all nor none) is on the other side of the surface from the others so activate the voxel
				acc.setValueOn(coord);
			}
		}
	}
	catch (openvdb::Exception &e)
	{
		std::ofstream logfile;
		logfile.open("C:\\Users\\zach\\Documents\\Unreal Projects\\obtuse-tanuki\\CosmicSafari\\ThirdParty\\exceptions.log");
		logfile << "CreateLibNoiseGrid: " << e.what() << std::endl;
		logfile.close();
		return 1;
	}
	return 0;
}

void addGrid(GridPtr grid, const std::wstring &gridID)
{
	GridByID[gridID] = grid;
}

GridPtr getGridByID(const std::wstring &gridID)
{
	checkGridID(gridID);
	GridPtr grid = openvdb::gridPtrCast<GridVdbType>(GridByID[gridID]);
	checkGrid(gridID, grid);
	return grid;
}

GridCPtr getConstGridByID(const std::wstring &gridID)
{
	checkGridID(gridID);
	GridCPtr constGrid = openvdb::gridConstPtrCast<GridVdbType>(GridByID[gridID]);
	checkGrid(gridID, constGrid);
	return constGrid;
}

void checkGrid(const std::wstring &gridID, const GridCPtr constGrid)
{
	if (constGrid == nullptr)
	{
		std::wostringstream message;
		message << "Grid ID " << gridID << " is not a float grid (or something else bad happened)!";
		std::wstring str = message.str();
		OPENVDB_THROW(openvdb::RuntimeError, std::string(str.begin(), str.end()));
	}
}

void checkGridID(const std::wstring &gridID)
{
	if (gridID.empty())
	{
		OPENVDB_THROW(openvdb::RuntimeError, "Invalid grid ID!");
	}

	if (GridByID.empty() || GridByID.find(gridID) == GridByID.end())
	{
		std::wostringstream message;
		message << "Grid ID " << gridID << " does not exist!";
		std::wstring str = message.str();
		OPENVDB_THROW(openvdb::RuntimeError, std::string(str.begin(), str.end()));
	}
}

void checkRegionID(const std::wstring &regionID)
{
	if (regionID.empty())
	{
		OPENVDB_THROW(openvdb::RuntimeError, "Invalid mesh ID!");
	}

	if (RegionByID.empty() || RegionByID.find(regionID) == RegionByID.end())
	{
		std::wostringstream message;
		message << "Mesh ID " << regionID << " does not exist!";
		std::wstring str = message.str();
		OPENVDB_THROW(openvdb::RuntimeError, std::string(str.begin(), str.end()));
	}
}

std::string gridNamesList(const openvdb::io::File &file)
{
	//Used for printing debug info
	//Must call with an open file
	std::string validNames;
	openvdb::io::File::NameIterator nameIter = file.beginName();
	while (nameIter != file.endName())
	{
		validNames += nameIter.gridName();
		++nameIter;
		if (nameIter != file.endName())
		{
			validNames += ", ";
		}
	}
	return validNames;
}