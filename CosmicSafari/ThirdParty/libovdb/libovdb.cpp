#include "libovdb.h"
#include "OpenVDBIncludes.h"
#include "OvdbVolume.h"
#include "OvdbNoise.h"
#include <fstream>

typedef openvdb::FloatTree TreeType;
typedef OvdbVoxelVolume<TreeType> VolumeType;
typedef std::map<GridIDType, openvdb::FloatGrid::Ptr> GridData;
typedef std::map<GridIDType, VolumeType> VolumeData;
typedef VolumeData::iterator VolumeDataPtr;
typedef VolumeData::const_iterator VolumeDataConstPtr;

GridData GridRegions;
VolumeData GridVolumes;

GridIDType addGridRegion(openvdb::FloatGrid::Ptr grid, const std::wstring &gridInfo);
openvdb::FloatGrid::Ptr getGridByID(const GridIDType &gridID);
openvdb::FloatGrid::Ptr getGridByID(const GridIDType &gridID, VolumeDataPtr &volumeData);
openvdb::FloatGrid::ConstPtr getConstGridByID(const GridIDType &gridID);
openvdb::FloatGrid::ConstPtr getConstGridByID(const GridIDType &gridID, VolumeDataConstPtr &volumeConstData);
void checkGrid(const GridIDType &gridID, const openvdb::FloatGrid::ConstPtr constGrid);
void checkGridID(const GridIDType &gridID);
std::string gridNamesList(const openvdb::io::File &file);

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

int OvdbReadVdb(const std::string &filename, const std::string gridName, GridIDType &gridID)
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
		openvdb::FloatGrid::Ptr grid = openvdb::gridPtrCast<openvdb::FloatGrid>(g);
		if (!grid)
		{
			OPENVDB_THROW(openvdb::RuntimeError, "No valid grids in " + filename);
		}
		gridID = addGridRegion(grid, std::wstring(gridName.begin(), gridName.end()));
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

int OvdbWriteVdbGrid(const GridIDType &gridID, const std::string &filename)
{
	int error = 0;
	try
	{
		openvdb::FloatGrid::Ptr grid = getGridByID(gridID);
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

int OvdbVolumeToMesh(const std::string &filename, const std::string &gridName, OvdbMeshMethod meshMethod, float isovalue)
{
	int error = 0;
	try
	{
		//Read a grid from file and then mesh it
		GridIDType gridID = INVALID_GRID_ID;
		OvdbReadVdb(filename, gridName, gridID);
		OvdbVolumeToMesh(gridID, meshMethod, isovalue);
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

int OvdbVolumeToMesh(const GridIDType &gridID, OvdbMeshMethod meshMethod, float isovalue)
{
    int error = 0;
    try
    {
		VolumeDataPtr volumeData;
		openvdb::FloatGrid::ConstPtr grid = getGridByID(gridID, volumeData);
		volumeData->second.buildVolume(VOLUME_STYLE_CUBE, isovalue);
		volumeData->second.doMesh(meshMethod);
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

int OvdbYieldNextMeshPoint(const GridIDType &gridID, float &vx, float &vy, float &vz)
{
	static GridIDType prevID = INVALID_GRID_ID;
	static VolumeDataConstPtr volumeData;
	static openvdb::FloatGrid::ConstPtr grid = nullptr;
	static VolumeType::VolumeVerticesCIter iter;
	if (gridID != prevID)
	{
		grid = getConstGridByID(gridID, volumeData);
		prevID = gridID;
		iter = volumeData->second.verticesCBegin();
	}

    if (iter == volumeData->second.verticesCEnd())
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

int OvdbYieldNextMeshPolygon(const GridIDType &gridID, uint32_t &i1, uint32_t &i2, uint32_t &i3)
{
	static GridIDType prevID = INVALID_GRID_ID;
	static VolumeDataConstPtr volumeData;
	static openvdb::FloatGrid::ConstPtr grid = nullptr;
	static VolumeType::VolumePolygonsCIter iter;
	if (gridID != prevID)
	{
		grid = getConstGridByID(gridID, volumeData);
		prevID = gridID;
		iter = volumeData->second.polygonsCBegin();
	}

	if (iter == volumeData->second.polygonsCEnd())
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

int OvdbYieldNextMeshNormal(const GridIDType &gridID, float &nx, float &ny, float &nz)
{
	static GridIDType prevID = INVALID_GRID_ID;
	static VolumeDataConstPtr volumeData;
	static openvdb::FloatGrid::ConstPtr grid = nullptr;
	static VolumeType::VolumeNormalsCIter iter;
	if (gridID != prevID)
	{
		grid = getConstGridByID(gridID, volumeData);
		prevID = gridID;
		iter = volumeData->second.normalsCBegin();
	}

	if (iter == volumeData->second.normalsCEnd())
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

//Surface value must be between 0 and 1
GridIDType OvdbCreateLibNoiseVolume(const std::string &gridName, float surfaceValue, const VolumeDimensions &dimensions, uint32_t libnoiseRange, float &isovalue)
{
	GridIDType gridID = INVALID_GRID_ID;
	const openvdb::math::CoordBBox mapBounds(openvdb::math::Coord(dimensions.x0, dimensions.y0, dimensions.z0), openvdb::math::Coord(dimensions.x1, dimensions.y1, dimensions.z1));
	const noise::utils::NoiseMap &noiseMap = CreateNoiseHeightMap(1.0, dimensions.sizeX(), dimensions.sizeY());
	float minNoise, maxNoise;
	GetHeightMapRange(noiseMap, minNoise, maxNoise);
	
	std::ostringstream gridInfoStr;
	gridInfoStr << gridName << "[" << dimensions.x0 << "," << dimensions.y0 << "," << dimensions.z0 << "|" << dimensions.x1 << "," << dimensions.y1 << "," << dimensions.z1 << "]";

	{ //TODO: Refactor
		const float minNoiseValue = minNoise; //Lowest height map noise value
		const float maxNoiseValue = maxNoise; //Largest height map noise value
		const float outsideValue = isovalue + 1.0f; //Value considered as outside the level-set
		const float insideValue = -outsideValue; //Value considered as inside the level-set
		const float noiseRange = maxNoiseValue - minNoiseValue; //Maximum range of the height map. TODO: What if max value is negative or 0?
		const float noiseZ0 = -minNoiseValue; //Value to add to a noise map value so that all values start from 0
		const float noiseValueToWorldConversion = libnoiseRange / noiseRange; //Scale factor mapping from the height map to the specified range. TODO: Support zero noise range? (something like a flat surface?)

		//Isovalue is the surface value with respect to the possible range of height map values
		//Note: I have no idea if that is the actual definition of an isovalue but it feels good to me ¯\_(?)_/¯
		isovalue = noiseRange * surfaceValue; //TODO: Constrain surface value between 0 and 1?

		//Create the default level set
		openvdb::FloatGrid::Ptr grid = openvdb::createLevelSet<openvdb::FloatGrid>();
		grid->setName(gridInfoStr.str());

		//Activate each voxel at the height map value
		for (int x = mapBounds.min().x(); x <= mapBounds.max().x(); x++)
		{
			for (int y = mapBounds.min().y(); y <= mapBounds.max().y(); y++)
			{
				int h = int(openvdb::math::RoundDown((noiseMap.GetValue(x, y) + noiseZ0) * noiseValueToWorldConversion));
				grid->tree().setValue(openvdb::Coord(x, y, h), isovalue);
			}
		}
		openvdb::tools::pruneLevelSet(grid->tree());
		//Signed flood-fill not working when breaking the level into multiple regions
		openvdb::tools::doSignedFloodFill(grid->tree(), outsideValue, insideValue, true, 1); //TODO: Mess with the grain size parameter

		//Fill in the holes which are on the sides of inclines by checking for an inside voxel that is off and adjacent to an outside value
		auto acc = grid->getAccessor();
		for (auto i = grid->beginValueOn(); i; ++i)
		{
			if (!i.isVoxelValue()) //Skip tile values
			{
				continue;
			}
			openvdb::Coord coord = i.getCoord();
			openvdb::Coord below = coord;
			openvdb::Coord adjacent;
			for (int32_t z = coord.z(); z >= 0; z--)
			{
				coord.setZ(z);
				//TODO: How to guarantee access to geometrically neighboring voxels?
				adjacent = coord.offsetBy(1, 0, 0);
				if (acc.isVoxel(adjacent) && !acc.isValueOn(adjacent) && openvdb::math::isApproxEqual(acc.getValue(adjacent), outsideValue))
				{
					acc.setValueOnly(coord, isovalue);
				}
				else
				{
					adjacent = coord.offsetBy(-1, 0, 0);
					if (acc.isVoxel(adjacent) && !acc.isValueOn(adjacent) && openvdb::math::isApproxEqual(acc.getValue(adjacent), outsideValue))
					{
						acc.setValueOn(coord, isovalue);
					}
					else
					{
						adjacent = coord.offsetBy(0, 1, 0);
						if (acc.isVoxel(adjacent) && !acc.isValueOn(adjacent) && openvdb::math::isApproxEqual(acc.getValue(adjacent), outsideValue))
						{
							acc.setValueOn(coord, isovalue);
						}
						else
						{
							adjacent = coord.offsetBy(0, -1, 0);
							if (acc.isVoxel(adjacent) && !acc.isValueOn(adjacent) && openvdb::math::isApproxEqual(acc.getValue(adjacent), outsideValue))
							{
								acc.setValueOn(coord, isovalue);
							}
						}
					}
				}
			}
		}
		std::string str = gridInfoStr.str();
		gridID = addGridRegion(grid, std::wstring(str.begin(), str.end()));
	}
	return gridID;
}

GridIDType addGridRegion(openvdb::FloatGrid::Ptr grid, const std::wstring& gridInfo)
{
	std::wostringstream gridIDStr;
	gridIDStr << GridRegions.size() << gridInfo;
	GridIDType gridID = gridIDStr.str();
	GridRegions.insert(std::pair<GridIDType, openvdb::FloatGrid::Ptr>(gridID, grid));
	VolumeType volume(grid);
	GridVolumes.insert(std::pair<GridIDType, VolumeType>(gridID, volume));
	return gridID;
}

openvdb::FloatGrid::Ptr getGridByID(const GridIDType &gridID)
{
	checkGridID(gridID);
	openvdb::FloatGrid::Ptr grid = openvdb::gridPtrCast<openvdb::FloatGrid>(GridRegions[gridID]);
	checkGrid(gridID, grid);
	return grid;
}

openvdb::FloatGrid::Ptr getGridByID(const GridIDType &gridID,  VolumeDataPtr &volumeData)
{
	openvdb::FloatGrid::Ptr grid = getGridByID(gridID);
	volumeData = GridVolumes.find(gridID);
	return grid;
}

openvdb::FloatGrid::ConstPtr getConstGridByID(const GridIDType &gridID)
{
	checkGridID(gridID);
	openvdb::FloatGrid::ConstPtr constGrid = openvdb::gridConstPtrCast<openvdb::FloatGrid>(GridRegions[gridID]);
	checkGrid(gridID, constGrid);
	return constGrid;
}

openvdb::FloatGrid::ConstPtr getConstGridByID(const GridIDType &gridID, VolumeDataConstPtr &volumeConstData)
{
	openvdb::FloatGrid::ConstPtr constGrid = getConstGridByID(gridID);
	volumeConstData = GridVolumes.find(gridID);
	return constGrid;
}

void checkGrid(const GridIDType &gridID, const openvdb::FloatGrid::ConstPtr constGrid)
{
	if (constGrid == nullptr)
	{
		std::wostringstream message;
		message << "Grid ID " << gridID << " is not a float grid (or something else bad happened)!";
		std::wstring str = message.str();
		OPENVDB_THROW(openvdb::RuntimeError, std::string(str.begin(), str.end()));
	}
}

void checkGridID(const GridIDType &gridID)
{
	if (gridID == INVALID_GRID_ID)
	{
		OPENVDB_THROW(openvdb::RuntimeError, "Invalid grid ID!");
	}
	
	if (GridRegions.empty() || GridRegions.find(gridID) == GridRegions.end())
	{
		std::wostringstream message;
		message << "Grid ID " << gridID << " does not exist!";
		std::wstring str = message.str();
		OPENVDB_THROW(openvdb::RuntimeError, std::string(str.begin(), str.end()));
	}

	if (GridVolumes.empty() || GridVolumes.find(gridID) == GridVolumes.end())
	{
		std::wostringstream message;
		message << "Grid volume ID " << gridID << " does not exist!";
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