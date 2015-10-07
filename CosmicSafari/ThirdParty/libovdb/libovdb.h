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

LIB_OVDB_API int OvdbInitialize(std::string errorMsg, const std::string filename);
//LIB_OVDB_API int OvdbUninitialize(std::string errorMsg);
LIB_OVDB_API void OvdbUninitialize();
LIB_OVDB_API int OvdbVolumeToMesh(std::string &errorMsg,
	std::vector<float> &pxs, std::vector<float> &pys, std::vector<float> &pzs,
	std::vector<unsigned int> &qxs, std::vector<unsigned int> &qys, std::vector<unsigned int> &qzs, std::vector<unsigned int> &qws);