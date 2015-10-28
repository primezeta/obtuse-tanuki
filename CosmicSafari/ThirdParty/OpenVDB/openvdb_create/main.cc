// openvdb_create.cpp : Defines the entry point for the console application.
//

#include <string>
#include <openvdb/openvdb.h>
#include <openvdb/Exceptions.h>
#include <openvdb/tools/LevelSetSphere.h>
#include <openvdb/tools/Dense.h>
#include <openvdb/tools/MeshToVolume.h>
#include "openvdbnoise.h"

void usage();

int main(int argc, char * argv[])
{
	int exitStatus = EXIT_SUCCESS;
	try
	{
		openvdb::initialize();

		std::string gridName = std::string(argv[argc - 1]);
		std::string gridType = argv[1];
		if (gridType != "dense" &&
			gridType != "sphere" &&
			gridType != "noise")
		{
			usage();
		}

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
			int tileCount = std::stoi(argv[3]);
			float mapScale = std::stof(argv[4]);
			float tolerance = std::stof(argv[5]);

			//int mapHeight = std::stoi(argv[3]);
			//int mapLength = std::stoi(argv[4]);
			//Assume the world is a cube
			int mapHeight = mapWidth;
			int mapLength = mapWidth;

			//Ensure the tile count evenly divides the world
			if (mapWidth % tileCount != 0)
			{
				std::cout << "Tile count of " << tileCount << "does not evenly divide map width of " << mapWidth << std::endl;
				return 1;
			}			
			filename << gridType << "_w" << mapWidth << "_h" << mapHeight << "_l" << mapLength << "_t" << tileCount << "_s" << mapScale << "_t" << tolerance;
			
			//Build the bounding box of each tile
			std::vector<TerrainData> terrainTiles;
			int tileSideLength = mapWidth / tileCount;
			double noiseWidth = GetNoiseHeightMapExtents().x1 - GetNoiseHeightMapExtents().x0;
			double noiseHeight = GetNoiseHeightMapExtents().y1 - GetNoiseHeightMapExtents().y0;
			float maxHeightMapValue = FLT_MIN;
			float minHeightMapValue = FLT_MAX;

			for (int i = 0; i < tileCount; i++)
			{
				TerrainData terrainData;
				for (int x = 0; x < mapWidth; x += tileSideLength)
				{
					terrainData.noiseMapBounds.x0 = (double(x)*noiseWidth) / double(tileCount);
					terrainData.noiseMapBounds.x1 = terrainData.noiseMapBounds.x0 + noiseWidth;
					for (int y = 0; y < mapHeight; y += tileSideLength)
					{
						terrainData.noiseMapBounds.y0 = (double(y)*noiseHeight) / double(tileCount);
						terrainData.noiseMapBounds.y1 = terrainData.noiseMapBounds.y0 + noiseHeight;

						//The z value will always be from 0 to max height
						int z0 = 0;
						int z1 = z0 + tileSideLength;
						openvdb::Coord lower(x, y, z0);
						openvdb::Coord upper(x + tileSideLength, y + tileSideLength, z1);
						terrainData.worldBounds = openvdb::CoordBBox(lower, upper);

						//Build the height map with these bounding boxes
						//TODO: Review libnoise to ensure that it is desirable to set the noise map size to the map size.
						//The actual perlin noise currently spans a plane hardcoded as (x0,x1)=(2.0,6.0) and (y0,y1)=(1.0,5.0)
						//The noise map is set here as the size of the world. Which I believe means libnoise interpolates
						//values from the perlin noise to span the height map size. Should determine if this is a true assumption.
						CreateNoiseHeightMap(terrainData, (double)mapScale, tileSideLength, tileSideLength);

						for (int w = 0; w < terrainData.heightMap.GetWidth(); w++)
						{
							for (int h = 0; h < terrainData.heightMap.GetHeight(); h++)
							{
								float value = terrainData.heightMap.GetValue(w, h);
								if (value > maxHeightMapValue)
								{
									maxHeightMapValue = value;
								}
								if (value < minHeightMapValue)
								{
									minHeightMapValue = value;
								}
							}
						}

						std::ostringstream tileInfo;
						tileInfo << "tile[" << x << "," << y << "]";
						terrainData.tileName = tileInfo.str();
						terrainTiles.push_back(terrainData);
					}
				}
			}

			//Create a dense grid from the bounding boxes of each tile
			float tileLength = maxHeightMapValue + abs(minHeightMapValue);
			float tileVoxelCount = float(mapLength) / float(openvdb::LEVEL_SET_HALF_WIDTH * 2);
			for (std::vector<TerrainData>::const_iterator i = terrainTiles.begin(); i < terrainTiles.end(); i++)
			{
				int tileWidth = i->heightMap.GetWidth();
				int tileHeight = i->heightMap.GetHeight();
				float tileVoxelUnit = float(openvdb::LEVEL_SET_HALF_WIDTH) * 2.0f * tileLength / tileVoxelCount;
				std::cout << "tile (w,h) = (" << tileWidth << "," << tileHeight
					<< "), height span = " << tileLength
					<< ", vertical voxel count = " << tileVoxelCount
					<< ", voxel unit = " << tileVoxelUnit << std::endl;

				//Grab values from the height map and build a dense grid
				openvdb::tools::Dense<float> denseGrid(i->worldBounds);
				for (int w = 0; w < tileWidth; w++)
				{
					for (int h = 0; h < tileHeight; h++)
					{
						float lengthValue = i->heightMap.GetValue(w, h) + abs(minHeightMapValue);
						float voxelPos = lengthValue * tileVoxelUnit;
						int voxelIndex = int(tileVoxelCount*(lengthValue / tileLength));

						denseGrid.setValue(openvdb::Coord(w, h, voxelIndex), voxelPos);
						for (int k = 0; k < voxelIndex; k++)
						{
							//Set these voxels to not be visible
							//TODO: This probably is not working like I assume it is
							denseGrid.setValue(openvdb::Coord(w, h, k), 0.0f);
						}
					}
				}
				std::cout << "voxel count = " << denseGrid.valueCount()
					<< ", grid width = " << denseGrid.xStride()
					<< ", grid height = " << denseGrid.yStride()
					<< ", grid length = " << denseGrid.zStride() << std::endl;
				//sparseGrid->treePtr()->addTile()
				//openvdb::tools::copyFromDense(denseGrid, *sparseGrid, tolerance);
			}
		}

		//openvdb::GridPtrVec grids;
		//grids.push_back(sparseGrid);
		//openvdb::io::File file("vdbs/" + filename.str() + ".vdb");
		//file.write(grids);
		//file.close();
		//std::cout << "Created " << file.filename() << std::endl;

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
		<< "\topenvdb_create dense boundsX boundsY boundsZ fillValue tolerance" << std::endl
		<< "\topenvdb_create sphere centerX centerY centerZ radius voxelSize [levelSetHalfWidth=3.0]" << std::endl
		<< "\topenvdb_create noise mapWidth tileCount mapScale tolerance" << std::endl
		<< std::endl;
}

const NoiseMapBounds GetNoiseHeightMapExtents()
{
	//This is the entirety of the noise map bounds
	return NoiseMapBounds { 2.0, 6.0, 1.0, 5.0 };
}

//Not sure what the scale actually means in the noise map...it's a double while height map values are floats
void CreateNoiseHeightMap(TerrainData &terrainData, double scale, int width, int height)
{
	double baseFrequency = 2.0;
	double baseBias = 1.0;
	double perlinFrequency = 0.5;
	double perlinPersistence = 0.25;
	double edgeFalloff = 0.125;
	double finalFrequency = 4.0;
	double finalPower = 0.125;

	noise::module::Billow baseFlatTerrain; //TODO: Set as a parameter
	noise::module::ScaleBias flatTerrain;
	CreateFlatTerrain(flatTerrain, baseFlatTerrain, baseFrequency, scale, baseBias);

	noise::module::Perlin terrainType;
	CreatePerlinNoise(terrainType, perlinFrequency, perlinPersistence);

	noise::module::Select terrainSelector;
	noise::module::RidgedMulti mountainTerrain; //TODO: Pass this as a parameter
	CreateTerrainSelector(terrainSelector, terrainType, flatTerrain, mountainTerrain, terrainData.noiseMapBounds.x0, terrainData.noiseMapBounds.x1, edgeFalloff);

	noise::module::Turbulence finalTerrain;
	CreateFinalTerrain(finalTerrain, terrainSelector, finalFrequency, finalPower);

	BuildHeightMap(terrainData.heightMap, finalTerrain, width, height, terrainData.noiseMapBounds.x0, terrainData.noiseMapBounds.x1, terrainData.noiseMapBounds.y0, terrainData.noiseMapBounds.y1);
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