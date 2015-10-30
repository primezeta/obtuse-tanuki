#pragma once

#include <noise.h>
#include <noiseutils.h>

//typedef bool DenseType;
//typedef openvdb::BoolGrid VoxelGridType;

typedef struct _noisemapbounds
{
	double x0;
	double x1;
	double y0;
	double y1;
	double max;
} NoiseMapBounds;

typedef struct _terraindata
{
	std::string tileName;
	openvdb::CoordBBox worldBounds;
	NoiseMapBounds noiseMapBounds;
	noise::utils::NoiseMap heightMap;
} TerrainData;

const NoiseMapBounds GetNoiseHeightMapExtents();
void CreateNoiseHeightMap(TerrainData &terrainData, double scale, int width, int height);
void CreateFlatTerrain(noise::module::ScaleBias &flatTerrain, noise::module::Billow &baseFlatTerrain, double baseFrequency, double scale, double bias);
void CreatePerlinNoise(noise::module::Perlin &terrainType, double frequency, double persistence);
void CreateTerrainSelector(noise::module::Select &terrainSelector,
	noise::module::Perlin &terrainType,
	noise::module::ScaleBias &flatTerrain,
	noise::module::RidgedMulti &mountainTerrain,
	double boundsX0, double boundsX1,
	double edgeFalloff);
void CreateFinalTerrain(noise::module::Module &finalTerrain,
	noise::module::Select &terrainSelector,
	double frequency,
	double power);
void BuildHeightMap(noise::utils::NoiseMap &heightMap,
	noise::module::Module &finalTerrain,
	int sizeX, int sizeY,
	double boundsLowerX, double boundsUpperX, double boundsLowerZ, double boundsUpperZ);
void CreatNoiseBitmap(const noise::utils::NoiseMap &heightMap, const std::string &filename);