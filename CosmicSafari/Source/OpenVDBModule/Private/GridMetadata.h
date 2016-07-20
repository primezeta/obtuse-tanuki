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
static std::string MetaName_WorldName(bool stdstring) { return "WorldName"; }
static FString MetaName_RegionScale() { return TEXT("RegionScale"); }
static std::string MetaName_RegionScale(bool stdstring) { return "RegionScale"; }
static FString MetaName_RegionStart() { return TEXT("RegionStart"); }
static std::string MetaName_RegionStart(bool stdstring) { return "RegionStart"; }
static FString MetaName_RegionEnd() { return TEXT("RegionEnd"); }
static std::string MetaName_RegionEnd(bool stdstring) { return "RegionEnd"; }
static FString MetaName_RegionIndexStart() { return TEXT("RegionIndexStart"); }
static std::string MetaName_RegionIndexStart(bool stdstring) { return "RegionIndexStart"; }
static FString MetaName_RegionIndexEnd() { return TEXT("RegionIndexEnd"); }
static std::string MetaName_RegionIndexEnd(bool stdstring) { return "RegionIndexEnd"; }