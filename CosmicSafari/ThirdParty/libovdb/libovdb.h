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

	IOvdb();
	~IOvdb();
	void InitializeGrid(const wchar_t * const gridID);
	int MaskRegions(const wchar_t * const gridID, int32_t regionCountX, int32_t regionCountY, int32_t regionCountZ);
	int ReadGrid(const wchar_t * const gridID, const wchar_t * const filename);
	int WriteGrid(const wchar_t * const gridID, const wchar_t * const filename);
	int GridToMesh(const wchar_t * const gridID, OvdbMeshMethod meshMethod, float surfaceValue);
	int RegionToMesh(const wchar_t * const gridID, const wchar_t * const meshID, OvdbMeshMethod meshMethod, float surfaceValue);
	int YieldVertex(const wchar_t * const gridID, const wchar_t * const meshID, float &vx, float &vy, float &vz);
	int YieldPolygon(const wchar_t * const gridID, const wchar_t * const meshID, uint32_t &i1, uint32_t &i2, uint32_t &i3);
	int YieldNormal(const wchar_t * const gridID, const wchar_t * const meshID, float &nx, float &ny, float &nz);
	int CreateLibNoiseGrid(const wchar_t * const gridID, int sizeX, int sizeY, int sizeZ, float surfaceValue, double scaleXYZ, double frequency, double lacunarity, double persistence, int octaveCount);

private:
};

LIB_OVDB_API IOvdb * GetIOvdbInstance();