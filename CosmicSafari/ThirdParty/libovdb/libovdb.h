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
	static Ovdb * GetIOvdbInstance(const char * vdbFilename);
};

class LIB_OVDB_API Ovdb
{
public:
	~Ovdb();
	enum OvdbMeshMethod { PRIMITIVE_CUBES, MARCHING_CUBES };
	friend Ovdb * IOvdb::GetIOvdbInstance(const char * vdbFilename);
	int DefineGrid(const char * const gridName, double sx, double sy, double sz, int x0, int y0, int z0, int x1, int y1, int z1);
	int DefineRegion(const char * const gridName, const char * const regionName, int x0, int y0, int z0, int x1, int y1, int z1, char * regionIDStr, size_t maxStrLen);
	int RemoveRegion(const char * const gridName, const char * const regionName);
	int ReadRegionBounds(const char * const gridName, const char * const regionName, int &x0, int &y0, int &z0, int &x1, int &y1, int &z1);
	size_t ReadMetaRegionCount();
	int WriteChanges();
	int LoadRegion(const char * const regionID);
	int ReadMetaRegionIDs(char ** regionIDList, size_t regionCount, size_t strMaxLen);	
	int MeshRegion(const char * const regionID, float surfaceValue);
	size_t VertexCount(const char * const regionID);
	size_t PolygonCount(const char * const regionID);
	size_t NormalCount(const char * const regionID);
	bool YieldVertex(const char * const regionID, double &vx, double &vy, double &vz);
	bool YieldPolygon(const char * const regionID, uint32_t &i1, uint32_t &i2, uint32_t &i3);
	bool YieldNormal(const char * const regionID, double &nx, double &ny, double &nz);
	int PopulateRegionDensityPerlin(const char * const regionID, double frequency, double lacunarity, double persistence, int octaveCount);

private:
	//Private constructor handles initialization of file handling via the singleton interface
	Ovdb(const char * vdbFilename);
};