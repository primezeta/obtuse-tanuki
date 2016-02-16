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
openvdb::FloatGrid::Ptr create2DNoiseMap(const int32_t &width, const int32_t &height, const int32_t &range);

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
		auto i = GridVolumes.find(volumeID);
		if (i == GridVolumes.end())
		{
			i = GridVolumes.insert(std::make_pair(volumeID, VolumeType(grid))).first;
		}
		auto &volume = i->second;
		checkVolumeID(volumeID);
		openvdb::math::CoordBBox bbox(CoordType(volumeDims.x0, volumeDims.y0, volumeDims.z0), CoordType(volumeDims.x1, volumeDims.y1, volumeDims.z1));
		volume.doSurfaceMesh(bbox, VOLUME_STYLE_CUBE, isoValue, meshMethod);
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

//Surface value must be between 0 and 1
IDType OvdbCreateLibNoiseVolume(const ovdb::meshing::NameType &volumeName, float surfaceValue, const VolumeDimensions &volumeDimensions, float &isoValue)
{
	//Create a valid level-set from the 2D noise grid
	openvdb::FloatGrid::Ptr levelSet = create2DNoiseMap(volumeDimensions.sizeX(), volumeDimensions.sizeY(), volumeDimensions.sizeZ());
	//const openvdb::CoordBBox &noiseBounds = noiseGrid->evalActiveVoxelBoundingBox();
	//openvdb::FloatGrid::Accessor noiseAcc = noiseGrid->getAccessor();
	//openvdb::FloatGrid::Ptr levelSet = openvdb::createLevelSet<openvdb::FloatGrid>();
	//openvdb::FloatGrid::Accessor levelSetAcc = levelSet->getAccessor();

	//for (openvdb::FloatGrid::ValueOnCIter i = noiseGrid->cbeginValueOn(); i; ++i)
	//{
	//	//Find the coord with lowest z-value to calculate how many voxels we should set vertically
	//	const openvdb::Coord &coord = i.getCoord();

	//	//Always place at least 1 voxel and the very bottom voxel such that the level-set will be enclosed
	//	levelSetAcc.setValueOn(openvdb::Coord(coord.x(), coord.y(), 0));

	//	//At the boundaries set all voxels from top to bottom
	//	openvdb::Coord lowestCoord = coord;
	//	if (coord.x() == noiseBounds.min().x() ||
	//		coord.x() == noiseBounds.max().x() ||
	//		coord.y() == noiseBounds.min().y() ||
	//		coord.y() == noiseBounds.max().y())
	//	{
	//		lowestCoord = openvdb::Coord(0, 0, 1);
	//	}
	//	else
	//	{
	//		//Otherwise set voxels vertically down to the lowest neighboring height-map value
	//		if (coord.offsetBy(1, 0, 0).z() < lowestCoord.z()) { lowestCoord = coord.offsetBy(1, 0, 0); }
	//		if (coord.offsetBy(-1, 0, 0).z() < lowestCoord.z()) { lowestCoord = coord.offsetBy(-1, 0, 0); }
	//		if (coord.offsetBy(0, 1, 0).z() < lowestCoord.z()) { lowestCoord = coord.offsetBy(0, 1, 0); }
	//		if (coord.offsetBy(0, -1, 0).z() < lowestCoord.z()) { lowestCoord = coord.offsetBy(0, -1, 0); }
	//		if (coord.offsetBy(1, 1, 0).z() < lowestCoord.z()) { lowestCoord = coord.offsetBy(1, 1, 0); }
	//		if (coord.offsetBy(-1, 1, 0).z() < lowestCoord.z()) { lowestCoord = coord.offsetBy(-1, 1, 0); }
	//		if (coord.offsetBy(1, -1, 0).z() < lowestCoord.z()) { lowestCoord = coord.offsetBy(1, -1, 0); }
	//		if (coord.offsetBy(-1, -1, 0).z() < lowestCoord.z()) { lowestCoord = coord.offsetBy(-1, -1, 0); }
	//		lowestCoord = openvdb::Coord(lowestCoord.x(), lowestCoord.y(), noiseAcc.getValue(lowestCoord));
	//	}

	//	for (int32_t z = lowestCoord.z(); z <= coord.z(); ++z)
	//	{
	//		levelSetAcc.setValueOn(openvdb::Coord(coord.x(), coord.y(), z));
	//	}
	//}

	const float outsideValue = 1.0f;
	const float insideValue = -1.0f;
	//openvdb::tools::doSignedFloodFill(levelSet->tree(), outsideValue, insideValue, false, 1); //TODO: Mess with the grain size parameter

	//template<class GridType>
	//inline typename GridType::Ptr
	//levelSetRebuild(const GridType& grid, float isovalue = 0,
	//	float halfWidth = float(LEVEL_SET_HALF_WIDTH), const math::Transform* xform = NULL);

	openvdb::tools::pruneLevelSet(levelSet->tree());
	return addGrid(levelSet, std::wstring(volumeName.begin(), volumeName.end()));
}

IDType addGrid(GridPtr grid, const std::wstring& gridInfo)
{
	IDType gridID = gridInfo;
	GridByID.insert(std::pair<IDType, GridPtr>(gridID, grid));
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

openvdb::FloatGrid::Ptr create2DNoiseMap(const int32_t &width, const int32_t &height, const int32_t &range) //Note: Assume bbox dims are bounded below at 0. TODO: Error check bounds
{
	//Construct a height map across the coordinate range
	float minNoise, maxNoise;
	const noise::utils::NoiseMap &noiseMap = CreateNoiseHeightMap(1.0, width - 1, height - 1, minNoise, maxNoise);
	const float noiseRange = fabs(maxNoise - minNoise);
	const float noiseUnit = noiseRange / range;

	//Create a grid with transform according to noise map scale
	openvdb::VectorGrid::Ptr grid = openvdb::VectorGrid::create();
	grid->setName("libnoise");
	grid->insertMeta("range", openvdb::FloatMetadata(noiseRange));
	grid->insertMeta("minimum", openvdb::FloatMetadata(minNoise));
	grid->insertMeta("maximum", openvdb::FloatMetadata(maxNoise));
	grid->insertMeta("unit", openvdb::FloatMetadata(noiseUnit));
	const openvdb::Vec3d noiseScale(1.0, 1.0, noiseUnit);
	const openvdb::Vec3d noiseTranslate(0.0, 0.0, minNoise);
	openvdb::math::Transform::Ptr xyzTransform = openvdb::math::Transform::Ptr(new openvdb::math::Transform(openvdb::math::MapBase::Ptr(new openvdb::math::ScaleTranslateMap(noiseScale, noiseTranslate))));

	//Initialize each noise xy to the noise value
	openvdb::VectorGrid::Accessor acc = grid->getAccessor();
	for (int32_t x = 0; x < width; x++)
	{
		for (int32_t y = 0; y < height; y++)
		{
			const float noiseValue = noiseMap.GetValue(x, y);
			const openvdb::Vec3d worldXYZ(x, y, noiseValue);
			const openvdb::Coord xyz = xyzTransform->worldToIndexNodeCentered(worldXYZ);
			acc.setValueOn(xyz, openvdb::Vec3d(x, y, 0.0));

			//Initialize each voxel below to a multiple of min noise
			for (int32_t z = 1; z < xyz.z(); ++z)
			{
				acc.setValueOn(xyz.offsetBy(0, 0, -z), openvdb::Vec3d(x, y, z*minNoise));
			}
		}
	}

	openvdb::FloatGrid::Ptr divGrid = openvdb::tools::divergence(*grid);
	for (openvdb::FloatGrid::ValueOnIter i = divGrid->beginValueOn(); i; ++i)
	{
		const float &divValue = i.getValue();
		if (divValue < 4.0f*minNoise || openvdb::math::isApproxEqual(divValue, 4.0f*minNoise))
		{
			i.setActiveState(false);
		}
	}
	for (openvdb::FloatGrid::ValueOffIter i = divGrid->beginValueOff(); i; ++i)
	{
		if (i.getCoord().z() == 0)
		{
			i.setActiveState(true);
		}
		else if (i.getCoord().x() == 0 || i.getCoord().x() == width-1 ||
			     i.getCoord().y() == 0 || i.getCoord().y() == height-1)
		{
			if (i.getValue() < 0.0f)
			{
				i.setActiveState(true);
			}
		}
	}
	return divGrid;
}