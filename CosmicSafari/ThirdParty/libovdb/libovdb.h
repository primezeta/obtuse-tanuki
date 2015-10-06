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
//namespace ovdb
//{
	LIB_OVDB_API void OvdbInitialize(const std::string filename);
	LIB_OVDB_API void OvdbUninitialize();
//}