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

			void setVertexIndex(CubeVertex v, IndexType i) { primitiveIndices[v] = i; }
			CoordType& getCoord(CubeVertex v) { return primitiveVertices[v]; }
			IndexType getIndex(CubeVertex v) { return primitiveIndices[v]; }
			//Add the vertex indices in counterclockwise order on each quad face
			QuadIndicesType getQuadXY0() { return openvdb::Vec4I(primitiveIndices[VX3], primitiveIndices[VX4], primitiveIndices[VX7], primitiveIndices[VX5]); }
			QuadIndicesType getQuadXY1() { return openvdb::Vec4I(primitiveIndices[VX6], primitiveIndices[VX2], primitiveIndices[VX0], primitiveIndices[VX1]); }
			QuadIndicesType getQuadXZ0() { return openvdb::Vec4I(primitiveIndices[VX7], primitiveIndices[VX4], primitiveIndices[VX2], primitiveIndices[VX6]); }
			QuadIndicesType getQuadXZ1() { return openvdb::Vec4I(primitiveIndices[VX5], primitiveIndices[VX1], primitiveIndices[VX0], primitiveIndices[VX3]); }
			QuadIndicesType getQuadYZ0() { return openvdb::Vec4I(primitiveIndices[VX7], primitiveIndices[VX6], primitiveIndices[VX1], primitiveIndices[VX5]); }
			QuadIndicesType getQuadYZ1() { return openvdb::Vec4I(primitiveIndices[VX0], primitiveIndices[VX2], primitiveIndices[VX4], primitiveIndices[VX3]); }

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
			TreeTypeCPtr getCRegionTree() const { return regionTree; }
			TreeTypePtr getRegionTree() { return regionTree; }
			GridValueAllCIterType valuesAllCBegin() const { return getCRegionTree()->cbeginValueAll(); }
			GridValueAllIterType valuesAllBegin() { return getRegionTree()->beginValueAll(); }
			GridValueOnCIterType valuesOnCBegin() const { return getCRegionTree()->cbeginValueOn(); }
			GridValueOnIterType valuesOnBegin() { return getRegionTree()->beginValueOn(); }
			GridValueOffCIterType valuesOffCBegin() const { return getCRegionTree()->cbeginValueOff(); }
			GridValueOffIterType valuesOffBegin() { return getRegionTree()->beginValueOff(); }

			OvdbVoxelVolume() : volumeGrid(nullptr) {};
			OvdbVoxelVolume(GridTypeCPtr grid) : volumeGrid(grid) {}
			OvdbVoxelVolume(OvdbVoxelVolume &rhs) : volumeGrid(rhs.volumeGrid)
			{
				vertices.clear();
				polygons.clear();
				normals.clear();
				quads.clear();
				regionTree = boost::static_pointer_cast<TreeType>(rhs.regionTree->copy());
				visitedVertexIndices = rhs.visitedVertexIndices->copy();
				vertices.insert(vertices.begin(), rhs.vertices.begin(), rhs.vertices.end());
				polygons.insert(polygons.begin(), rhs.polygons.begin(), rhs.polygons.end());
				normals.insert(normals.begin(), rhs.normals.begin(), rhs.normals.end());
				quads.insert(rhs.quads.begin(), rhs.quads.end());
			}

			VolumeVerticesType &getVertices() { return vertices; }
			VolumePolygonsType &getPolygons() { return polygons; }
			VolumeNormalsType &getNormals() { return normals; }

			void doSurfaceMesh(const openvdb::math::CoordBBox &meshBBox, VolumeStyle volumeStyle, float isoValue, OvdbMeshMethod method) //TODO: Swap isovalue/surface value among parameters
			{
				regionBBox = meshBBox;
				initializeRegion();
				buildRegionMeshSurface(volumeStyle, isoValue);
				doMesh(method);
			}

		private:
			const GridTypeCPtr volumeGrid;
			TreeTypePtr regionTree;
			openvdb::math::CoordBBox regionBBox;
			VolumeVerticesType vertices;
			VolumePolygonsType polygons;
			VolumeNormalsType normals;
			UniqueQuadsType quads;
			IndexGridPtr visitedVertexIndices;

			void initializeRegion()
			{
				//The visited vertex grid mirrors the region, except that values are indices of vertices already used by a quad (or max index if not yet used)
				visitedVertexIndices = IndexGridType::create(INDEX_TYPE_MAX);
				visitedVertexIndices->setTransform(volumeGrid->transformPtr()->copy());
				visitedVertexIndices->topologyUnion(*volumeGrid);
				visitedVertexIndices->clip(regionBBox);

				//Tree clipping is destructive so copy the grid tree prior to clipping
				TreeTypePtr subtreePtr = boost::static_pointer_cast<TreeType>(volumeGrid->tree().copy());
				subtreePtr->clip(regionBBox);
				regionTree = subtreePtr;
			}

			void buildRegionMeshSurface(VolumeStyle volumeStyle, float isoValue) 
			{
				//Step through only voxels that are on
				for (GridValueOnCIterType i = valuesOnCBegin(); i; ++i)
				{
					const CoordType &startCoord = i.getCoord();
					//Skip tile values and values that are not on the surface
					if (!i.isVoxelValue())
					{
						continue;
					}
					float value = volumeGrid->getConstAccessor().getValue(startCoord);
					if (!openvdb::math::isApproxEqual(value, isoValue))
					{
						continue;
					}

					//Set up the 6 quads
					if (volumeStyle == VOLUME_STYLE_CUBE)
					{
						OvdbPrimitiveCube primitiveIndices(startCoord);
						buildCubeQuads(primitiveIndices);
						OvdbQuad xy0(primitiveIndices.getQuadXY0());
						OvdbQuad xy1(primitiveIndices.getQuadXY1());
						OvdbQuad xz0(primitiveIndices.getQuadXZ0());
						OvdbQuad xz1(primitiveIndices.getQuadXZ1());
						OvdbQuad yz0(primitiveIndices.getQuadYZ0());
						OvdbQuad yz1(primitiveIndices.getQuadYZ1());
						quads[OvdbQuadKey(xy0)] = xy0;
						quads[OvdbQuadKey(xy1)] = xy1;
						quads[OvdbQuadKey(xz0)] = xz0;
						quads[OvdbQuadKey(xz1)] = xz1;
						quads[OvdbQuadKey(yz0)] = yz0;
						quads[OvdbQuadKey(yz1)] = yz1;
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
				for (UniqueQuadsType::const_iterator i = quads.begin(); i != quads.end(); ++i)
				{
					const OvdbQuad &q = i->second;
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

			void buildCubeQuads(OvdbPrimitiveCube &primitiveIndices)
			{
				//Make 6 quads, each of width / height 1
				for (uint32_t i = 0; i < CUBE_VERTEX_COUNT; ++i)
				{
					CoordType coord = primitiveIndices.getCoord((CubeVertex)i);
					IndexGridType::Accessor acc = visitedVertexIndices->getAccessor();
					IndexType vertexIndex = acc.getValue(coord);
					if (vertexIndex == UNVISITED_VERTEX_INDEX)
					{
						vertexIndex = IndexType(vertices.size()); //TODO: Error check index ranges
						QuadVertexType vertex = volumeGrid->indexToWorld(coord);
						vertices.push_back(vertex);
						//Since this is a new vertex save it to the global visited vertex grid for use by any other voxels in the same region that share it
						acc.setValue(coord, vertexIndex);
					}
					primitiveIndices.setVertexIndex((CubeVertex)i, vertexIndex);
				}
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