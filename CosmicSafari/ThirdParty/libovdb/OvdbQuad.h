#include "OvdbTypes.h"

namespace ovdb
{
	namespace meshing
	{
		class OvdbQuad
		{
		private:
			QuadIndicesType indices;
			bool isMerged;

		public:
			OvdbQuad() {}
			OvdbQuad(QuadIndicesType idxs) : indices(idxs), isMerged(false) {}
			OvdbQuad(const OvdbQuad &rhs) : indices(rhs.indices), isMerged(rhs.isMerged) {}
			OvdbQuad& operator=(const OvdbQuad &rhs)
			{
				indices = rhs.indices;
				isMerged = rhs.isMerged;
				return *this;
			}
			const QuadIndicesType& quad() const { return indices; }
			QuadUVType quadU() { return openvdb::Vec2I(indices[V0], indices[V1]); }
			QuadUVType quadV() { return openvdb::Vec2I(indices[V0], indices[V3]); }
			PolygonIndicesType quadPoly1() { return openvdb::Vec3I(indices[V0], indices[V1], indices[V2]); }
			PolygonIndicesType quadPoly2() { return openvdb::Vec3I(indices[V0], indices[V2], indices[V3]); }
			bool quadIsMerged() const { return isMerged; }
			void setIsMerged() { isMerged = true; }
			void mergeU(OvdbQuad &rhs)
			{
				if (!rhs.quadIsMerged())
				{
					rhs.setIsMerged();
					indices = QuadIndicesType(indices[0], rhs.indices[1], rhs.indices[2], indices[3]);
				}
			}
			void mergeV(OvdbQuad &rhs)
			{
				if (!rhs.quadIsMerged())
				{
					rhs.setIsMerged();
					indices = QuadIndicesType(indices[0], indices[1], rhs.indices[2], rhs.indices[3]);
				}
			}
		};

		////Sort quads by a total ordering
		////via Mikola Lysenko at http://0fps.net/2012/06/30/meshing-in-a-minecraft-game
		//typedef struct _cmpByQuad_
		//{
		//	bool operator()(const OvdbQuad &l, const OvdbQuad &r) const
		//	{
		//		if (l.quadFace() != r.quadFace()) return l.quadFace() < r.quadFace();
		//		if (!openvdb::math::isApproxEqual(l.posW(V0), r.posW(V0))) return l.posW(V0) < r.posW(V0);
		//		if (!openvdb::math::isApproxEqual(l.posV(V0), r.posV(V0))) return l.posV(V0) < r.posV(V0);
		//		if (!openvdb::math::isApproxEqual(l.posU(V0), r.posU(V0))) return l.posU(V0) < r.posU(V0);
		//		if (!openvdb::math::isApproxEqual(l.quadWidth(), r.quadWidth())) return l.quadWidth() > r.quadWidth();
		//		return openvdb::math::isApproxEqual(l.quadLength(), r.quadLength()) || l.quadLength() > r.quadLength();
		//	}
		//} cmpByQuad;

		typedef struct _OvdbQuadKey_
		{
			_OvdbQuadKey_(const OvdbQuad &q) : indices(q.quad()) {}
			_OvdbQuadKey_(const _OvdbQuadKey_ &rhs) : indices(rhs.indices) {}
			const QuadIndicesType &indices;
			bool operator==(const _OvdbQuadKey_ &rhs) const
			{
				return indices.x() == rhs.indices.x() &&
					indices.y() == rhs.indices.y() &&
					indices.z() == rhs.indices.z() &&
					indices.w() == rhs.indices.w();
			}
		} OvdbQuadKey;

		struct OvdbQuadHash
		{
			std::size_t operator()(const OvdbQuadKey& k) const
			{
				return std::hash<IndexType>()(k.indices.x())
					^ ((std::hash<IndexType>()(k.indices.y()) << 1) >> 1)
					^ ((std::hash<IndexType>()(k.indices.z()) << 1) >> 1)
					^ (std::hash<IndexType>()(k.indices.w()) << 1);
			}
		};
	}
}