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
} NoiseMapBounds;

const NoiseMapBounds GetNoiseHeightMapExtents();
noise::utils::NoiseMap& CreateNoiseHeightMap(double scale, int width, int height);
noise::module::ScaleBias& CreateFlatTerrain(noise::module::Billow &baseFlatTerrain, double baseFrequency, double scale, double bias);
noise::module::Perlin& CreatePerlinNoise(double frequency, double persistence);
noise::module::Select& CreateTerrainSelector(noise::module::Perlin &terrainType,
	noise::module::ScaleBias &flatTerrain,
	noise::module::RidgedMulti &mountainTerrain,
	double boundsX0, double boundsX1,
	double edgeFalloff);
noise::module::Turbulence& CreateFinalTerrain(noise::module::Select &terrainSelector,
	double frequency,
	double power);
noise::utils::NoiseMap& BuildHeightMap(noise::module::Module &finalTerrain,
	int sizeX, int sizeY,
	double boundsLowerX, double boundsUpperX, double boundsLowerZ, double boundsUpperZ);
void CreatNoiseBitmap(const noise::utils::NoiseMap &heightMap, const std::string &filename);
void GetHeightMapExtents(const noise::utils::NoiseMap& noiseMap, float &minHeightMapValue, float &maxHeightMapValue);