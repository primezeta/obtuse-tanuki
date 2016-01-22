// openvdb_create.cpp : Defines the entry point for the console application.
//

#include <string>
#include <openvdb/openvdb.h>
#include <openvdb/Exceptions.h>
#include <openvdb/tools/LevelSetSphere.h>
#include <openvdb/tools/Dense.h>
#include <openvdb/tools/MeshToVolume.h>
#include <openvdb/tools/VolumeToMesh.h>
#include <openvdb/tools/SignedFloodFill.h>
#include "openvdbnoise.h"
#include "../libovdb/libovdb.h"

const static float FLOAT_EPS = 0.00001f;
void usage();
void printGridStats(openvdb::FloatGrid::ConstPtr grid, bool printForVoxels, float comparisonEps = FLOAT_EPS);

//Cylinder creation code from http://kirilllykov.github.io/blog/2013/04/02/level-set-openvdb-intro-1/
void makeCylinder(openvdb::FloatGrid::Ptr grid, float radius, const openvdb::CoordBBox& indexBB, double h);
void createAndSaveCylinder();

int main(int argc, char * argv[])
{
	int exitStatus = EXIT_SUCCESS;
	try
	{
		openvdb::initialize();

		if (argc == 1)
		{
			usage();
		}
		std::string gridName = argv[1];

		openvdb::GridPtrVec grids;
		std::ostringstream filename;
		if (argc != 4)
		{
			usage();
		}

		int mapX = 0;
		int mapY = 0;
		int mapZ = 0;

		int mapSize = std::stoi(argv[2])-1;
		mapX = mapSize;
		mapY = mapSize;
		mapZ = 10; //TODO: Throw an error if map size is less than 10...or just set it to 10. Duhhhh
		std::cout << "Map height " << mapZ << std::endl;
		float mapScale = std::stof(argv[3]);
		filename << gridName << "-X" << mapX << "-Y" << mapY << "-Z" << mapZ << "_scale" << mapScale;

		noise::utils::NoiseMap& noiseMap = CreateNoiseHeightMap((float)mapScale, mapX, mapY);
		CreatNoiseBitmap(noiseMap, "noisemap.bmp");
		float maxHeightMapValue, minHeightMapValue, maxHeightMapValueShifted;
		GetHeightMapExtents(noiseMap, minHeightMapValue, maxHeightMapValue);
		maxHeightMapValueShifted = maxHeightMapValue + fabs(minHeightMapValue);

		openvdb::math::Coord minBounds(0, 0, 0);
		openvdb::math::Coord maxBounds(mapX, mapY, mapZ);
		openvdb::math::CoordBBox mapBounds(minBounds, maxBounds);

		openvdb::tools::Dense<float> denseGrid(mapBounds);
		std::cout << "dense grid bbox (" << denseGrid.bbox().min().x() << "," << denseGrid.bbox().min().y() << "," << denseGrid.bbox().min().z() << ") by ("
			<< denseGrid.bbox().max().x() << "," << denseGrid.bbox().max().y() << "," << denseGrid.bbox().max().z() << ")" << std::endl;

		float noiseValueToWorldConversion = (denseGrid.bbox().max().z() - denseGrid.bbox().min().z()) / maxHeightMapValueShifted;
		for (int x = denseGrid.bbox().min().x(); x <= denseGrid.bbox().max().x(); x++)
		{
			for (int y = denseGrid.bbox().min().y(); y <= denseGrid.bbox().max().y(); y++)
			{
				float terrainHeight = noiseMap.GetValue(x, y);
				int h = int(openvdb::math::RoundDown((terrainHeight + fabs(minHeightMapValue)) * noiseValueToWorldConversion));

				float posTerrainHeight = fabs(terrainHeight);
				for (int z = denseGrid.bbox().min().z(); z < h; z++)
				{
					denseGrid.setValue(openvdb::Coord(x, y, z), -posTerrainHeight*float(h-z));
				}
				denseGrid.setValue(openvdb::Coord(x, y, h), 0.0f);
				for (int z = h+1; z <= denseGrid.bbox().max().z(); z++)
				{
					denseGrid.setValue(openvdb::Coord(x, y, z), posTerrainHeight*float(z));
				}
			}
		}

		openvdb::FloatGrid::Ptr sparseGrid = openvdb::FloatGrid::create();
		openvdb::tools::copyFromDense(denseGrid, *sparseGrid, 0.0f);
		//sparseGrid->setName(gridName);
		sparseGrid->insertMeta(openvdb::FloatGrid::META_GRID_NAME, openvdb::StringMetadata(gridName));
		sparseGrid->setGridClass(openvdb::GRID_LEVEL_SET);
		
		for (auto i = sparseGrid->beginValueAll(); i; ++i)
		{
			if (i.isVoxelValue())
			{
				if (openvdb::math::isApproxZero(i.getValue(), FLOAT_EPS))
				{
					i.setActiveState(true);
				}
				else
				{
					i.setActiveState(false);
				}
			}
		}

		//Save points that are contained within the surface set (outside, inside)
		//float outside = float(sparseGrid->voxelSize().length());
		//float inside = -float(sparseGrid->voxelSize().length());
		//openvdb::tools::doSignedFloodFill(sparseGrid->tree(), outside, inside, false, 1);
		openvdb::tools::pruneLevelSet(sparseGrid->tree());

		printGridStats(sparseGrid, true); //Print stats for voxel values
		printGridStats(sparseGrid, false); //Print stats for tile values

		grids.push_back(sparseGrid);
		openvdb::io::File file("vdbs/" + filename.str() + ".vdb");
		file.write(grids);
		file.close();
		std::cout << file.filename() << std::endl;

		openvdb::uninitialize();
	}
	catch (const std::exception& e)
	{
		OPENVDB_LOG_FATAL(e.what());
		exitStatus = EXIT_FAILURE;
	}
	catch (...)
	{
		OPENVDB_LOG_FATAL("Exception caught (unexpected type)");
		std::unexpected();
	}
	return exitStatus;
}

void usage()
{
	std::cout << "Usage:" << std::endl
		<< "\topenvdb_create gridName mapSize mapScale" << std::endl;
	exit(0);
}

const NoiseMapBounds GetNoiseHeightMapExtents()
{
	//This is the entirety of the noise map bounds
	return NoiseMapBounds { 2.0, 6.0, 1.0, 5.0 };
}

//Not sure what the scale actually means in the noise map...it's a double while height map values are floats
noise::utils::NoiseMap& CreateNoiseHeightMap(double scale, int width, int height)
{
	double baseFrequency = 2.0;
	double baseBias = 0.0;
	double perlinFrequency = 0.5;
	double perlinPersistence = 0.25;
	double edgeFalloff = 0.125;
	double finalFrequency = 4.0;
	double finalPower = 0.125;

	noise::module::Billow baseFlatTerrain; //TODO: Set as a parameter
	noise::module::RidgedMulti mountainTerrain; //TODO: Pass this as a parameter
	const NoiseMapBounds noiseMapBounds = GetNoiseHeightMapExtents();

	noise::module::ScaleBias& flatTerrain = CreateFlatTerrain(baseFlatTerrain, baseFrequency, scale, baseBias);
	noise::module::Perlin& terrainType = CreatePerlinNoise(perlinFrequency, perlinPersistence);
	noise::module::Select& terrainSelector = CreateTerrainSelector(terrainType, flatTerrain, mountainTerrain, noiseMapBounds.x0, noiseMapBounds.x1, edgeFalloff);
	noise::module::Turbulence& finalTerrain = CreateFinalTerrain(terrainSelector, finalFrequency, finalPower);

	return BuildHeightMap(finalTerrain, width, height, noiseMapBounds.x0, noiseMapBounds.x1, noiseMapBounds.y0, noiseMapBounds.y1);
}

noise::module::ScaleBias& CreateFlatTerrain(noise::module::Billow &baseFlatTerrain, double baseFrequency, double scale, double bias)
{
	static noise::module::ScaleBias FlatTerrain;
	baseFlatTerrain.SetFrequency(baseFrequency);
	FlatTerrain.SetSourceModule(0, baseFlatTerrain); //TODO: Make a vector for multiple source modules
	FlatTerrain.SetScale(scale);
	FlatTerrain.SetBias(bias);
	return FlatTerrain;
}

noise::module::Perlin& CreatePerlinNoise(double frequency, double persistence)
{
	static noise::module::Perlin TerrainType;
	TerrainType.SetFrequency(frequency);
	TerrainType.SetPersistence(persistence);
	return TerrainType;
}

noise::module::Select& CreateTerrainSelector(noise::module::Perlin &terrainType,
	noise::module::ScaleBias &flatTerrain,
	noise::module::RidgedMulti &mountainTerrain,
	double boundsX0, double boundsX1,
	double edgeFalloff)
{
	static noise::module::Select TerrainSelector;
	TerrainSelector.SetSourceModule(0, flatTerrain);
	TerrainSelector.SetSourceModule(1, mountainTerrain);
	TerrainSelector.SetControlModule(terrainType);
	TerrainSelector.SetBounds(boundsX0, boundsX1);
	TerrainSelector.SetEdgeFalloff(edgeFalloff);
	return TerrainSelector;
}

noise::module::Turbulence& CreateFinalTerrain(noise::module::Select &terrainSelector,
	double frequency,
	double power)
{
	static noise::module::Turbulence FinalTerrain;
	FinalTerrain.SetSourceModule(0, terrainSelector);
	return FinalTerrain;
}

noise::utils::NoiseMap& BuildHeightMap(noise::module::Module &finalTerrain,
	int sizeX, int sizeY,
	double boundsLowerX, double boundsUpperX, double boundsLowerZ, double boundsUpperZ)

{
	static noise::utils::NoiseMap HeightMap;
	noise::utils::NoiseMapBuilderPlane heightMapBuilder;
	heightMapBuilder.SetSourceModule(finalTerrain);
	heightMapBuilder.SetDestNoiseMap(HeightMap);
	heightMapBuilder.SetDestSize(sizeX, sizeY);
	heightMapBuilder.SetBounds(boundsLowerX, boundsUpperX, boundsLowerZ, boundsUpperZ);
	heightMapBuilder.Build();
	return HeightMap;
}

void CreatNoiseBitmap(const noise::utils::NoiseMap &heightMap, const std::string &filename)
{
	noise::utils::RendererImage renderer;
	noise::utils::Image image;
	renderer.SetSourceNoiseMap(heightMap);
	renderer.SetDestImage(image);
	renderer.ClearGradient();
	renderer.AddGradientPoint(-1.00, noise::utils::Color(32, 160, 0, 255)); // grass
	renderer.AddGradientPoint(-0.25, noise::utils::Color(224, 224, 0, 255)); // dirt
	renderer.AddGradientPoint(0.25, noise::utils::Color(128, 128, 128, 255)); // rock
	renderer.AddGradientPoint(1.00, noise::utils::Color(255, 255, 255, 255)); // snow
	renderer.EnableLight();
	renderer.SetLightContrast(3.0);
	renderer.SetLightBrightness(2.0);
	renderer.Render();

	noise::utils::WriterBMP writer;
	writer.SetSourceImage(image);
	writer.SetDestFilename(filename);
	writer.WriteDestFile();
}

void GetHeightMapExtents(const noise::utils::NoiseMap& noiseMap, float &minHeightMapValue, float &maxHeightMapValue)
{
	const int width = noiseMap.GetWidth();
	const int height = noiseMap.GetHeight();
	minHeightMapValue = FLT_MAX;
	maxHeightMapValue = FLT_MIN;
	for (int x = 0; x < width; x++)
	{
		for (int y = 0; y < height; y++)
		{
			float value = noiseMap.GetValue(x, y);
			if (value < minHeightMapValue)
			{
				minHeightMapValue = value;
			}
			if (value > maxHeightMapValue)
			{
				maxHeightMapValue = value;
			}
		}
	}
#ifdef DEBUG
	std::cout << "min noise = " << minHeightMapValue << std::endl;
	std::cout << "max noise = " << maxHeightMapValue << std::endl;
#endif
}

void printGridStats(openvdb::FloatGrid::ConstPtr grid, bool printForVoxels, float comparisonEps)
{
	int count = 0;
	int countZero = 0;
	int countNonZero = 0;
	int countOn = 0;
	int countOff = 0;

	//Collect the counts
	for (auto i = grid->beginValueAll(); i; ++i)
	{
		if (i.isVoxelValue() == printForVoxels)
		{
			count++;
			if (openvdb::math::isApproxZero(i.getValue(), comparisonEps))
			{
				countZero++;
			}
			else
			{
				countNonZero++;
			}

			if (i.isValueOn())
			{
				countOn++;
			}
			else
			{
				countOff++;
			}
		}
	}

	if (printForVoxels)
	{
		std::cout << "Voxel stats:" << std::endl;
	}
	else
	{
		std::cout << "Tile stats:" << std::endl;
	}
	std::cout << "\t" << count << " total" << std::endl;
	std::cout << "\t" << countZero << " == 0, " << countNonZero << " != 0, " << "eps " << comparisonEps << std::endl;
	std::cout << "\t" << countOn << " on, " << countOff << " off" << std::endl;
}

void makeCylinder(openvdb::FloatGrid::Ptr grid, float radius, const openvdb::CoordBBox& indexBB, double h)
{
	openvdb::FloatGrid::Accessor accessor = grid->getAccessor();

	for (openvdb::Int32 i = indexBB.min().x(); i <= indexBB.max().x(); ++i) {
		for (openvdb::Int32 j = indexBB.min().y(); j <= indexBB.max().y(); ++j) {
			for (openvdb::Int32 k = indexBB.min().z(); k <= indexBB.max().z(); ++k) {
				// transform point (i, j, k) of index space into world space
				openvdb::Vec3d p(i * h, j * h, k * h);
				// compute level set function value
				float distance = sqrt(p.x() * p.x() + p.y() * p.y()) - radius;

				accessor.setValue(openvdb::Coord(i, j, k), distance);
			}
		}
	}

	grid->setTransform(openvdb::math::Transform::createLinearTransform(h));
}

void createAndSaveCylinder()
{
	openvdb::initialize();

	openvdb::FloatGrid::Ptr grid = openvdb::FloatGrid::create(2.0);

	openvdb::CoordBBox indexBB(openvdb::Coord(-20, -20, -20), openvdb::Coord(20, 20, 20));
	makeCylinder(grid, 5.0f, indexBB, 0.5);

	// specify dataset name
	grid->setName("LevelSetCylinder");

	// save grid in the file
	openvdb::io::File file("vdbs/cylinder.vdb");
	openvdb::GridPtrVec grids;
	grids.push_back(grid);
	file.write(grids);
	file.close();
}