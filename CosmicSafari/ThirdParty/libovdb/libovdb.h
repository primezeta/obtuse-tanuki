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

enum OvdbMeshMethod { MESHING_NAIVE, MESHING_GREEDY };

LIB_OVDB_API int OvdbInitialize();
LIB_OVDB_API int OvdbUninitialize();
LIB_OVDB_API int OvdbLoadVdb(const std::string &filename, const std::string &gridName);
LIB_OVDB_API int OvdbVolumeToMesh(OvdbMeshMethod meshMethod, int32_t regionCountX, int32_t regionCountY, int32_t regionCountZ, double isovalue = 0.0, double adaptivity = 0.0);
LIB_OVDB_API int OvdbGetNextMeshPoint(float &vx, float &vy, float &vz);
LIB_OVDB_API int OvdbGetNextMeshTriangle(uint32_t &i0, uint32_t &i1, uint32_t &i2);