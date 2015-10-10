#pragma once

#include <noise.h>
#include <noiseutils.h>

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
void CreateNoiseHeightMap(noise::utils::NoiseMap &heightMap,
	double baseFrequency, double baseScale, double baseBias,
	double perlinFrequency, double perlinPersistence,
	double boundsX0, double boundsX1, double edgeFalloff,
	double finalFrequency, double finalPower,
	int sizeX, int sizeY, double boundsLowerX, double boundsUpperX, double boundsLowerZ, double boundsUpperZ);
