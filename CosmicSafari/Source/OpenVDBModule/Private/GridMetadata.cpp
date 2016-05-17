#include "OpenVDBModule.h"
#include "GridMetadata.h"

template<> inline std::string openvdb::TypedMetadata<openvdb::math::ScaleMap>::str() const
{
	return mValue.str();
}

template<> inline openvdb::math::ScaleMap openvdb::zeroVal<openvdb::math::ScaleMap>()
{
	return openvdb::math::ScaleMap(openvdb::Vec3d(0.0));
}

template<> inline openvdb::CoordBBox openvdb::zeroVal<openvdb::math::CoordBBox>()
{
	return openvdb::CoordBBox(openvdb::Coord(), openvdb::Coord());
}