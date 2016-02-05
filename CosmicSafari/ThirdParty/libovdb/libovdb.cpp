#include "OvdbTypes.h"
#include "OvdbVolume.h"
#include "OvdbNoise.h"
#include <fstream>

using namespace ovdb; //TODO: Remove 'using' to match UE4 coding standards
using namespace ovdb::meshing; //TODO: Remove 'using' to match UE4 coding standards
using namespace ovdb::tools;

typedef OvdbVoxelVolume<TreeVdbType> VolumeType;
std::map<meshing::IDType, GridPtr> GridRegions;
std::map<IDType, VolumeType> GridVolumes;

void fillTerrainHoles(GridPtr grid, const float outsideValue);
bool isVoxelMissing(GridCPtr grid, const CoordType &coord, const float outsideValue);
IDType addGridRegion(GridPtr grid, const std::wstring &gridInfo);
GridPtr getGridByID(IDType gridID);
GridCPtr getConstGridByID(IDType gridID);
void checkGrid(IDType gridID, const GridCPtr constGrid);
void checkGridID(IDType gridID);
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

int OvdbWriteVdbGrid(IDType gridID, const std::string &filename)
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

int OvdbVolumeToMesh(const std::string &filename, const std::string &gridName, OvdbMeshMethod meshMethod, float isoValue)
{
	int error = 0;
	try
	{
		//Read a grid from file and then mesh it
		IDType gridID = INVALID_GRID_ID;
		OvdbReadVdb(filename, gridName, gridID);
		OvdbVolumeToMesh(gridID, meshMethod, isoValue);
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

int OvdbVolumeToMesh(IDType gridID, OvdbMeshMethod meshMethod, float isoValue)
{
    int error = 0;
    try
    {
		GridCPtr grid = getGridByID(gridID);
		VolumeType &volumeData = GridVolumes[gridID];
		volumeData.buildVolume(gridID, VOLUME_STYLE_CUBE, isoValue);
		volumeData.doMesh(gridID, meshMethod);
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

int OvdbYieldNextMeshPoint(IDType gridID, float &vx, float &vy, float &vz)
{
	static IDType prevID = INVALID_GRID_ID;
	static VolumeType *volumeData = nullptr;
	static GridCPtr grid = nullptr;
	static VolumeVerticesType::const_iterator iter;
	if (gridID != prevID)
	{
		grid = getConstGridByID(gridID);
		volumeData = &(GridVolumes[gridID]);
		prevID = gridID;
		iter = volumeData->getVertices(gridID).begin();
	}

    if (iter == volumeData->getVertices(gridID).end())
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

int OvdbYieldNextMeshPolygon(IDType gridID, uint32_t &i1, uint32_t &i2, uint32_t &i3)
{
	static IDType prevID = INVALID_GRID_ID;
	static VolumeType *volumeData = nullptr;
	static GridCPtr grid = nullptr;
	static VolumePolygonsType::const_iterator iter;
	if (gridID != prevID)
	{
		grid = getConstGridByID(gridID);
		volumeData = &(GridVolumes[gridID]);
		prevID = gridID;
		iter = volumeData->getPolygons(gridID).begin();
	}

	if (iter == volumeData->getPolygons(gridID).end())
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

int OvdbYieldNextMeshNormal(IDType gridID, float &nx, float &ny, float &nz)
{
	static IDType prevID = INVALID_GRID_ID;
	static VolumeType *volumeData = nullptr;
	static GridCPtr grid = nullptr;
	static VolumeNormalsType::const_iterator iter;
	if (gridID != prevID)
	{
		grid = getConstGridByID(gridID);
		volumeData = &(GridVolumes[gridID]);
		prevID = gridID;
		iter = volumeData->getNormals(gridID).begin();
	}

	if (iter == volumeData->getNormals(gridID).end())
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
IDType OvdbCreateLibNoiseVolume(const std::string &gridName, float surfaceValue, const VolumeDimensions &dimensions, uint32_t libnoiseRange, float &isovalue)
{
	IDType gridID = INVALID_GRID_ID;
	const openvdb::math::CoordBBox mapBounds(CoordType(dimensions.x0, dimensions.y0, dimensions.z0), CoordType(dimensions.x1, dimensions.y1, dimensions.z1));
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
		GridPtr grid = openvdb::createLevelSet<GridVdbType>();
		grid->setName(gridInfoStr.str());

		//Activate each voxel at the height map value
		for (int x = mapBounds.min().x(); x <= mapBounds.max().x(); x++)
		{
			for (int y = mapBounds.min().y(); y <= mapBounds.max().y(); y++)
			{
				int h = int(openvdb::math::RoundDown((noiseMap.GetValue(x, y) + noiseZ0) * noiseValueToWorldConversion));
				grid->tree().setValue(CoordType(x, y, h), isovalue);
			}
		}
		openvdb::tools::pruneLevelSet(grid->tree());
		//Note: signed flood-fill will not work (crashes) if a level-set grid is broken up into multiple regions
		openvdb::tools::doSignedFloodFill(grid->tree(), outsideValue, insideValue, true, 1); //TODO: Mess with the grain size parameter
		fillTerrainHoles(grid, outsideValue);
		std::string str = gridInfoStr.str();
		gridID = addGridRegion(grid, std::wstring(str.begin(), str.end()));
	}
	return gridID;
}

//Fill in the holes which are on the sides of inclines by checking for an inside voxel that is off and adjacent to an outside value
void fillTerrainHoles(GridPtr grid, const float outsideValue)
{
	for (auto i = grid->beginValueOn(); i; ++i)
	{
		if (!i.isVoxelValue()) //Skip tile values
		{
			continue;
		}
		const CoordType coord = i.getCoord();
		CoordType below(i.getCoord().x(), i.getCoord().y(), i.getCoord().z());
		//Start at the voxel immediately below the current voxel and check if any single neighboring voxel is outside
		for (int32_t z = 1; z <= i.getCoord().z(); z++)
		{
			//If there is a neighboring 'missing' voxel, that means there is a hole in the terrain so activate this voxel to patch up the hole
			CoordType below = coord.offsetBy(0, 0, -z);
			if (isVoxelMissing(grid, below, outsideValue))
			{
				//Activate the voxel while retaining the voxel 'outside' value so that the actual surface can still be differentiated by voxels on the sides of inclines
				grid->getAccessor().setValueOn(below);
			}
		}
	}
}

//Check if a voxel is inside but adjacent to an outside voxel (i.e. it is a "hole" in the terrain)
bool isVoxelMissing(GridCPtr grid, const CoordType &coord, const float outsideValue)
{
	//If the coord pertains to a voxel (not a tile), check for any one adjacent voxel to the below-voxel that is an 'outside value'
	//TODO: How to guarantee access to geometrically neighboring voxel values (instead of tile values) without potentially skipping a coord?
	CoordType adjacentL = coord.offsetBy(1, 0, 0);  //To the left
	CoordType adjacentR = coord.offsetBy(-1, 0, 0); //To the right
	CoordType adjacentF = coord.offsetBy(0, 1, 0);  //To the front
	CoordType adjacentB = coord.offsetBy(0, -1, 0); //To the back
	//Check that any one adjacent value is a voxel, is off, and is outside
	GridCAcc acc = grid->getConstAccessor();
	return acc.isVoxel(coord) &&
	 ((acc.isVoxel(adjacentL) && !acc.isValueOn(adjacentL) && openvdb::math::isApproxEqual(acc.getValue(adjacentL), outsideValue)) ||
	  (acc.isVoxel(adjacentR) && !acc.isValueOn(adjacentR) && openvdb::math::isApproxEqual(acc.getValue(adjacentR), outsideValue)) ||
	  (acc.isVoxel(adjacentF) && !acc.isValueOn(adjacentF) && openvdb::math::isApproxEqual(acc.getValue(adjacentF), outsideValue)) ||
	  (acc.isVoxel(adjacentB) && !acc.isValueOn(adjacentB) && openvdb::math::isApproxEqual(acc.getValue(adjacentB), outsideValue)));
}

IDType addGridRegion(GridPtr grid, const std::wstring& gridInfo)
{
	std::wostringstream gridIDStr;
	gridIDStr << GridRegions.size() << gridInfo;
	IDType gridID = gridIDStr.str();
	GridRegions.insert(std::pair<IDType, GridPtr>(gridID, grid));
	VolumeType volume(grid);
	GridVolumes.insert(std::pair<IDType, VolumeType>(gridID, volume));
	return gridID;
}

GridPtr getGridByID(IDType gridID)
{
	checkGridID(gridID);
	GridPtr grid = openvdb::gridPtrCast<GridVdbType>(GridRegions[gridID]);
	checkGrid(gridID, grid);
	return grid;
}

GridCPtr getConstGridByID(IDType gridID)
{
	checkGridID(gridID);
	GridCPtr constGrid = openvdb::gridConstPtrCast<GridVdbType>(GridRegions[gridID]);
	checkGrid(gridID, constGrid);
	return constGrid;
}

void checkGrid(IDType gridID, const GridCPtr constGrid)
{
	if (constGrid == nullptr)
	{
		std::wostringstream message;
		message << "Grid ID " << gridID << " is not a float grid (or something else bad happened)!";
		std::wstring str = message.str();
		OPENVDB_THROW(openvdb::RuntimeError, std::string(str.begin(), str.end()));
	}
}

void checkGridID(IDType gridID)
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