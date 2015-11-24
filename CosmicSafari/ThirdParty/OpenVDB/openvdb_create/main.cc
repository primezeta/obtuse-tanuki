// openvdb_create.cpp : Defines the entry point for the console application.
//

#include <string>
#include <openvdb/openvdb.h>
#include <openvdb/Exceptions.h>
#include <openvdb/tools/LevelSetSphere.h>
#include <openvdb/tools/Dense.h>
#include <openvdb/tools/MeshToVolume.h>
#include <openvdb/tools/VolumeToMesh.h>
#include "openvdbnoise.h"
#include "../libovdb/libovdb.h"

typedef float NoiseTreeDataType;
typedef openvdb::FloatGrid NoiseGridDataType;
const NoiseTreeDataType fillValue = false;

void usage();

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
		if (argc != 6)
		{
			usage();
		}

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
			std::cout << "Tile count of " << tileCount << " does not evenly divide map width of " << mapWidth << std::endl;
			return 1;
		}			
		filename << gridName << "_w" << mapWidth << "_h" << mapHeight << "_l" << mapLength << "_t" << tileCount << "_s" << mapScale << "_t" << tolerance;
			
		//Build the bounding box of each tile
		int tileSideLength = mapWidth / tileCount;
		double noiseWidth = GetNoiseHeightMapExtents().x1 - GetNoiseHeightMapExtents().x0;
		double noiseHeight = GetNoiseHeightMapExtents().y1 - GetNoiseHeightMapExtents().y0;
		double noiseTileWidth = noiseWidth / tileCount;
		double noiseTileHeight = noiseHeight / tileCount;
		float maxHeightMapValue = FLT_MIN;
		float minHeightMapValue = FLT_MAX;

#ifdef _DEBUG
		std::cout << "Tile side length is " << tileSideLength << " with map width " << mapWidth << " and tile count " << tileCount << std::endl;
		std::cout << "Noise map bounds are x[" << GetNoiseHeightMapExtents().x0 << "," << GetNoiseHeightMapExtents().x1 << "] by "
				    << "y[" << GetNoiseHeightMapExtents().y0 << ", " << GetNoiseHeightMapExtents().y1 << "]" << std::endl;
		std::cout << "Noise map width is " << noiseWidth << " and height " << noiseHeight << std::endl << std::endl;
#endif
		std::vector<TerrainData> terrainTiles;
		for (int x = 0; x < mapWidth; x += tileSideLength)
		{
			TerrainData terrainData;
			terrainData.noiseMapBounds.x0 = GetNoiseHeightMapExtents().x0 + double((x / tileSideLength))*noiseTileWidth;
			terrainData.noiseMapBounds.x1 = terrainData.noiseMapBounds.x0 + noiseTileWidth;
			std::ostringstream logstrX;
			logstrX << "\tnoise map x bounds(" << terrainData.noiseMapBounds.x0 << "," << terrainData.noiseMapBounds.x1 << ")" << std::endl;

			for (int y = 0; y < mapHeight; y += tileSideLength)
			{
				terrainData.noiseMapBounds.y0 = GetNoiseHeightMapExtents().y0 + double((y / tileSideLength))*noiseTileHeight;
				terrainData.noiseMapBounds.y1 = terrainData.noiseMapBounds.y0 + noiseTileHeight;
				std::ostringstream logstrY;
				logstrY << "\tnoise map y bounds(" << terrainData.noiseMapBounds.y0 << "," << terrainData.noiseMapBounds.y1 << ")" << std::endl;

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

				std::ostringstream name;
				name << "tile[" << x << ", " << y << "]";
				terrainData.tileName = name.str();
				terrainTiles.push_back(terrainData);

#ifdef _DEBUG
				logstrY << "\ttile name = " << name.str() << std::endl;
				std::cout << "processing map(" << x << "," << y << ")" << std::endl << logstrX.str() << logstrY.str() << std::endl;
#endif
			}
		}
			
		//Create a dense grid from the bounding boxes of each tile
		float heightMapTotalHeight = maxHeightMapValue + abs(minHeightMapValue);
		float voxelUnitConversion = float(tileSideLength) / heightMapTotalHeight;
#ifdef _DEBUG
		std::cout << "noise map max = " << maxHeightMapValue << std::endl
				  << "noise map min = " << minHeightMapValue << std::endl
				  << "noise map total height = " << heightMapTotalHeight << std::endl
				  << "noise map unit conversion = " << voxelUnitConversion << std::endl;
#endif

		NoiseGridDataType::Ptr sparseGrid = NoiseGridDataType::create();
		sparseGrid->setName(gridName);
		sparseGrid->setGridClass(openvdb::GRID_LEVEL_SET);

		for (std::vector<TerrainData>::const_iterator i = terrainTiles.begin(); i < terrainTiles.end(); i++)
		{
			//Grab values from the height map and build a dense grid
			int heightMapWidth = i->heightMap.GetWidth();
			int heightMapHeight = i->heightMap.GetHeight();
			openvdb::tools::Dense<NoiseTreeDataType> denseGrid(i->worldBounds);
#ifdef _DEBUG
			std::cout << "world bounds" << std::endl
				      << "\t" << i->worldBounds.getStart().x() << "," << i->worldBounds.getStart().y() << "," << i->worldBounds.getStart().z() << std::endl
					  << "\t" << i->worldBounds.getEnd().x() << "," << i->worldBounds.getEnd().y() << "," << i->worldBounds.getEnd().z() << std::endl;
#endif

			openvdb::Coord initialCoord(i->worldBounds.getStart().x(), i->worldBounds.getStart().y(), i->worldBounds.getStart().z());
			NoiseTreeDataType tileValue = 0.0f;
			sparseGrid->treePtr()->addTile(NoiseGridDataType::TreeType::RootNodeType::LEVEL+1, initialCoord, tileValue, true);			
			denseGrid.fill(fillValue);
				
			for (int w = 0; w < heightMapWidth; w++)
			{
				for (int h = 0; h < heightMapHeight; h++)
				{
					float heightValue = i->heightMap.GetValue(w, h) + abs(minHeightMapValue);
					float voxelPos = heightValue * voxelUnitConversion;
					int voxelIndex = openvdb::math::Floor(voxelPos);
					openvdb::Coord denseCoord(i->worldBounds.min().x() + w, i->worldBounds.min().y() + h, voxelIndex);

					//Set the location of the height map value to true to make a boundary						
					//denseGrid.setValue(denseCoord, true);
					//Set locations below the height map value to negative, proportionally decreasing
					for (int z = -(voxelIndex+1); z < 0; z++)
					{
						denseGrid.setValue(denseCoord, z * voxelPos / voxelIndex);
					}
					//Set locations below the height map value to positive, proportionally decreasing
					for (int z = i->worldBounds.max().z(); z > 0; z--)
					{
						denseGrid.setValue(denseCoord, z * voxelPos / voxelIndex);
					}
				}
			}
#ifdef _DEBUG
			std::cout << "grid stride" << std::endl					      
					    << "\t" << denseGrid.xStride() << "," << denseGrid.yStride() << "," << denseGrid.zStride() << std::endl;
#endif
			//TODO: Investigate using these functions:
			//	sparseGrid->worldToIndex()
			//	sparseGrid->voxelSize()
			openvdb::tools::copyFromDense(denseGrid, *sparseGrid, tolerance);
		}

		grids.push_back(sparseGrid);
		openvdb::io::File file("vdbs/" + filename.str() + ".vdb");
		file.write(grids);
		file.close();
		std::cout << "Created " << file.filename() << std::endl;

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
		<< "\topenvdb_create gridName mapWidth tileCount mapScale tolerance" << std::endl;
	exit(0);
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
	double baseBias = 0.0;
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