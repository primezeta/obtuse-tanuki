#ifdef LIB_OVDB_DLL
	#ifdef LIB_OVDB_EXPORT
		#define LIB_OVDB_API __declspec(dllexport)
	#else
		#define LIB_OVDB_API __declspec(dllimport)
	#endif
#else
	#define LIB_OVDB_API
#endif

LIB_OVDB_API void OvdbInitialize();
LIB_OVDB_API void OvdbUninitialize();

//class GridBase;
//template <GridType>
//class OvdbGrid
//{
//public:
//
//private:
//};