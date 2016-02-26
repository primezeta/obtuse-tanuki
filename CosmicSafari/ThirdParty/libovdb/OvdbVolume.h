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

			OvdbVoxelVolume() : volumeGrid(nullptr) {};
			OvdbVoxelVolume(GridTypeCPtr grid) : volumeGrid(grid) { }
			OvdbVoxelVolume(OvdbVoxelVolume &rhs) { (*this) = rhs; }
			OvdbVoxelVolume &operator=(const OvdbVoxelVolume &rhs)
			{
				if (rhs.volumeGrid)
				{
					volumeGrid = rhs.volumeGrid;
				}
				if (rhs.regionMask)
				{
					regionMask = openvdb::gridPtrCast<openvdb::BoolGrid>(rhs.regionMask->copyGrid());
				}
				if (rhs.visitedVertexIndices)
				{
					visitedVertexIndices = openvdb::gridPtrCast<IndexGridType>(rhs.visitedVertexIndices->copyGrid());
				}
				vertices.clear();
				polygons.clear();
				normals.clear();
				quads.clear();
				vertices.insert(vertices.begin(), rhs.vertices.begin(), rhs.vertices.end());
				polygons.insert(polygons.begin(), rhs.polygons.begin(), rhs.polygons.end());
				normals.insert(normals.begin(), rhs.normals.begin(), rhs.normals.end());
				quads.insert(quads.begin(), rhs.quads.begin(), rhs.quads.end());
				regionBBox = rhs.regionBBox;
				isMeshed = rhs.isMeshed;
				return *this;
			}

			VolumeVerticesType &getVertices() { return vertices; }
			VolumePolygonsType &getPolygons() { return polygons; }
			VolumeNormalsType &getNormals() { return normals; }
			size_t vertexCount() { return vertices.size(); }
			size_t polygonCount() { return polygons.size(); }
			size_t normalCount() { return normals.size(); }

			void initializeRegion(const openvdb::math::CoordBBox &bbox)
			{
				if (regionBBox != bbox)
				{
					regionBBox = bbox;
					isMeshed = false;
					auto acc = volumeGrid->getAccessor();
					regionMask = openvdb::BoolGrid::create(false);
					regionMask->setTransform(volumeGrid->transformPtr()->copy());
					openvdb::BoolGrid::Accessor maskAcc = regionMask->getAccessor();
					for (int32_t x = bbox.min().x(); x <= bbox.max().x(); ++x)
					{
						for (int32_t y = bbox.min().y(); y <= bbox.max().y(); ++y)
						{
							for (int32_t z = bbox.min().z(); z <= bbox.max().z(); ++z)
							{
								const openvdb::Coord coord(x, y, z);
								if (acc.isValueOn(coord))
								{
									maskAcc.setValueOn(coord, true);
								}
							}
						}
					}
					visitedVertexIndices = IndexGridType::create(UNVISITED_VERTEX_INDEX);
					visitedVertexIndices->setTransform(volumeGrid->transformPtr()->copy());
					visitedVertexIndices->topologyUnion(*regionMask);
				}
			}

			void doPrimitiveCubesMesh()
			{
				if (!isMeshed)
				{
					isMeshed = true;
					//Step through only voxels that are on
					auto acc = volumeGrid->getAccessor();
					for (openvdb::BoolGrid::ValueOnCIter i = regionMask->cbeginValueOn(); i; ++i)
					{
						const openvdb::Coord coord = i.getCoord();
						const float &density = acc.getValue(coord);
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

		private:
			GridTypeCPtr volumeGrid;
			openvdb::BoolGrid::Ptr regionMask;
			IndexGridPtr visitedVertexIndices;
			VolumeVerticesType vertices;
			VolumePolygonsType polygons;
			VolumeNormalsType normals;
			//UniqueQuadsType quads;
			std::vector<OvdbQuad> quads;
			openvdb::CoordBBox regionBBox;
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
			}

			OvdbPrimitiveCube buildCubeQuads(const openvdb::Coord &startCoord)
			{
				//Make 6 quads, each of width / height 1
				OvdbPrimitiveCube primitiveIndices(startCoord);
				for (uint32_t i = 0; i < CUBE_VERTEX_COUNT; ++i)
				{
					CubeVertex vtx = (CubeVertex)i;
					CoordType coord = primitiveIndices.getCoord(vtx);
					IndexGridType::Accessor acc = visitedVertexIndices->getAccessor();
					IndexType vertexIndex = acc.getValue(coord);
					if (vertexIndex == UNVISITED_VERTEX_INDEX)
					{
						vertexIndex = IndexType(vertices.size()); //TODO: Error check index ranges
						vertices.push_back(volumeGrid->indexToWorld(coord));
						//Since this is a new vertex save it to the global visited vertex grid for use by any other voxels in the same region that share it
						acc.setValue(coord, vertexIndex);
					}
					primitiveIndices[vtx] = vertexIndex;
				}
				return primitiveIndices;
			}
		};
	}
}