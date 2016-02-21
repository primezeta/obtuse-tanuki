#pragma once
#include "OvdbTypes.h"

#pragma warning(push, 0)
#include <noise.h>
#include <noiseutils.h>
#pragma warning(pop)

namespace ovdb
{
	namespace tools
	{
		typedef struct _noisemapbounds
		{
			double x0;
			double x1;
			double y0;
			double y1;
		} NoiseMapBounds;

		void GetHeightMapRange(const noise::utils::NoiseMap& noiseMap, float &minHeightMapValue, float &maxHeightMapValue);
		noise::utils::NoiseMap& CreateNoiseHeightMap(double scale, int width, int height, float &minValue, float &maxValue);
		noise::module::Perlin& CreatePerlinNoise(double frequency, double persistence, int octaveCount);
	}
}