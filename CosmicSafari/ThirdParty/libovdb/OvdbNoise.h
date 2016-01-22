#pragma once

#pragma warning(push, 0)
#include <noise.h>
#include <noiseutils.h>
#pragma warning(pop)

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
void GetHeightMapRange(const noise::utils::NoiseMap& noiseMap, float &minHeightMapValue, float &maxHeightMapValue);
noise::utils::NoiseMap& CreateNoiseHeightMap(double scale, int width, int height);