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

namespace ovdb
{
	namespace meshing
	{
		enum OvdbMeshMethod { MESHING_NAIVE, MESHING_GREEDY };
		typedef struct _VolumeDimensions_
		{
			//Dimensions of a volume or sub-volume. Bounds are inclusive
			int32_t x0, x1, y0, y1, z0, z1;
			_VolumeDimensions_(int32_t _x0, int32_t _x1, int32_t _y0, int32_t _y1, int32_t _z0, int32_t _z1)
				: x0(_x0), x1(_x1), y0(_y0), y1(_y1), z0(_z0), z1(_z1) {}
			int32_t _VolumeDimensions_::sizeX() const { return abs(x1 - x0) + 1; }
			int32_t _VolumeDimensions_::sizeY() const { return abs(y1 - y0) + 1; }
			int32_t _VolumeDimensions_::sizeZ() const { return abs(z1 - z0) + 1; }
		} VolumeDimensions;

		typedef std::string NameType;
		typedef std::wstring IDType;
		const static IDType INVALID_GRID_ID = std::wstring();
	}
}

#ifdef LIB_OVDB_API
LIB_OVDB_API int OvdbInitialize();
LIB_OVDB_API int OvdbUninitialize();
LIB_OVDB_API int OvdbReadVdb(const std::string &filename, const std::string gridName, ovdb::meshing::IDType &gridID);
LIB_OVDB_API int OvdbWriteVdbGrid(const ovdb::meshing::IDType &gridID, const std::string &filename);
LIB_OVDB_API int OvdbVolumeToMesh(const ovdb::meshing::IDType &gridID, const ovdb::meshing::IDType &volumeID, ovdb::meshing::VolumeDimensions volumeDims, ovdb::meshing::OvdbMeshMethod meshMethod, float isoValue);
LIB_OVDB_API int OvdbYieldNextMeshPoint(const ovdb::meshing::IDType &volumeID, float &vx, float &vy, float &vz);
LIB_OVDB_API int OvdbYieldNextMeshPolygon(const ovdb::meshing::IDType &volumeID, uint32_t &i1, uint32_t &i2, uint32_t &i3);
LIB_OVDB_API int OvdbYieldNextMeshNormal(const ovdb::meshing::IDType &volumeID, float &nx, float &ny, float &nz);
LIB_OVDB_API ovdb::meshing::IDType OvdbCreateLibNoiseVolume(const ovdb::meshing::NameType &volumeName, float surfaceValue, const ovdb::meshing::VolumeDimensions &volumeDimensions, float &isoValue);
#endif