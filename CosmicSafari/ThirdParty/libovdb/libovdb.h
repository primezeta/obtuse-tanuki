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

class Ovdb; //forward declaration

class LIB_OVDB_API IOvdb
{
public:
	static Ovdb * GetIOvdbInstance(const char * vdbFilename, const char * const gridName);
};

class LIB_OVDB_API Ovdb
{
public:
	~Ovdb();
	enum OvdbMeshMethod { PRIMITIVE_CUBES, MARCHING_CUBES };
	friend Ovdb * IOvdb::GetIOvdbInstance(const char * vdbFilename, const char * const gridName);
	int DefineRegion(const char * const gridName, const char * const regionName, int x0, int y0, int z0, int x1, int y1, int z1, bool commitChanges = false);
	size_t ReadMetaRegionCount(const char * const gridName = nullptr);
	int ReadMetaRegionIDs(char ** regionIDList, size_t regionCount, size_t strMaxLen);
	int ReadRegion(const char * const regionID, int &x0, int &y0, int &z0, int &x1, int &y1, int &z1);
	int LoadRegion(const char * const regionID);
	int MeshRegion(const char * const regionID, float surfaceValue);
	bool YieldVertex(const char * const regionID, double &vx, double &vy, double &vz);
	bool YieldPolygon(const char * const regionID, uint32_t &i1, uint32_t &i2, uint32_t &i3);
	bool YieldNormal(const char * const regionID, double &nx, double &ny, double &nz);
	int PopulateRegionDensityPerlin(const char * const regionID, double scaleXYZ, double frequency, double lacunarity, double persistence, int octaveCount);

private:
	//Private constructor handles initialization of file handling via the singleton interface
	Ovdb(const char * vdbFilename, const char * gridName);
};