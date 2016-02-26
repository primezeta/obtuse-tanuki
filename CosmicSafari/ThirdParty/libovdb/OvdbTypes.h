#pragma once
#include "libovdb.h"
#pragma warning(push, 0)
#include <openvdb/openvdb.h>
#include <openvdb/Exceptions.h>
#include <openvdb/tools/Dense.h>
#include <openvdb/tools/VolumeToMesh.h>
#include <openvdb/tools/Clip.h>
#include <openvdb/tools/GridOperators.h>
#pragma warning(pop)

namespace ovdb
{
	typedef openvdb::FloatTree TreeVdbType;
	typedef openvdb::UInt32Tree IndexTreeType;
	typedef openvdb::Grid<TreeVdbType> GridVdbType;
	typedef openvdb::Vec3d QuadVertexType;
	typedef openvdb::Vec4I QuadIndicesType;
	typedef openvdb::Vec3I PolygonIndicesType;
	typedef openvdb::Vec2I QuadUVType;
	typedef openvdb::Index32 IndexType;
	typedef GridVdbType::Ptr GridPtr;
	typedef GridVdbType::ConstPtr GridCPtr;
	typedef GridVdbType::Accessor GridAcc;
	typedef GridVdbType::ConstAccessor GridCAcc;
	typedef openvdb::Coord CoordType;
	typedef openvdb::Grid<IndexTreeType> IndexGridType;
	typedef IndexGridType::Ptr IndexGridPtr;
	typedef IndexGridType::Ptr IndexGridCPtr;
	
	const static openvdb::Index32 INDEX_TYPE_MAX = UINT32_MAX;

	namespace meshing
	{
		typedef std::vector<QuadVertexType> VolumeVerticesType;
		typedef std::vector<PolygonIndicesType> VolumePolygonsType;
		typedef std::vector<QuadVertexType> VolumeNormalsType;

		enum MeshMethod { METHOD_PRIMITIVE_CUBES, METHOD_MARCHING_CUBES };
		enum CubeVertex { VX0, VX1, VX2, VX3, VX4, VX5, VX6, VX7, VX8 };
		enum QuadVertexIndex { V0, V1, V2, V3 };
		const static IndexType UNVISITED_VERTEX_INDEX = INDEX_TYPE_MAX;
		const static size_t CUBE_VERTEX_COUNT = VX8 + 1;
		const static size_t QUAD_VERTEX_INDEX_COUNT = V3 + 1;
	}
}