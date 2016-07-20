#pragma once
#include "EngineMinimal.h"
#pragma warning(push)
#pragma warning(1:4211 4800 4503 4146)
#include <openvdb/openvdb.h>

//Template specializations so that the respective types can be used as metadata
template<> OPENVDBMODULE_API inline std::string openvdb::TypedMetadata<openvdb::math::ScaleMap>::str() const;
template<> OPENVDBMODULE_API inline openvdb::math::ScaleMap openvdb::zeroVal<openvdb::math::ScaleMap>();
template<> OPENVDBMODULE_API inline openvdb::CoordBBox openvdb::zeroVal<openvdb::math::CoordBBox>();

//Metaname strings
//TODO: Add namespace
static FString MetaName_WorldName() { return TEXT("WorldName"); }
static FString MetaName_RegionScale() { return TEXT("RegionScale"); }
static FString MetaName_RegionStart() { return TEXT("RegionStart"); }
static FString MetaName_RegionEnd() { return TEXT("RegionEnd"); }
static FString MetaName_RegionIndexStart() { return TEXT("RegionIndexStart"); }
static FString MetaName_RegionIndexEnd() { return TEXT("RegionIndexEnd"); }