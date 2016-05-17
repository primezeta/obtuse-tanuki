#pragma once
#include "EngineMinimal.h"
#pragma warning(push)
#pragma warning(1:4211 4800 4503 4146)
#include <openvdb/openvdb.h>

template<> OPENVDBMODULE_API inline std::string openvdb::TypedMetadata<openvdb::math::ScaleMap>::str() const;
template<> OPENVDBMODULE_API inline openvdb::math::ScaleMap openvdb::zeroVal<openvdb::math::ScaleMap>();
template<> OPENVDBMODULE_API inline openvdb::CoordBBox openvdb::zeroVal<openvdb::math::CoordBBox>();