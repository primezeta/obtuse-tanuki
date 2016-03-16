#pragma once
#include "libovdb.h"
#pragma warning(push, 0)
#include <openvdb/openvdb.h>
#include <openvdb/Exceptions.h>
#include <openvdb/tools/Dense.h>
#include <openvdb/tools/VolumeToMesh.h>
#include <openvdb/tools/Clip.h>
#include <openvdb/tools/GridOperators.h>
#include <noise.h>
#include <noiseutils.h>
#pragma warning(pop)

namespace ovdb
{
	typedef openvdb::FloatTree TreeVdbType;
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
	
	const static openvdb::Index32 INDEX_TYPE_MAX = UINT32_MAX;

	namespace meshing
	{
		typedef std::vector<QuadVertexType> VolumeVerticesType;
		typedef std::vector<PolygonIndicesType> VolumePolygonsType;
		typedef std::vector<QuadVertexType> VolumeNormalsType;

		enum CubeVertex { VX0, VX1, VX2, VX3, VX4, VX5, VX6, VX7, VX8 };
		enum QuadVertexIndex { V0, V1, V2, V3 };
		const static IndexType UNVISITED_VERTEX_INDEX = INDEX_TYPE_MAX;
		const static size_t CUBE_VERTEX_COUNT = VX8 + 1;
		const static size_t QUAD_VERTEX_INDEX_COUNT = V3 + 1;

		class PrimitiveCube
		{
		public:
			PrimitiveCube(const CoordType &cubeStart)
			{
				openvdb::CoordBBox bbox = openvdb::CoordBBox::createCube(cubeStart, 1);
				primitiveVertices[0] = bbox.getStart();
				primitiveVertices[1] = bbox.getStart().offsetBy(1, 0, 0);
				primitiveVertices[2] = bbox.getStart().offsetBy(0, 1, 0);
				primitiveVertices[3] = bbox.getStart().offsetBy(0, 0, 1);
				primitiveVertices[4] = bbox.getEnd().offsetBy(-1, 0, 0);
				primitiveVertices[5] = bbox.getEnd().offsetBy(0, -1, 0);
				primitiveVertices[6] = bbox.getEnd().offsetBy(0, 0, -1);
				primitiveVertices[7] = bbox.getEnd();
			}

			IndexType& operator[](CubeVertex v) { return primitiveIndices[v]; }
			CoordType& getCoord(CubeVertex v) { return primitiveVertices[v]; }
			//Add the vertex indices in counterclockwise order on each quad face
			QuadIndicesType getQuadXY0() { return openvdb::Vec4I(primitiveIndices[3], primitiveIndices[4], primitiveIndices[7], primitiveIndices[5]); }
			QuadIndicesType getQuadXY1() { return openvdb::Vec4I(primitiveIndices[6], primitiveIndices[2], primitiveIndices[0], primitiveIndices[1]); }
			QuadIndicesType getQuadXZ0() { return openvdb::Vec4I(primitiveIndices[7], primitiveIndices[4], primitiveIndices[2], primitiveIndices[6]); }
			QuadIndicesType getQuadXZ1() { return openvdb::Vec4I(primitiveIndices[5], primitiveIndices[1], primitiveIndices[0], primitiveIndices[3]); }
			QuadIndicesType getQuadYZ0() { return openvdb::Vec4I(primitiveIndices[7], primitiveIndices[6], primitiveIndices[1], primitiveIndices[5]); }
			QuadIndicesType getQuadYZ1() { return openvdb::Vec4I(primitiveIndices[0], primitiveIndices[2], primitiveIndices[4], primitiveIndices[3]); }
		private:
			CoordType primitiveVertices[CUBE_VERTEX_COUNT];
			IndexType primitiveIndices[CUBE_VERTEX_COUNT];
		};
	}
}