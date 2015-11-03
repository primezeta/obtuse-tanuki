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
#include <vector>
#include <stdint.h>

LIB_OVDB_API int OvdbInitialize();
LIB_OVDB_API int OvdbUninitialize();
LIB_OVDB_API int OvdbLoadVdb(const std::string &filename);
LIB_OVDB_API int OvdbVolumeToMesh(double isovalue = 0.0, double adaptivity = 0.0);
LIB_OVDB_API int OvdbGetNextMeshPoint(float &px, float &py, float &pz);
LIB_OVDB_API int OvdbGetNextMeshTriangle(uint32_t &tx, uint32_t &ty, uint32_t &tz);
LIB_OVDB_API int OvdbGetNextMeshQuad(uint32_t &qw, uint32_t &qx, uint32_t &qy, uint32_t &qz);