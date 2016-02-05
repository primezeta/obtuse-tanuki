#include "OvdbTypes.h"
#include "OvdbQuad.h"
#include <unordered_map>

namespace ovdb
{
	namespace meshing
	{
		enum Plane2d { XY_FACE, XZ_FACE, YZ_FACE };
		const static size_t PLANE_2D_COUNT = YZ_FACE + 1;

		typedef std::unordered_map<OvdbQuadKey, OvdbQuad, OvdbQuadHash> UniqueQuadsType;
		typedef std::map<IDType, VolumeVerticesType> RegionVerticesType;
		typedef std::map<IDType, VolumePolygonsType> RegionPolygonsType;
		typedef std::map<IDType, VolumeNormalsType> RegionNormalsType;
		typedef std::map<IDType, UniqueQuadsType> RegionQuadsType;
		typedef std::map<IDType, IndexGridPtr> RegionVisitedVertexIndicesType;

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

		template<typename _TreeType> class OvdbVoxelVolume
		{
		public:
			typedef typename _TreeType TreeType;
			typedef typename TreeType::Ptr TreeTypePtr;
			typedef typename TreeType::ConstPtr TreeTypeCPtr;
			typedef typename openvdb::Grid<TreeType> GridType;
			typedef std::map<IDType, TreeTypePtr> RegionTreesType;
			typedef typename GridType::Ptr GridTypePtr;
			typedef typename GridType::ConstPtr GridTypeCPtr;
			typedef typename GridType::ValueAllCIter GridValueAllCIterType;
			typedef typename GridType::ValueAllIter GridValueAllIterType;
			typedef typename GridType::ValueOnCIter GridValueOnCIterType;
			typedef typename GridType::ValueOnIter GridValueOnIterType;
			typedef typename GridType::ValueOffCIter GridValueOffCIterType;
			typedef typename GridType::ValueOffIter GridValueOffIterType;

			//Note: Grids don't have an end value. Just need to check if the iter is null
			TreeTypeCPtr getCRegionTree(IDType regionID) const { RegionTreesType::const_iterator i = regionTrees.find(regionID); assert(i != regionTrees.cend()); return i->second; }
			TreeTypePtr getRegionTree(IDType regionID) { RegionTreesType::iterator i = regionTrees.find(regionID); assert(i != regionTrees.end()); return i->second; }
			GridValueAllCIterType valuesAllCBegin(IDType regionID) const { return getCRegionTree(regionID)->cbeginValueAll(); }
			GridValueAllIterType valuesAllBegin(IDType regionID) { return getRegionTree(regionID)->beginValueAll(); }
			GridValueOnCIterType valuesOnCBegin(IDType regionID) const { return getCRegionTree(regionID)->cbeginValueOn(); }
			GridValueOnIterType valuesOnBegin(IDType regionID) { return getRegionTree(regionID)->beginValueOn(); }
			GridValueOffCIterType valuesOffCBegin(IDType regionID) const { return getCRegionTree(regionID)->cbeginValueOff(); }
			GridValueOffIterType valuesOffBegin(IDType regionID) { return getRegionTree(regionID)->beginValueOff(); }

			GridTypeCPtr volumeGrid;

			OvdbVoxelVolume() : volumeGrid(nullptr) {};
			OvdbVoxelVolume(GridTypeCPtr grid) : volumeGrid(grid) {}

			//Copy the volume and visited grid with shared trees and deep-copy the vertices, polygons, and normals
			OvdbVoxelVolume(OvdbVoxelVolume &rhs) : volumeGrid(rhs.volumeGrid)
			{
				//Copy region trees and geometry. TODO: Safe to clear some of the held data such as tree ptrs?
				regionTrees.clear();
				regionVertices.clear();
				regionPolygons.clear();
				regionNormals.clear();
				for (int i = 0; i < PLANE_2D_COUNT; ++i)
				{
					regionQuads[i].clear();
				}

				for (RegionTreesType::iterator i = rhs.regionTrees.begin(); i != rhs.regionTrees.cend(); ++i)
				{
					regionTrees[i->first] = boost::static_pointer_cast<TreeType>(i->second->copy());
					visitedVertexIndices[i->first] = rhs.visitedVertexIndices[i->first]->copy();
					regionVertices[i->first].insert(regionVertices[i->first].begin(), rhs.regionVertices[i->first].begin(), rhs.regionVertices[i->first].end());
					regionPolygons[i->first].insert(regionPolygons[i->first].begin(), rhs.regionPolygons[i->first].begin(), rhs.regionPolygons[i->first].end());
					regionNormals[i->first].insert(regionNormals[i->first].begin(), rhs.regionNormals[i->first].begin(), rhs.regionNormals[i->first].end());
					for (int i = 0; i < PLANE_2D_COUNT; ++i)
					{
						regionQuads[i].insert(rhs.regionQuads[i].begin(), rhs.regionQuads[i].end());
					}
				}
			}

			VolumeVerticesType &getVertices(IDType regionID) { return regionVertices[regionID]; }
			VolumePolygonsType &getPolygons(IDType regionID) { return regionPolygons[regionID]; }
			VolumeNormalsType &getNormals(IDType regionID) { return regionNormals[regionID]; }

			void addRegion(IDType regionID, const openvdb::CoordBBox &regionBBox)
			{
				//Tree clipping is destructive so copy the tree prior to clipping in order to preserve the original tree
				//TODO: Possibly check if the region ID already exists and if so, handle a changed bounding box (or throw an error)
				TreeTypePtr subtreePtr = volumeGrid->tree().copy();
				subtreePtr->clip(regionBBox);
				regionTrees[regionID] = subtreePtr;
				regionVertices.insert(regionID, VolumeVerticesType());
				regionPolygons.insert(regionID, VolumePolygonsType());
				regionNormals.insert(regionID, VolumeNormalsType());
				for (int i = 0; i < PLANE_2D_COUNT; ++i)
				{
					regionQuads[i].insert(regionID, UniqueQuadsType());
				}
				//The visited vertex grid mirrors the volume grid, where the value of each voxel is the vertex index or max value if that voxel has not been visited
				auto i = visitedVertexIndices.insert(regionID, IndexGridType::create(INDEX_TYPE_MAX));
				i->second->setTransform(volumeGrid->transformPtr()->copy());
				i->second->topologyUnion(*subtreePtr);
			}

			void buildVolume(IDType regionID, VolumeStyle volumeStyle, float isoValue)
			{
				//Step through only voxels that are on
				for (GridValueOnCIterType i = valuesOnCBegin(regionID); i; ++i)
				{
					const CoordType &startCoord = i.getCoord();
					//Skip tile values and values that are not on the surface
					if (!i.isVoxelValue())
					{
						continue;
					}
					float value = getRegionGridValue(regionID, startCoord);
					if (!openvdb::math::isApproxEqual(value, isoValue))
					{
						continue;
					}

					//Set up the 6 quads
					if (volumeStyle == VOLUME_STYLE_CUBE)
					{
						OvdbPrimitiveCube primitiveIndices(startCoord);
						buildCubeQuads(regionID, primitiveIndices);
						OvdbQuad xy0(primitiveIndices.getQuadXY0());
						OvdbQuad xy1(primitiveIndices.getQuadXY1());
						OvdbQuad xz0(primitiveIndices.getQuadXZ0());
						OvdbQuad xz1(primitiveIndices.getQuadXZ1());
						OvdbQuad yz0(primitiveIndices.getQuadYZ0());
						OvdbQuad yz1(primitiveIndices.getQuadYZ1());
						regionQuads[XY_FACE][regionID][OvdbQuadKey(xy0)] = xy0;
						regionQuads[XY_FACE][regionID][OvdbQuadKey(xy1)] = xy1;
						regionQuads[XZ_FACE][regionID][OvdbQuadKey(xz0)] = xz0;
						regionQuads[XZ_FACE][regionID][OvdbQuadKey(xz1)] = xz1;
						regionQuads[YZ_FACE][regionID][OvdbQuadKey(yz0)] = yz0;
						regionQuads[YZ_FACE][regionID][OvdbQuadKey(yz1)] = yz1;
					}
				}
			}

			void doMesh(IDType regionID, OvdbMeshMethod method)
			{
				//Collect the quads in a linear list and mesh them. TODO: Better way to do this than copying them all?
				std::vector<OvdbQuad*> quads;
				for (int i = 0; i < PLANE_2D_COUNT; ++i)
				{
					UniqueQuadsType &unorderedQuads = regionQuads[i][regionID];
					for (UniqueQuadsType::iterator j = unorderedQuads.begin(); j != unorderedQuads.end(); ++j)
					{
						OvdbQuad *q = &(j->second);
						quads.push_back(q);
					}
				}

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
				for (std::vector<OvdbQuad*>::const_iterator i = quads.begin(); i != quads.end(); ++i)
				{
					OvdbQuad &q = **i;
					if (q.quadIsMerged())
					{
						continue;
					}
					//Collect triangle indices of the two triangles comprising this quad
					regionPolygons[regionID].push_back(q.quadPoly1());
					regionPolygons[regionID].push_back(q.quadPoly2());
					//regionNormals[regionID].push_back();
					//regionNormals[regionID].push_back();
					//regionNormals[regionID].push_back();
					//regionNormals[regionID].push_back();
				}
			}

		private:
			RegionTreesType regionTrees;
			RegionVerticesType regionVertices;
			RegionPolygonsType regionPolygons;
			RegionNormalsType regionNormals;
			RegionQuadsType regionQuads[PLANE_2D_COUNT]; //TODO: Set up iterator and get/set functions for regionQuads
			RegionVisitedVertexIndicesType visitedVertexIndices;

			float getGridValue(const CoordType &coord) { return volumeGrid->getConstAccessor().getValue(coord); }
			float getRegionGridValue(IDType regionID, const CoordType &coord) { return getRegionTree(regionID)->getValue(coord); }
			IndexType getVisitedVertexValue(IDType regionID, const CoordType &coord) { return visitedVertexIndices[regionID]->getConstAccessor().getValue(coord); }
			void setVisitedVertexValue(IDType regionID, const CoordType &coord, IndexType value) { visitedVertexIndices[regionID]->getAccessor().setValue(coord, value); }

			void buildCubeQuads(IDType regionID, OvdbPrimitiveCube &primitiveIndices)
			{
				//Make 6 quads, each of width / height 1
				for (uint32_t i = 0; i < CUBE_VERTEX_COUNT; ++i)
				{
					CoordType coord = primitiveIndices.getCoord((CubeVertex)i);
					IndexType vertexIndex = getVisitedVertexValue(regionID, coord);
					if (vertexIndex == UNVISITED_VERTEX_INDEX)
					{
						vertexIndex = IndexType(regionVertices[regionID].size()); //TODO: Error check index ranges
						TreeTypeCPtr region = regionTrees[regionID];
						QuadVertexType vertex = volumeGrid->indexToWorld(coord);
						regionVertices[regionID].push_back(vertex);
						//Since this is a new vertex save it to the global visited vertex grid for use by any other voxels in the same region that share it
						setVisitedVertexValue(regionID, coord, vertexIndex);
					}
					primitiveIndices.setVertexIndex((CubeVertex)i, vertexIndex);
				}
			}
		};
	}
}