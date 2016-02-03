#pragma once
#include <string>
#include <stdint.h>
enum OvdbMeshMethod { MESHING_NAIVE, MESHING_GREEDY };
typedef struct _VolumeDimensions_
{
	int32_t x0, x1;
	int32_t y0, y1;
	int32_t z0, z1;
	_VolumeDimensions_(int32_t _x0, int32_t _x1, int32_t _y0, int32_t _y1, int32_t _z0, int32_t _z1)
		: x0(_x0), x1(_x1), y0(_y0), y1(_y1), z0(_z0), z1(_z1) {}
	int32_t _VolumeDimensions_::sizeX() const { return abs(x1 - x0) + 1; }
	int32_t _VolumeDimensions_::sizeY() const { return abs(y1 - y0) + 1; }
	int32_t _VolumeDimensions_::sizeZ() const { return abs(z1 - z0) + 1; }
} VolumeDimensions;

typedef float GridType;
typedef std::wstring IDType;
const static IDType INVALID_GRID_ID = std::wstring();