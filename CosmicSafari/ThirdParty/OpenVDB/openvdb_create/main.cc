// openvdb_create.cpp : Defines the entry point for the console application.
//

#include <string>
#include <openvdb/openvdb.h>
#include <openvdb/Exceptions.h>
#include <openvdb/tools/LevelSetSphere.h>
#include <openvdb/tools/Dense.h>
#include <openvdb/tools/MeshToVolume.h>
#include "openvdbnoise.h"

int main(int argc, char * argv[])
{
	int exitStatus = EXIT_SUCCESS;
	try
	{
		openvdb::initialize();

		std::string gridName = std::string(argv[argc - 1]);
		std::string gridType = argv[1];
		openvdb::FloatGrid::Ptr sparseGrid = openvdb::FloatGrid::create();
		sparseGrid->setName(gridName);

		std::ostringstream filename;
		if (gridType == "dense")
		{
			openvdb::Vec3I bounds(std::stoi(argv[2]), std::stoi(argv[3]), std::stoi(argv[4]));
			float fillValue = std::stof(argv[5]);
			openvdb::FloatGrid::ValueType tolerance = std::stof(argv[6]);
			filename << gridType << "_x" << bounds.x() << "_y" << bounds.y() << "_z" << bounds.z() << "_f" << fillValue << "_t" << tolerance;

			openvdb::CoordBBox boundingBox(openvdb::Coord(0, 0, 0), openvdb::Coord(bounds.x(), bounds.y(), bounds.z()));
			openvdb::tools::Dense<float> denseGrid(boundingBox);
			denseGrid.fill(fillValue);
			openvdb::tools::copyFromDense(denseGrid, *sparseGrid, tolerance);
		}
		else if (gridType == "sphere")
		{
			openvdb::Vec3f center(std::stof(argv[2]), std::stof(argv[3]), std::stof(argv[4]));
			float radius = std::stof(argv[5]);
			float voxelSize = std::stof(argv[6]);
			float levelSetHalfWidth = (float)openvdb::LEVEL_SET_HALF_WIDTH;
			if (argc >= 8)
			{
				levelSetHalfWidth = std::stof(argv[7]);
			}
			filename << gridType << "_x" << center.x() << "_y" << center.y() << "_z" << center.z() << "_r" << radius << "_v" << voxelSize << "_h" << levelSetHalfWidth;
			sparseGrid = openvdb::tools::createLevelSetSphere<openvdb::FloatGrid>(radius, center, voxelSize, levelSetHalfWidth);
		}
		else if (gridType == "noise")
		{
			int mapWidth = std::stoi(argv[2]);
			int mapHeight = std::stoi(argv[3]);
			int mapLength = std::stoi(argv[4]);
			float mapScale = std::stof(argv[5]);
			float tolerance = std::stof(argv[6]);
			filename << gridType << "_w" << mapWidth << "_h" << mapHeight << "_l" << mapLength << "_s" << mapScale << "_t" << tolerance;

			openvdb::CoordBBox boundingBox(openvdb::Coord(0, 0, 0), openvdb::Coord(mapWidth, mapHeight, mapLength));
			openvdb::tools::Dense<float> denseGrid(boundingBox);

			noise::utils::NoiseMap heightMap;
			CreateNoiseHeightMap(heightMap, (double)mapScale, mapWidth, mapHeight);

			int heightMapWidth = heightMap.GetWidth();
			int heightMapHeight = heightMap.GetHeight();

			//Find the largest height value to determine the height of the world space
			float maxValue = FLT_MIN;
			float minValue = FLT_MAX;
			for (int i = 0; i < heightMapWidth; i++)
			{
				for (int j = 0; j < heightMapHeight; j++)
				{
					float x = heightMap.GetValue(i, j);
					if (x > maxValue)
					{
						maxValue = x;
					}
					if (x < minValue)
					{
						minValue = x;
					}
				}
			}
			float heightSpan = maxValue + abs(minValue);
			int voxelCount = mapLength / openvdb::LEVEL_SET_HALF_WIDTH * 2;
			float voxelUnit = openvdb::LEVEL_SET_HALF_WIDTH * 2 * heightSpan / voxelCount;
			printf("map (w,h) = (%d,%d), height span = %f, vertical voxel count = %d, voxel unit = %f\n", heightMapWidth, heightMapHeight, heightSpan, voxelCount, voxelUnit);

			//Grab values from the height map and build a dense grid
			for (int i = 0; i < mapWidth; i++)
			{
				for (int j = 0; j < mapHeight; j++)
				{
					float heightValue = heightMap.GetValue(i, j) + abs(minValue);
					int voxelIndex = voxelCount*(heightValue / heightSpan);
					float voxelPos = heightValue * voxelUnit;
					denseGrid.setValue(openvdb::Coord(i, j, voxelIndex), voxelPos);
					for (int k = 0; k < voxelIndex; k++)
					{
						//Set these voxels to not be visible
						denseGrid.setValue(openvdb::Coord(i, j, k), tolerance);
					}
				}
			}
			printf("voxel count = %d, grid width = %d, grid height = %d, grid length = %d\n", denseGrid.valueCount(), denseGrid.xStride(), denseGrid.yStride(), denseGrid.zStride());
			openvdb::tools::copyFromDense(denseGrid, *sparseGrid, tolerance);
		}

		openvdb::GridPtrVec grids;
		grids.push_back(sparseGrid);
		openvdb::io::File file("vdbs/" + filename.str() + ".vdb");
		file.write(grids);
		file.close();
		printf("Created %s\n", file.filename().c_str());

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

//Not sure what the scale actually means in the noise map...it's a double while height map values are floats
void CreateNoiseHeightMap(noise::utils::NoiseMap &heightMap, double scale, int width, int height)
{
	double baseFrequency = 2.0;
	double baseBias = 1.0;
	double perlinFrequency = 0.5;
	double perlinPersistence = 0.25;
	double edgeFalloff = 0.125;
	double finalFrequency = 4.0;
	double finalPower = 0.125;
	double boundsX0 = 2.0;
	double boundsX1 = 6.0;
	double boundsY0 = 1.0;
	double boundsY1 = 5.0;

	noise::module::Billow baseFlatTerrain; //TODO: Set as a parameter
	noise::module::ScaleBias flatTerrain;
	CreateFlatTerrain(flatTerrain, baseFlatTerrain, baseFrequency, scale, baseBias);

	noise::module::Perlin terrainType;
	CreatePerlinNoise(terrainType, perlinFrequency, perlinPersistence);

	noise::module::Select terrainSelector;
	noise::module::RidgedMulti mountainTerrain; //TODO: Pass this as a parameter
	CreateTerrainSelector(terrainSelector, terrainType, flatTerrain, mountainTerrain, boundsX0, boundsX1, edgeFalloff);

	noise::module::Turbulence finalTerrain;
	CreateFinalTerrain(finalTerrain, terrainSelector, finalFrequency, finalPower);

	BuildHeightMap(heightMap, finalTerrain, width, height, boundsX0, boundsX1, boundsY0, boundsY1);
}

void CreateFlatTerrain(noise::module::ScaleBias &flatTerrain, noise::module::Billow &baseFlatTerrain, double baseFrequency, double scale, double bias)
{
	baseFlatTerrain.SetFrequency(baseFrequency);
	flatTerrain.SetSourceModule(0, baseFlatTerrain); //TODO: Make a vector for multiple source modules
	flatTerrain.SetScale(scale);
	flatTerrain.SetBias(bias);
}

void CreatePerlinNoise(noise::module::Perlin &terrainType, double frequency, double persistence)
{
	terrainType.SetFrequency(frequency);
	terrainType.SetPersistence(persistence);
}

void CreateTerrainSelector(noise::module::Select &terrainSelector,
	noise::module::Perlin &terrainType,
	noise::module::ScaleBias &flatTerrain,
	noise::module::RidgedMulti &mountainTerrain,
	double boundsX0, double boundsX1,
	double edgeFalloff)
{
	terrainSelector.SetSourceModule(0, flatTerrain);
	terrainSelector.SetSourceModule(1, mountainTerrain);
	terrainSelector.SetControlModule(terrainType);
	terrainSelector.SetBounds(boundsX0, boundsX1);
	terrainSelector.SetEdgeFalloff(edgeFalloff);
}

void CreateFinalTerrain(noise::module::Module &finalTerrain,
	noise::module::Select &terrainSelector,
	double frequency,
	double power)
{
	finalTerrain.SetSourceModule(0, terrainSelector);
}

void BuildHeightMap(noise::utils::NoiseMap &heightMap,
	noise::module::Module &finalTerrain,
	int sizeX, int sizeY,
	double boundsLowerX, double boundsUpperX, double boundsLowerZ, double boundsUpperZ)

{
	noise::utils::NoiseMapBuilderPlane heightMapBuilder;
	heightMapBuilder.SetSourceModule(finalTerrain);
	heightMapBuilder.SetDestNoiseMap(heightMap);
	heightMapBuilder.SetDestSize(sizeX, sizeY);
	heightMapBuilder.SetBounds(boundsLowerX, boundsUpperX, boundsLowerZ, boundsUpperZ);
	heightMapBuilder.Build();
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