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

//TODO: Restructure header files
#include <string>
#include <vector>
#include <stdint.h>
#include "OpenVDBIncludes.h"

enum OvdbMeshMethod { MESHING_NAIVE, MESHING_GREEDY };

LIB_OVDB_API int OvdbInitialize();
LIB_OVDB_API int OvdbUninitialize();
LIB_OVDB_API int OvdbReadVdb(const std::string &filename, const std::string &gridName, uint32_t &gridID);
LIB_OVDB_API int OvdbWriteVdbGrid(const std::string &filename, uint32_t gridID);
LIB_OVDB_API int OvdbVolumeToMesh(OvdbMeshMethod meshMethod, float surfaceValue, const std::string &filename, const std::string &gridName, uint32_t &gridID);
LIB_OVDB_API int OvdbVolumeToMesh(uint32_t gridID, OvdbMeshMethod meshMethod, float surfaceValue);
LIB_OVDB_API int OvdbYieldNextMeshPoint(uint32_t gridID, float &vx, float &vy, float &vz);
LIB_OVDB_API int OvdbYieldNextMeshPolygon(uint32_t gridID, uint32_t &i1, uint32_t &i2, uint32_t &i3);
LIB_OVDB_API int OvdbYieldNextMeshNormal(uint32_t gridID, float &nx, float &ny, float &nz);
LIB_OVDB_API int OvdbCreateLibNoiseVolume(const std::string &gridName, float surfaceValue, uint32_t dimX, uint32_t dimY, uint32_t dimZ, uint32_t &gridID, float &isovalue);