#include "OvdbTypes.h"
#include "OvdbVolume.h"
#include "OvdbNoise.h"
#include <fstream>

using namespace ovdb; //TODO: Remove 'using' to match UE4 coding standards
using namespace ovdb::meshing; //TODO: Remove 'using' to match UE4 coding standards
using namespace ovdb::tools;

typedef std::map<IDType, GridPtr> GridByIDType;
typedef OvdbVoxelVolume<TreeVdbType> VolumeType;
typedef std::map<IDType, VolumeType, IDLessThan> GridVolumesType;

GridByIDType GridByID;
GridVolumesType GridVolumes;

IDType addGrid(GridPtr grid, const std::wstring &gridInfo);
GridPtr getGridByID(const IDType &gridID);
GridCPtr getConstGridByID(const IDType &gridID);
void checkGrid(const IDType &gridID, const GridCPtr constGrid);
void checkGridID(const IDType &gridID);
void checkVolumeID(const IDType &volumeID);
std::string gridNamesList(const openvdb::io::File &file);

//Return the scalar grid with each coord(x,y,z[floor noiseValue]) = noiseValue and return isoValue = noiseRange[fabs max - min] * surfaceValue[0,1]
openvdb::FloatGrid::Ptr create2DNoiseUnsignedDistanceGrid(const float surfaceValue, const int32_t &width, const int32_t &height, const int32_t &range);

int OvdbInitialize()
{
	int error = 0;
	try
	{
		openvdb::initialize();
	}
	catch (openvdb::Exception &e)
	{
		std::ofstream logfile;
		logfile.open("C:\\Users\\zach\\Documents\\Unreal Projects\\obtuse-tanuki\\CosmicSafari\\ThirdParty\\exceptions.log");
		logfile << "OvdbInitialize: " << e.what() << std::endl;
		logfile.close();
		error = 1;
	}
	return error;
}

int OvdbUninitialize()
{
	int error = 0;
	try
	{
		openvdb::uninitialize();
	}
	catch (openvdb::Exception &e)
	{
		std::ofstream logfile;
		logfile.open("C:\\Users\\zach\\Documents\\Unreal Projects\\obtuse-tanuki\\CosmicSafari\\ThirdParty\\exceptions.log");
		logfile << "OvdbUninitialize: " << e.what() << std::endl;
		logfile.close();
		error = 1;
	}
	return error;
}

int OvdbReadVdb(const std::string &filename, const std::string gridName, IDType &gridID)
{
	int error = 0;
	try
	{
		//A .vdb might have multiple grids, so we need the grid ID along with just the filename
		openvdb::io::File file(filename);
		file.open();
		if (file.getSize() < 1)
		{
			OPENVDB_THROW(openvdb::IoError, "Could not read " + filename);
		}
		std::string ids = gridNamesList(file);

		auto g = file.readGrid(gridName);
		file.close();
		if (!g)
		{
			OPENVDB_THROW(openvdb::RuntimeError, "Unable to find grid \"" + gridName + "\" in " + filename);
		}
		GridPtr grid = openvdb::gridPtrCast<GridVdbType>(g);
		if (!grid)
		{
			OPENVDB_THROW(openvdb::RuntimeError, "No valid grids in " + filename);
		}
		gridID = addGrid(grid, std::wstring(gridName.begin(), gridName.end()));
	}
	catch (openvdb::Exception &e)
	{
		std::ofstream logfile;
		logfile.open("C:\\Users\\zach\\Documents\\Unreal Projects\\obtuse-tanuki\\CosmicSafari\\ThirdParty\\exceptions.log");
		logfile << "OvdbReadVdbGrid: " << e.what() << std::endl;
		logfile.close();
		error = 1;
	}
	//TODO: Reorganize throw/catches to give a call trace
	return error;
}

int OvdbWriteVdbGrid(const IDType &gridID, const std::string &filename)
{
	int error = 0;
	try
	{
		GridPtr grid = getGridByID(gridID);
		openvdb::GridPtrVec grids;
		grids.push_back(grid);
		//TODO: Check if directory exists and whatever else would be Good Stuff to do
		openvdb::io::File file(filename);
		file.write(grids);
		file.close();
	}
	catch (openvdb::Exception &e)
	{
		std::ofstream logfile;
		logfile.open("C:\\Users\\zach\\Documents\\Unreal Projects\\obtuse-tanuki\\CosmicSafari\\ThirdParty\\exceptions.log");
		logfile << "OvdbWriteVdbGrid: " << e.what() << std::endl;
		logfile.close();
		error = 1;
	}
	return error;
}

//Note: Probably will eventually generate volumeID internally (e.g. for when using a database or some other scheme to store generated terrain)
int OvdbVolumeToMesh(const ovdb::meshing::IDType &gridID, const ovdb::meshing::IDType &volumeID, ovdb::meshing::VolumeDimensions volumeDims, ovdb::meshing::OvdbMeshMethod meshMethod, float isoValue)
{
	int error = 0;
	try
	{
		GridPtr grid = getGridByID(gridID);
		GridVolumes[volumeID] = VolumeType(grid);
		checkVolumeID(volumeID);
		openvdb::math::CoordBBox bbox(CoordType(volumeDims.x0, volumeDims.y0, volumeDims.z0), CoordType(volumeDims.x1, volumeDims.y1, volumeDims.z1));
		GridVolumes[volumeID].doSurfaceMesh(bbox, VOLUME_STYLE_CUBE, isoValue, meshMethod);
	}
	catch (openvdb::Exception &e)
	{
		std::ofstream logfile;
		logfile.open("C:\\Users\\zach\\Documents\\Unreal Projects\\obtuse-tanuki\\CosmicSafari\\ThirdParty\\exceptions.log");
		logfile << "OvdbVolumeToMesh: " << e.what() << std::endl;
		logfile.close();
		error = 1;
	}
	return error;
}

int OvdbYieldNextMeshPoint(const IDType &volumeID, float &vx, float &vy, float &vz)
{
	static IDType prevID = INVALID_GRID_ID;
	static VolumeType *volumeData = nullptr;
	static VolumeVerticesType::const_iterator iter;
	if (volumeID != prevID)
	{
		volumeData = &(GridVolumes[volumeID]);
		prevID = volumeID;
		iter = volumeData->getVertices().begin();
	}

	if (iter == volumeData->getVertices().end())
	{
		prevID = INVALID_GRID_ID;
		return 0;
	}
	vx = float(iter->x());
	vy = float(iter->y());
	vz = float(iter->z());
	++iter;
	return 1;
}

int OvdbYieldNextMeshPolygon(const IDType &volumeID, uint32_t &i1, uint32_t &i2, uint32_t &i3)
{
	static IDType prevID = INVALID_GRID_ID;
	static VolumeType *volumeData = nullptr;
	static VolumePolygonsType::const_iterator iter;
	if (volumeID != prevID)
	{
		volumeData = &(GridVolumes[volumeID]);
		prevID = volumeID;
		iter = volumeData->getPolygons().begin();
	}

	if (iter == volumeData->getPolygons().end())
	{
		prevID = INVALID_GRID_ID;
		return 0;
	}
	i1 = uint32_t(iter->x());
	i2 = uint32_t(iter->y());
	i3 = uint32_t(iter->z());
	++iter;
	return 1;
}

int OvdbYieldNextMeshNormal(const IDType &volumeID, float &nx, float &ny, float &nz)
{
	static IDType prevID = INVALID_GRID_ID;
	static VolumeType *volumeData = nullptr;
	static VolumeNormalsType::const_iterator iter;
	if (volumeID != prevID)
	{
		volumeData = &(GridVolumes[volumeID]);
		prevID = volumeID;
		iter = volumeData->getNormals().begin();
	}

	if (iter == volumeData->getNormals().end())
	{
		prevID = INVALID_GRID_ID;
		return 0;
	}
	nx = float(iter->x());
	ny = float(iter->y());
	nz = float(iter->z());
	++iter;
	return 1;
}

IDType OvdbCreateLibNoiseGrid(const ovdb::meshing::NameType &volumeName, const ovdb::meshing::VolumeDimensions &dimensions, float surfaceValue, double scaleXYZ, double frequency, double lacunarity, double persistence, int octaveCount)
{
	const int sizeX = dimensions.sizeX();
	const int sizeY = dimensions.sizeY();
	const int sizeZ = dimensions.sizeZ();

	noise::module::Perlin perlin;
	perlin.SetFrequency(frequency);
	perlin.SetLacunarity(lacunarity * scaleXYZ);
	perlin.SetPersistence(persistence);
	perlin.SetOctaveCount(octaveCount);
	//perlin.SetFrequency(1.0);
	//perlin.SetLacunarity(2.01 * scaleXYZ);
	//perlin.SetPersistence(0.5);
	//perlin.SetOctaveCount(9);

	openvdb::FloatGrid::Ptr noiseGrid = openvdb::createGrid<openvdb::FloatGrid>();
	openvdb::FloatGrid::Accessor acc = noiseGrid->getAccessor();
	noiseGrid->setTransform(openvdb::math::Transform::Ptr(new openvdb::math::Transform(openvdb::math::MapBase::Ptr(new openvdb::math::ScaleMap(openvdb::Vec3d(scaleXYZ))))));
	openvdb::BoolGrid::Ptr mask = openvdb::BoolGrid::create(false);
	mask->topologyUnion(*noiseGrid);
	mask->setTransform(noiseGrid->transform().copy());
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
	return addGrid(noiseGrid, std::wstring(volumeName.begin(), volumeName.end()));
}

IDType addGrid(GridPtr grid, const std::wstring& gridInfo)
{
	IDType gridID = gridInfo;
	GridByID[gridID] = grid;
	return gridID;
}

GridPtr getGridByID(const IDType &gridID)
{
	checkGridID(gridID);
	GridPtr grid = openvdb::gridPtrCast<GridVdbType>(GridByID[gridID]);
	checkGrid(gridID, grid);
	return grid;
}

GridCPtr getConstGridByID(const IDType &gridID)
{
	checkGridID(gridID);
	GridCPtr constGrid = openvdb::gridConstPtrCast<GridVdbType>(GridByID[gridID]);
	checkGrid(gridID, constGrid);
	return constGrid;
}

void checkGrid(const IDType &gridID, const GridCPtr constGrid)
{
	if (constGrid == nullptr)
	{
		std::wostringstream message;
		message << "Grid ID " << gridID << " is not a float grid (or something else bad happened)!";
		std::wstring str = message.str();
		OPENVDB_THROW(openvdb::RuntimeError, std::string(str.begin(), str.end()));
	}
}

void checkGridID(const IDType &gridID)
{
	if (gridID == INVALID_GRID_ID)
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

void checkVolumeID(const IDType &volumeID)
{
	if (volumeID == INVALID_GRID_ID)
	{
		OPENVDB_THROW(openvdb::RuntimeError, "Invalid volume ID!");
	}

	if (GridVolumes.empty() || GridVolumes.find(volumeID) == GridVolumes.end())
	{
		std::wostringstream message;
		message << "Volume ID " << volumeID << " does not exist!";
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