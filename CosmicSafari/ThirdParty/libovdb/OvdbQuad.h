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
			QuadUVType quadU() const { return openvdb::Vec2I(indices[0], indices[1]); }
			QuadUVType quadV() const { return openvdb::Vec2I(indices[0], indices[3]); }
			PolygonIndicesType quadPoly1() const { return openvdb::Vec3I(indices[0], indices[1], indices[2]); }
			PolygonIndicesType quadPoly2() const { return openvdb::Vec3I(indices[0], indices[2], indices[3]); }
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
				return indices.eq(rhs.indices);
			}
		} OvdbQuadKey;

		struct OvdbQuadHash
		{
			std::size_t operator()(const OvdbQuadKey& k) const
			{
				//Sort the indices so that a face that is shared by a single cube hashes to the same value
				std::vector<IndexType> idxs;
				idxs.push_back(k.indices[0]);
				idxs.push_back(k.indices[1]);
				idxs.push_back(k.indices[2]);
				idxs.push_back(k.indices[3]);
				std::sort(idxs.begin(), idxs.end());
				return std::hash<IndexType>()(idxs[0])
					^ ((std::hash<IndexType>()(idxs[1]) << 1) >> 1)
					^ ((std::hash<IndexType>()(idxs[2]) << 1) >> 1)
					^ (std::hash<IndexType>()(idxs[3]) << 1);
			}
		};
	}
}