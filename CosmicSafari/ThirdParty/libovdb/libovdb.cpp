#include "libovdb.h"
#include "OpenVDBIncludes.h"
#include "OvdbVolume.h"
#include "OvdbNoise.h"
#include <fstream>

typedef float GridType;
typedef openvdb::FloatTree TreeType;
std::vector<openvdb::FloatGrid::Ptr> GridRegions;
std::vector< OvdbVoxelVolume<TreeType> > GridVolumes;
const static uint32_t INVALID_GRID_ID = UINT32_MAX;

uint32_t addGridRegion(openvdb::FloatGrid::Ptr grid);
openvdb::FloatGrid::Ptr getGridByID(uint32_t gridID);
openvdb::FloatGrid::Ptr getGridByID(uint32_t gridID, OvdbVoxelVolume<TreeType> *volume);
openvdb::FloatGrid::ConstPtr getConstGridByID(uint32_t gridID);
openvdb::FloatGrid::ConstPtr getConstGridByID(uint32_t gridID, const OvdbVoxelVolume<TreeType> *constVolume);
void checkGridID(uint32_t gridID);
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

int OvdbReadVdb(const std::string &filename, const std::string &gridName, uint32_t &gridID)
{
    int error = 0;
    try
    {
		//A .vdb might have multiple grids, so we need the grid name along with just the filename
        openvdb::io::File file(filename);
        file.open();
		if (file.getSize() < 1)
		{
			OPENVDB_THROW(openvdb::IoError, "Could not read " + filename);
		}
		std::string gridNames = gridNamesList(file);

		auto i = file.getGrids()->cbegin();
		for (; i != file.getGrids()->end() && i->get()->getName() != gridName; ++i);
		if (i == file.getGrids()->end())
		{
			OPENVDB_THROW(openvdb::RuntimeError, "Unable to find grid \"" + gridName + "\" in " + filename);
		}
		
		openvdb::FloatGrid::Ptr grid = openvdb::gridPtrCast<openvdb::FloatGrid>(*i);
		file.close();
		if (*i == nullptr)
		{
			OPENVDB_THROW(openvdb::RuntimeError, "Failed to cast grid \"" + gridName + "\". Valid grid names are: " + gridNames);
		}        
		gridID = addGridRegion(grid);
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

int OvdbWriteVdb(const std::string &filename, uint32_t gridID)
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

int OvdbVolumeToMesh(const std::string &filename, const std::string &gridName, OvdbMeshMethod meshMethod, float surfaceValue)
{
	int error = 0;
	try
	{
		//Read a grid from file and then mesh it
		uint32_t gridID = INVALID_GRID_ID;
		OvdbReadVdb(filename, gridName, gridID);
		OvdbVolumeToMesh(gridID, meshMethod, surfaceValue);
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

int OvdbVolumeToMesh(uint32_t gridID, OvdbMeshMethod meshMethod, float surfaceValue)
{
    int error = 0;
    try
    {
		OvdbVoxelVolume<TreeType> * volume = nullptr;
		openvdb::FloatGrid::ConstPtr grid = getGridByID(gridID, volume);
		assert(volume);
		volume->buildVolume(grid->evalActiveVoxelBoundingBox(), surfaceValue);
		volume->doMesh(meshMethod);
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

int OvdbYieldNextMeshPoint(uint32_t gridID, float &vx, float &vy, float &vz)
{
	static uint32_t prevID = INVALID_GRID_ID;
	static OvdbVoxelVolume<TreeType> * volume = nullptr;
	static openvdb::FloatGrid::ConstPtr grid = nullptr;
	static std::vector<QuadVertexType>::const_iterator iter;
	if (gridID != prevID)
	{
		getConstGridByID(gridID, volume);
		assert(volume);
		prevID = gridID;
		iter = volume->verticesCBegin();
	}

    if (iter == volume->verticesCEnd())
    {
        return 0;
    }
    vx = float(iter->x());
    vy = float(iter->y());
    vz = float(iter->z());
	++iter;
    return 1;
}

int OvdbYieldNextMeshPolygon(uint32_t gridID, uint32_t &i1, uint32_t &i2, uint32_t &i3)
{
	static uint32_t prevID = INVALID_GRID_ID;
	static OvdbVoxelVolume<TreeType> * volume = nullptr;
	static openvdb::FloatGrid::ConstPtr grid = nullptr;
	static std::vector<PolygonIndicesType>::const_iterator iter;
	if (gridID != prevID)
	{
		getConstGridByID(gridID, volume);
		assert(volume);
		prevID = gridID;
		iter = volume->polygonsCBegin();
	}

	if (iter == volume->polygonsCEnd())
	{
		return 0;
	}
    i1 = uint32_t(iter->x());
    i2 = uint32_t(iter->y());
    i3 = uint32_t(iter->z());
	++iter;
    return 1;
}

int OvdbYieldNextMeshNormal(uint32_t gridID, float &nx, float &ny, float &nz)
{
	static uint32_t prevID = INVALID_GRID_ID;
	static OvdbVoxelVolume<TreeType> * volume = nullptr;
	static openvdb::FloatGrid::ConstPtr grid = nullptr;
	static std::vector<QuadVertexType>::const_iterator iter;
	if (gridID != prevID)
	{
		getConstGridByID(gridID, volume);
		assert(volume);
		prevID = gridID;
		iter = volume->normalsCBegin();
	}

	if (iter == volume->normalsCEnd())
	{
		return 0;
	}
	nx = float(iter->x());
	ny = float(iter->y());
	nz = float(iter->z());
	++iter;
	return 1;
}

//Surface value must be between 0 and 1
int OvdbCreateLibNoiseVolume(const std::string &gridName, float surfaceValue, uint32_t dimX, uint32_t dimY, uint32_t dimZ, uint32_t &gridID, float &isovalue)
{
	openvdb::math::Coord minBounds(0, 0, 0);
	openvdb::math::Coord maxBounds(dimX - 1, dimY - 1, dimZ - 1);
	openvdb::math::CoordBBox mapBounds(minBounds, maxBounds);
	openvdb::tools::Dense<GridType> denseGrid(mapBounds);
	noise::utils::NoiseMap &noiseMap = CreateNoiseHeightMap(1.0, dimX, dimY);

	float minNoiseValue, maxNoiseValue;
	GetHeightMapRange(noiseMap, minNoiseValue, maxNoiseValue);
	float noiseRange = maxNoiseValue - fabs(minNoiseValue); //TODO: What happens if the max noise value is negative?
	float noiseZ0 = 0.0f;
	if (minNoiseValue < 0.0f)
	{
		noiseZ0 = fabs(minNoiseValue) + 1.0f;
	}
	else if (minNoiseValue > 0.00001f)
	{
		noiseZ0 = -minNoiseValue + 1.0f;
	}

	isovalue = noiseRange * surfaceValue;
	float noiseValueToWorldConversion = (denseGrid.bbox().max().z() - denseGrid.bbox().min().z()) / noiseRange;
	for (int x = denseGrid.bbox().min().x(); x <= denseGrid.bbox().max().x(); x++)
	{
		for (int y = denseGrid.bbox().min().y(); y <= denseGrid.bbox().max().y(); y++)
		{
			int h = int(openvdb::math::RoundDown((noiseMap.GetValue(x, y) + noiseZ0) * noiseValueToWorldConversion));
			for (int z = denseGrid.bbox().min().z(); z < h; z++)
			{
				denseGrid.setValue(openvdb::Coord(x, y, z), isovalue - (h - z));
			}
			denseGrid.setValue(openvdb::Coord(x, y, h), isovalue);
			for (int z = h + 1; z <= denseGrid.bbox().max().z(); z++)
			{
				denseGrid.setValue(openvdb::Coord(x, y, z), isovalue + z);
			}
		}
	}

	openvdb::FloatGrid::Ptr grid = openvdb::FloatGrid::create();
	openvdb::tools::copyFromDense(denseGrid, *grid, 0.0f);
	grid->setName(gridName);
	grid->setGridClass(openvdb::GRID_LEVEL_SET);
	openvdb::tools::pruneLevelSet(grid->tree());
	gridID = addGridRegion(grid);
	return 0;
}

uint32_t addGridRegion(openvdb::FloatGrid::Ptr grid)
{
	GridRegions.push_back(grid);
	OvdbVoxelVolume<openvdb::FloatTree> volume(grid);
	GridVolumes.push_back(volume);
	uint32_t gridID = GridRegions.size() - 1; //TODO: Error check index range (size_t could be bigger)
	return gridID;
}

openvdb::FloatGrid::Ptr getGridByID(uint32_t gridID)
{
	checkGridID(gridID);
	openvdb::FloatGrid::Ptr grid = openvdb::gridPtrCast<openvdb::FloatGrid>(GridRegions[gridID]);
	if (grid == nullptr)
	{
		std::ostringstream message;
		message << "Grid ID " << gridID << " is not a float grid (or something else bad happened)!";
		OPENVDB_THROW(openvdb::RuntimeError, message.str());
	}
	return grid;
}

openvdb::FloatGrid::Ptr getGridByID(uint32_t gridID, OvdbVoxelVolume<TreeType> *volume)
{
	openvdb::FloatGrid::Ptr grid = getGridByID(gridID);
	volume = &GridVolumes[gridID]; //Note: index already checked in getGridByID(gridID)
	return grid;
}

openvdb::FloatGrid::ConstPtr getConstGridByID(uint32_t gridID)
{
	checkGridID(gridID);
	openvdb::FloatGrid::ConstPtr constGrid = openvdb::gridPtrCast<openvdb::FloatGrid>(GridRegions[gridID]);
	if (constGrid == nullptr)
	{
		std::ostringstream message;
		message << "Grid ID " << gridID << " is not a float grid (or something else bad happened)!";
		OPENVDB_THROW(openvdb::RuntimeError, message.str());
	}
	return constGrid;
}

openvdb::FloatGrid::ConstPtr getConstGridByID(uint32_t gridID, const OvdbVoxelVolume<TreeType> *constVolume)
{
	openvdb::FloatGrid::ConstPtr constGrid = getConstGridByID(gridID);
	constVolume = &GridVolumes[gridID]; //Note: index already checked in getGridByID(gridID)
	return constGrid;
}

void checkGridID(uint32_t gridID)
{
	if (gridID == INVALID_GRID_ID)
	{
		OPENVDB_THROW(openvdb::RuntimeError, "Invalid grid ID!");
	}
	
	if (GridRegions.empty() || gridID >= GridRegions.size())
	{
		std::ostringstream message;
		message << "Grid ID " << gridID << " does not exist!";
		OPENVDB_THROW(openvdb::RuntimeError, message.str());
	}

	if (GridVolumes.empty() || gridID >= GridVolumes.size())
	{
		std::ostringstream message;
		message << "Grid volume ID " << gridID << " does not exist!";
		OPENVDB_THROW(openvdb::RuntimeError, message.str());
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