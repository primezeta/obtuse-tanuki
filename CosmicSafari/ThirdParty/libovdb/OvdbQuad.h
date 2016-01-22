#include "libovdb.h"
#include "OpenVDBIncludes.h"

typedef openvdb::Vec3d QuadVertexType;
typedef openvdb::Vec4I QuadIndicesType;
typedef openvdb::Vec3I PolygonIndicesType;
enum CubeFace { XY_FACE, XZ_FACE, YZ_FACE };
const static int32_t CUBE_FACE_COUNT = YZ_FACE+1;
enum QuadVertexIndex { V0, V1, V2, V3 };
const static int32_t QUAD_VERTEX_INDEX_COUNT = V3+1;

LIB_OVDB_API class OvdbQuad
{
private:
	const std::vector<QuadVertexType> &vertices;
	QuadIndicesType indices;
	const CubeFace cubeFace;
	QuadVertexType quadSize;
	bool isMerged;

public:
	OvdbQuad(std::vector<QuadVertexType> &vs, QuadIndicesType is, CubeFace f) : vertices(vs), indices(is), cubeFace(f), isMerged(false)
	{
		setIndices(is);
	}
	OvdbQuad(const OvdbQuad &rhs) : vertices(rhs.vertices), cubeFace(rhs.quadFace()), quadSize(rhs.quadSizeUVW()), isMerged(rhs.quadIsMerged())
	{
		setIndices(QuadIndicesType(rhs(V0), rhs(V1), rhs(V2), rhs(V3)));
	}
	QuadVertexType operator[](QuadVertexIndex v) const { return vertices[indices[v]]; }
	openvdb::Int32 operator()(QuadVertexIndex v) const { return indices[v]; }
	const CubeFace &quadFace() const { return cubeFace; }
	const QuadVertexType &quadSizeUVW() const { return quadSize; }
	const double quadHeight() const { return quadSizeUVW().z(); }
	const double quadLength() const { return quadSizeUVW().y(); }
	const double quadWidth() const { return quadSizeUVW().x(); }
	bool quadIsMerged() const { return isMerged; }
	void setIsMerged() { isMerged = true; } //Can only merge a quad once
	double OvdbQuad::posW(QuadVertexIndex v) const { return posUVW(v).z(); }
	double OvdbQuad::posV(QuadVertexIndex v) const { return posUVW(v).y(); }
	double OvdbQuad::posU(QuadVertexIndex v) const { return posUVW(v).x(); }

	QuadVertexType OvdbQuad::posUVW(QuadVertexIndex v) const;
	void OvdbQuad::setIndices(const QuadIndicesType &newIndices);
	bool OvdbQuad::mergeQuadsByLength(OvdbQuad &rhs);
	bool OvdbQuad::mergeQuadsByWidth(OvdbQuad &rhs);
	bool OvdbQuad::isQuadAdjacentByLength(const OvdbQuad &rhs);
	bool OvdbQuad::isQuadAdjacentByWidth(const OvdbQuad &rhs);
};

//Sort quads by a total ordering
//via Mikola Lysenko at http://0fps.net/2012/06/30/meshing-in-a-minecraft-game
typedef struct _cmpByQuad_
{
	bool operator()(const OvdbQuad &l, const OvdbQuad &r) const
	{
		if (l.quadFace() != r.quadFace()) return l.quadFace() < r.quadFace();
		if (!openvdb::math::isApproxEqual(l.posW(V0), r.posW(V0))) return l.posW(V0) < r.posW(V0);
		if (!openvdb::math::isApproxEqual(l.posV(V0), r.posV(V0))) return l.posV(V0) < r.posV(V0);
		if (!openvdb::math::isApproxEqual(l.posU(V0), r.posU(V0))) return l.posU(V0) < r.posU(V0);
		if (!openvdb::math::isApproxEqual(l.quadWidth(), r.quadWidth())) return l.quadWidth() > r.quadWidth();
		return openvdb::math::isApproxEqual(l.quadLength(), r.quadLength()) || l.quadLength() > r.quadLength();
	}
} cmpByQuad;