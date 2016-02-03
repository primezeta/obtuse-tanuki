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

#include "OvdbTypes.h"

LIB_OVDB_API int OvdbInitialize();
LIB_OVDB_API int OvdbUninitialize();
LIB_OVDB_API int OvdbReadVdb(const std::string &filename, const std::string gridName, IDType &gridID);
LIB_OVDB_API int OvdbWriteVdbGrid(const IDType &gridID, const std::string &filename);
LIB_OVDB_API int OvdbVolumeToMesh(const IDType &gridID, const std::string &filename, OvdbMeshMethod meshMethod, float isovalue);
LIB_OVDB_API int OvdbVolumeToMesh(const IDType &gridID, OvdbMeshMethod meshMethod, float isovalue);
LIB_OVDB_API int OvdbYieldNextMeshPoint(const IDType &gridID, float &vx, float &vy, float &vz);
LIB_OVDB_API int OvdbYieldNextMeshPolygon(const IDType &gridID, uint32_t &i1, uint32_t &i2, uint32_t &i3);
LIB_OVDB_API int OvdbYieldNextMeshNormal(const IDType &gridID, float &nx, float &ny, float &nz);
LIB_OVDB_API IDType OvdbCreateLibNoiseVolume(const std::string &gridName, float surfaceValue, const VolumeDimensions &dimensions, uint32_t libnoiseRange, float &isovalue);