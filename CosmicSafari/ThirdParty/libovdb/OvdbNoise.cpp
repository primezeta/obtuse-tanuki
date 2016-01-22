#include "OvdbNoise.h"

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
void CreateNoiseBitmap(const noise::utils::NoiseMap &heightMap, const std::string &filename);

const NoiseMapBounds GetNoiseHeightMapExtents()
{
	//This is the entirety of the noise map bounds
	return NoiseMapBounds{ 2.0, 6.0, 1.0, 5.0 };
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

void CreateNoiseBitmap(const noise::utils::NoiseMap &heightMap, const std::string &filename)
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

void GetHeightMapRange(const noise::utils::NoiseMap& noiseMap, float &minHeightMapValue, float &maxHeightMapValue)
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