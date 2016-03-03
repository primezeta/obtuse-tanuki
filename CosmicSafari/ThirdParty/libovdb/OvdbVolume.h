#include "OvdbTypes.h"
#include "OvdbQuad.h"
#include <unordered_map>

namespace ovdb
{
	namespace meshing
	{
		typedef std::unordered_map<OvdbQuadKey, OvdbQuad, OvdbQuadHash> UniqueQuadsType;

		class OvdbPrimitiveCube
		{
		public:
			OvdbPrimitiveCube(const CoordType &cubeStart)
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

		template<typename _TreeType>
		class OvdbVoxelVolume
		{
		public:
			typedef typename _TreeType TreeType;
			typedef typename TreeType::Ptr TreeTypePtr;
			typedef typename TreeType::ConstPtr TreeTypeCPtr;
			typedef typename openvdb::Grid<TreeType> GridType;
			typedef typename GridType::Ptr GridTypePtr;
			typedef typename GridType::ConstPtr GridTypeCPtr;
			typedef typename GridType::ValueAllCIter GridValueAllCIterType;
			typedef typename GridType::ValueAllIter GridValueAllIterType;
			typedef typename GridType::ValueOnCIter GridValueOnCIterType;
			typedef typename GridType::ValueOnIter GridValueOnIterType;
			typedef typename GridType::ValueOffCIter GridValueOffCIterType;
			typedef typename GridType::ValueOffIter GridValueOffIterType;

			OvdbVoxelVolume() : volumeGrid(nullptr) {}
			OvdbVoxelVolume(GridTypeCPtr grid) : volumeGrid(grid)
			{
				volAccPtr = boost::shared_ptr<openvdb::FloatGrid::ConstAccessor>(new openvdb::FloatGrid::ConstAccessor(volumeGrid->tree()));
			}
			OvdbVoxelVolume(OvdbVoxelVolume &rhs) { (*this) = rhs; }
			OvdbVoxelVolume &operator=(const OvdbVoxelVolume &rhs)
			{
				if (rhs.volumeGrid)
				{
					volumeGrid = rhs.volumeGrid;
				}
				if (rhs.volAccPtr)
				{
					volAccPtr.reset();
					volAccPtr = rhs.volAccPtr;
				}
				if (rhs.visitedVertexIndices)
				{
					visitedVertexIndices = openvdb::gridPtrCast<IndexGridType>(rhs.visitedVertexIndices->copyGrid());
				}
				if (rhs.idxAccPtr)
				{
					idxAccPtr.reset();
					idxAccPtr = rhs.idxAccPtr;
				}
				vertices.clear();
				polygons.clear();
				normals.clear();
				quads.clear();
				vertices.insert(vertices.begin(), rhs.vertices.begin(), rhs.vertices.end());
				polygons.insert(polygons.begin(), rhs.polygons.begin(), rhs.polygons.end());
				normals.insert(normals.begin(), rhs.normals.begin(), rhs.normals.end());
				quads.insert(quads.begin(), rhs.quads.begin(), rhs.quads.end());
				isMeshed = rhs.isMeshed;
				currentVertex = vertices.begin();
				currentPolygon = polygons.begin();
				currentNormal = normals.begin();
				return *this;
			}

			void initializeRegion()
			{
				isMeshed = false;
				if (visitedVertexIndices)
				{
					visitedVertexIndices.reset();
				}
				visitedVertexIndices = IndexGridType::create(UNVISITED_VERTEX_INDEX);
				visitedVertexIndices->setTransform(volumeGrid->transformPtr()->copy());
				visitedVertexIndices->topologyUnion(*volumeGrid);
				if (idxAccPtr)
				{
					idxAccPtr.reset();
				}
				idxAccPtr = boost::shared_ptr<IndexGridType::Accessor>(new IndexGridType::Accessor(visitedVertexIndices->tree()));
				vertices.clear();
				polygons.clear();
				normals.clear();
				currentVertex = vertices.begin();
				currentPolygon = polygons.begin();
				currentNormal = normals.begin();
			}

			void doPrimitiveCubesMesh()
			{
				if (!isMeshed)
				{
					isMeshed = true;
					//Step through only voxels that are on
					for (GridVdbType::ValueOnCIter i = volumeGrid->cbeginValueOn(); i; ++i)
					{
						const openvdb::Coord coord = i.getCoord();
						const float &density = volAccPtr->getValue(coord);
						//Mesh the voxel as a simple cube
						OvdbPrimitiveCube primitiveIndices = buildCubeQuads(coord);
						quads.push_back(OvdbQuad(primitiveIndices.getQuadXY0()));
						quads.push_back(OvdbQuad(primitiveIndices.getQuadXY1()));
						quads.push_back(OvdbQuad(primitiveIndices.getQuadXZ0()));
						quads.push_back(OvdbQuad(primitiveIndices.getQuadXZ1()));
						quads.push_back(OvdbQuad(primitiveIndices.getQuadYZ0()));
						quads.push_back(OvdbQuad(primitiveIndices.getQuadYZ1()));
					}
					collectGeometry();
				}
			}

			void doMarchingCubesMesh(const float &surfaceValue)
			{
				//TODO
			}

			bool nextVertex(double &v1, double &v2, double &v3)
			{
				if (currentVertex == vertices.end())
				{
					return false;
				}
				v1 = (*currentVertex)[0];
				v2 = (*currentVertex)[1];
				v3 = (*currentVertex)[2];
				return ++currentVertex != vertices.end();
			}

			bool nextPolygon(uint32_t &p1, uint32_t &p2, uint32_t &p3)
			{
				if (currentPolygon == polygons.end())
				{
					return false;
				}
				p1 = (*currentPolygon)[0];
				p2 = (*currentPolygon)[1];
				p3 = (*currentPolygon)[2];
				return ++currentPolygon != polygons.end();
			}

			bool nextNormal(double &n1, double &n2, double &n3)
			{
				if (currentNormal == normals.end())
				{
					return false;
				}
				n1 = (*currentNormal)[0];
				n2 = (*currentNormal)[1];
				n3 = (*currentNormal)[2];
				return ++currentNormal != normals.end();
			}

		private:
			GridTypeCPtr volumeGrid;
			IndexGridPtr visitedVertexIndices;
			boost::shared_ptr<openvdb::FloatGrid::ConstAccessor> volAccPtr;
			boost::shared_ptr<IndexGridType::Accessor> idxAccPtr;
			VolumeVerticesType vertices;
			VolumePolygonsType polygons;
			VolumeNormalsType normals;
			VolumeVerticesType::const_iterator currentVertex;
			VolumePolygonsType::const_iterator currentPolygon;
			VolumeNormalsType::const_iterator currentNormal;

			//UniqueQuadsType quads;
			std::vector<OvdbQuad> quads;
			bool isMeshed;

			void collectGeometry()
			{
				//Finally, collect polygon and normal data
				//for (UniqueQuadsType::const_iterator i = quads.begin(); i != quads.end(); ++i)
				for (std::vector<OvdbQuad>::const_iterator i = quads.begin(); i != quads.end(); ++i)
				{
					//const OvdbQuad &q = i->second;
					const OvdbQuad &q = *i;
					if (q.quadIsMerged())
					{
						continue;
					}
					//Collect triangle indices of the two triangles comprising this quad
					polygons.push_back(q.quadPoly1());
					polygons.push_back(q.quadPoly2());
					//normals.push_back();
					//normals.push_back();
					//normals.push_back();
					//normals.push_back();
				}
				currentVertex = vertices.begin();
				currentPolygon = polygons.begin();
				currentNormal = normals.begin();
			}

			OvdbPrimitiveCube buildCubeQuads(const openvdb::Coord &startCoord)
			{
				//Make 6 quads, each of width / height 1
				OvdbPrimitiveCube primitiveIndices(startCoord);
				for (uint32_t i = 0; i < CUBE_VERTEX_COUNT; ++i)
				{
					const CubeVertex &vtx = (CubeVertex)i;
					const openvdb::Coord &coord = primitiveIndices.getCoord(vtx);					
					IndexType vertexIndex = idxAccPtr->getValue(coord);
					if (vertexIndex == UNVISITED_VERTEX_INDEX)
					{
						vertexIndex = IndexType(vertices.size()); //TODO: Error check index ranges
						vertices.push_back(volumeGrid->indexToWorld(coord));
						//Since this is a new vertex save it to the global visited vertex grid for use by any other voxels in the same region that share it
						idxAccPtr->setValue(coord, vertexIndex);
					}
					primitiveIndices[vtx] = vertexIndex;
				}
				return primitiveIndices;
			}
		};
	}
}