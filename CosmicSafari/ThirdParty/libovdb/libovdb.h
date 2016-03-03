#pragma once
#ifdef LIB_OVDB_DLL
	#ifdef LIB_OVDB_EXPORT
		#define LIB_OVDB_API __declspec(dllexport)
	#else
		#define LIB_OVDB_API __declspec(dllimport)
	#endif
#else
	#define LIB_OVDB_API
#endif
#include <string>
#include <stdint.h>
#include <vector>

class LIB_OVDB_API IOvdb
{
public:
	enum OvdbMeshMethod { PRIMITIVE_CUBES, MARCHING_CUBES };	

	IOvdb(const char * vdbFilename);
	~IOvdb();
	int InitializeGrid(const char * const gridName);
	int DefineRegion(int x0, int y0, int z0, int x1, int y1, int z1, char * regionStr, size_t regionStrSize);
	int LoadRegion(const char * const regionName);
	int MeshRegion(const char * const regionName, float surfaceValue);
	bool YieldVertex(const char * const regionName, double &vx, double &vy, double &vz);
	bool YieldPolygon(const char * const regionName, uint32_t &i1, uint32_t &i2, uint32_t &i3);
	bool YieldNormal(const char * const regionName, double &nx, double &ny, double &nz);
	int PopulateRegionDensityPerlin(const char * const regionName, double scaleXYZ, double frequency, double lacunarity, double persistence, int octaveCount);
};

LIB_OVDB_API IOvdb * GetIOvdbInstance(const char * vdbFilename);