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

			//Note: Grids don't have an end value. Just need to check if the iter is null
			GridValueAllCIterType valuesAllCBegin() const { return regionGrid->cbeginValueAll(); }
			GridValueAllIterType valuesAllBegin() { return regionGrid->beginValueAll(); }
			GridValueOnCIterType valuesOnCBegin() const { return regionGrid->cbeginValueOn(); }
			GridValueOnIterType valuesOnBegin() { return regionGrid->beginValueOn(); }
			GridValueOffCIterType valuesOffCBegin() const { return regionGrid->cbeginValueOff(); }
			GridValueOffIterType valuesOffBegin() { return regionGrid->beginValueOff(); }

			OvdbVoxelVolume() : volumeGrid(nullptr) {};
			OvdbVoxelVolume(GridTypeCPtr grid) : volumeGrid(grid) { }
			OvdbVoxelVolume(OvdbVoxelVolume &rhs) { (*this) = rhs; }
			OvdbVoxelVolume &operator=(const OvdbVoxelVolume &rhs)
			{
				volumeGrid = rhs.volumeGrid;
				regionGrid = rhs.regionGrid;
				visitedVertexIndices = rhs.visitedVertexIndices;
				volumeBBox = rhs.volumeBBox;
				vertices.clear();
				polygons.clear();
				normals.clear();
				quads.clear();
				vertices.insert(vertices.begin(), rhs.vertices.begin(), rhs.vertices.end());
				polygons.insert(polygons.begin(), rhs.polygons.begin(), rhs.polygons.end());
				normals.insert(normals.begin(), rhs.normals.begin(), rhs.normals.end());
				quads.insert(quads.begin(), rhs.quads.begin(), rhs.quads.end());
				return *this;
			}

			VolumeVerticesType &getVertices() { return vertices; }
			VolumePolygonsType &getPolygons() { return polygons; }
			VolumeNormalsType &getNormals() { return normals; }

			void doSurfaceMesh(const openvdb::math::CoordBBox &meshBBox, VolumeStyle volumeStyle, float isoValue, OvdbMeshMethod method) //TODO: Swap isovalue/surface value among parameters
			{
				initializeRegion(meshBBox);
				buildRegionMeshSurface(volumeStyle, isoValue);
				doMesh(method);
			}

		private:
			GridTypeCPtr volumeGrid;
			GridTypePtr regionGrid;
			IndexGridPtr visitedVertexIndices;
			openvdb::math::CoordBBox volumeBBox;
			VolumeVerticesType vertices;
			VolumePolygonsType polygons;
			VolumeNormalsType normals;
			//UniqueQuadsType quads;
			std::vector<OvdbQuad> quads;

			void initializeRegion(const openvdb::math::CoordBBox &bbox)
			{
				if (bbox == volumeBBox)
				{
					return; //Don't need to do anything
				}

				//Clip bounding box has changed
				volumeBBox = bbox;

				//regionGrid = GridType::create(volumeGrid->background());
				//regionGrid->setTransform(volumeGrid->transformPtr()->copy());
				//regionGrid->topologyUnion(*volumeGrid);
				//regionGrid->clip(volumeBBox);

				visitedVertexIndices = IndexGridType::create(INDEX_TYPE_MAX);
				visitedVertexIndices->setTransform(volumeGrid->transformPtr()->copy());
				visitedVertexIndices->topologyUnion(*volumeGrid);
				//visitedVertexIndices->clip(volumeBBox);
			}

			void buildRegionMeshSurface(VolumeStyle volumeStyle, float isoValue) 
			{
				//Step through only voxels that are on
				auto acc = volumeGrid->getAccessor();
				openvdb::VectorGrid::Ptr gradient = openvdb::tools::gradient(*volumeGrid);
				for (GridValueOnCIterType i = volumeGrid->cbeginValueOn(); i; ++i)
				{
					//Skip tile values and values that are not on the surface
					if (!i.isVoxelValue())
					{
						continue;
					}
					const CoordType &coord = i.getCoord();
					const float &density = acc.getValue(coord);
					if (volumeStyle == VOLUME_STYLE_CUBE)
					{
						//Mesh the voxel as a simple cube						
						OvdbPrimitiveCube primitiveIndices = buildCubeQuads(coord);
						quads.push_back(OvdbQuad(primitiveIndices.getQuadXY0()));
						quads.push_back(OvdbQuad(primitiveIndices.getQuadXY1()));
						quads.push_back(OvdbQuad(primitiveIndices.getQuadXZ0()));
						quads.push_back(OvdbQuad(primitiveIndices.getQuadXZ1()));
						quads.push_back(OvdbQuad(primitiveIndices.getQuadYZ0()));
						quads.push_back(OvdbQuad(primitiveIndices.getQuadYZ1()));
					}
					else if (volumeStyle == MARCHING_CUBES)
					{
						openvdb::VectorGrid::Ptr volumeGrad = openvdb::tools::gradient(*volumeGrid);
						const float gridValue = i.getValue();
						std::vector<openvdb::Vec3d> vertices;
						vertices.resize(8);
						vertices[0] = volumeGrid->indexToWorld(coord);
						vertices[1] = volumeGrid->indexToWorld(coord.offsetBy(1, 0, 0));
						vertices[2] = volumeGrid->indexToWorld(coord.offsetBy(0, 1, 0));
						vertices[3] = volumeGrid->indexToWorld(coord.offsetBy(1, 1, 0));
						vertices[4] = volumeGrid->indexToWorld(coord.offsetBy(0, 0, 1));
						vertices[5] = volumeGrid->indexToWorld(coord.offsetBy(1, 0, 1));
						vertices[6] = volumeGrid->indexToWorld(coord.offsetBy(0, 1, 1));
						vertices[7] = volumeGrid->indexToWorld(coord.offsetBy(1, 1, 1));
					}
				}
			}

			void doMesh(OvdbMeshMethod method)
			{
				////If method is naive do nothing special
				//if (method == MESHING_GREEDY)
				//{
				//	//Merge adjacent quads in a greedy manner
				//	for (std::vector<OvdbQuad*>::iterator i = quads.begin(); i != quads.end(); ++i)
				//	{
				//		auto j = i;
				//		++j;
				//		for (; j != quads.end() && ((*i)->mergeQuadsByLength(**j) || (*i)->mergeQuadsByWidth(**j)); ++j);
				//	}
				//}

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

		struct IDLessThan
		{
			bool operator()(const IDType &lhs, const IDType& rhs) const
			{
				return std::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
			}
		};
	}
}