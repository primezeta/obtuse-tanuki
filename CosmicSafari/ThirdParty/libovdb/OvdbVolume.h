#include "libovdb.h"
#include "OvdbQuad.h"
#include <unordered_map>
#include "OpenVDBIncludes.h"

const static openvdb::Int32 UNVISITED_VERTEX_INDEX = -1;
enum VolumeStyle { VOLUME_STYLE_CUBE };
const static uint32_t VOLUME_STYLE_COUNT = VOLUME_STYLE_CUBE + 1;
enum CubeVertex { VX0, VX1, VX2, VX3, VX4, VX5, VX6, VX7, VX8 };
const static uint32_t CUBE_VERTEX_COUNT = VX8+1;

class OvdbPrimitiveCube
{
public:
	OvdbPrimitiveCube(const openvdb::Coord &cubeStart)
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

	void setVertexIndex(CubeVertex v, openvdb::Int32 i) { primitiveIndices[v] = i; }
	openvdb::Coord& getCoord(CubeVertex v) { return primitiveVertices[v]; }
	openvdb::Int32 getIndex(CubeVertex v) { return primitiveIndices[v];  }
	//Add the vertex indices in counterclockwise order on each quad face
	openvdb::Vec4I getQuadXY0() { return openvdb::Vec4I(primitiveIndices[VX3], primitiveIndices[VX4], primitiveIndices[VX7], primitiveIndices[VX5]); }
	openvdb::Vec4I getQuadXY1() { return openvdb::Vec4I(primitiveIndices[VX6], primitiveIndices[VX2], primitiveIndices[VX0], primitiveIndices[VX1]); }
	openvdb::Vec4I getQuadXZ0() { return openvdb::Vec4I(primitiveIndices[VX7], primitiveIndices[VX4], primitiveIndices[VX2], primitiveIndices[VX6]); }
	openvdb::Vec4I getQuadXZ1() { return openvdb::Vec4I(primitiveIndices[VX5], primitiveIndices[VX1], primitiveIndices[VX0], primitiveIndices[VX3]); }
	openvdb::Vec4I getQuadYZ0() { return openvdb::Vec4I(primitiveIndices[VX7], primitiveIndices[VX6], primitiveIndices[VX1], primitiveIndices[VX5]); }
	openvdb::Vec4I getQuadYZ1() { return openvdb::Vec4I(primitiveIndices[VX0], primitiveIndices[VX2], primitiveIndices[VX4], primitiveIndices[VX3]); }

private:
	openvdb::Coord primitiveVertices[CUBE_VERTEX_COUNT];
	openvdb::Int32 primitiveIndices[CUBE_VERTEX_COUNT];
};

template<typename _TreeType> class OvdbVoxelVolume
{
public:
	typedef typename _TreeType TreeType;
	typedef typename TreeType::Ptr TreeTypePtr;
	typedef typename TreeType::ConstPtr TreeTypeCPtr;
	typedef typename openvdb::Grid<TreeType> GridType;
	typedef typename GridType::Ptr GridTypePtr;
	typedef typename GridType::ConstPtr GridTypeCPtr;
	typedef typename std::map<IDType, TreeTypePtr> RegionTreesType;
	typedef typename std::vector<QuadVertexType> VolumeVerticesType;
	typedef typename std::vector<PolygonIndicesType> VolumePolygonsType;
	typedef typename std::vector<QuadVertexType> VolumeNormalsType;
	typedef typename std::unordered_map<OvdbQuadKey, OvdbQuad, OvdbQuadHash> UniqueQuadsType;
	typedef typename std::map<IDType, VolumeVerticesType> RegionVerticesType;
	typedef typename std::map<IDType, VolumePolygonsType> RegionPolygonsType;
	typedef typename std::map<IDType, VolumeNormalsType> RegionNormalsType;
	typedef typename std::map<IDType, UniqueQuadsType> RegionQuadsType;
	typedef typename GridType::ValueAllCIter GridValueAllCIterType;
	typedef typename GridType::ValueAllIter GridValueAllIterType;
	typedef typename GridType::ValueOnCIter GridValueOnCIterType;
	typedef typename GridType::ValueOnIter GridValueOnIterType;
	typedef typename GridType::ValueOffCIter GridValueOffCIterType;	
	typedef typename GridType::ValueOffIter GridValueOffIterType;
	typedef typename RegionVerticesType::const_iterator RegionVerticesCIterType;
	typedef typename RegionVerticesType::iterator RegionVerticesIterType;
	typedef typename RegionPolygonsType::const_iterator RegionPolygonsCIterType;
	typedef typename RegionPolygonsType::iterator RegionPolygonsIterType;
	typedef typename RegionNormalsType::const_iterator RegionNormalsCIterType;
	typedef typename RegionNormalsType::iterator RegionNormalsIterType;
	typedef typename RegionTreesType::const_iterator RegionCIterType;
	typedef typename RegionTreesType::iterator RegionIterType;
	typedef typename VolumeVerticesType::const_iterator VolumeVerticesCIterType;
	typedef typename VolumeNormalsType::iterator VolumeVerticesIterType;
	typedef typename VolumePolygonsType::const_iterator VolumePolygonsCIterType;
	typedef typename VolumePolygonsType::iterator VolumePolygonsIterType;
	typedef typename VolumeNormalsType::const_iterator VolumeNormalsCIterType;
	typedef typename VolumeNormalsType::iterator VolumeNormalsIterType;

	//Note: Grids don't have an end value. Just need to check if the iter is null
	TreeTypeCPtr getCRegionTree(const IDType &regionID) const { RegionCIterType i = regionTrees.find(regionID); assert(i != getRegionsCEnd()); return i->second; }
	TreeTypePtr getRegionTree(const IDType &regionID) { RegionIterType i = regionTrees.find(regionID); assert(i != getRegionsEnd()); return i->second; }
	RegionCIterType getRegionsCBegin() const { return regionTrees.cbegin(); }
	RegionCIterType getRegionsCEnd() const { return regionTrees.cend(); }
	RegionIterType getRegionsBegin() { return regionTrees.begin(); }
	RegionIterType getRegionsEnd() { return regionTrees.end(); }
	GridValueAllCIterType valuesAllCBegin(const IDType &regionID) const { return getCRegionTree(regionID)->cbeginValueAll(); }
	GridValueAllIterType valuesAllBegin(const IDType &regionID) { return getRegionTree(regionID)->beginValueAll(); }
	GridValueOnCIterType valuesOnCBegin(const IDType &regionID) const { return getCRegionTree(regionID)->cbeginValueOn(); }
	GridValueOnIterType valuesOnBegin(const IDType &regionID) { return getRegionTree(regionID)->beginValueOn(); }
	GridValueOffCIterType valuesOffCBegin(const IDType &regionID) const { return getCRegionTree(regionID)->cbeginValueOff(); }
	GridValueOffIterType valuesOffBegin(const IDType &regionID) { return getRegionTree(regionID)->beginValueOff(); }

	RegionVerticesCIterType getCVertices(const IDType &regionID) const { RegionVerticesCIterType i = regionVertices.find(regionID); assert(i != regionVertices.cend()); return i; }
	RegionVerticesIterType getVertices(const IDType &regionID) { RegionVerticesIterType i = regionVertices.find(regionID); assert(i != regionVertices.end()); return i; }
	VolumeVerticesCIterType verticesCBegin(const IDType &regionID) const { return getCVertices(regionID)->second.cbegin(); }
	VolumeVerticesCIterType verticesCEnd(const IDType &regionID) const { return getCVertices(regionID)->second.cend(); }
	VolumeVerticesIterType verticesBegin(const IDType &regionID) { return getVertices(regionID)->second.begin(); }
	VolumeVerticesIterType verticesEnd(const IDType &regionID) { return getVertices(regionID)->second.end(); }
	size_t verticesCount(const IDType &regionID) { return getVertices(regionID)->second.size(); }
	void insertVertex(const IDType &regionID, const QuadVertexType &vertex) { RegionVerticesIterType i = regionVertices.find(regionID); assert(i != regionVertices.end()); i->second.push_back(vertex); }

	RegionPolygonsCIterType getCPolygons(const IDType &regionID) const { RegionPolygonsCIterType i = regionPolygons.find(regionID); assert(i != regionPolygons.cend()); return i; }
	RegionPolygonsIterType getPolygons(const IDType &regionID) { RegionPolygonsIterType i = regionPolygons.find(regionID); assert(i != regionPolygons.end()); return i; }
	VolumePolygonsCIterType polygonsCBegin(const IDType &regionID) const { return getCPolygons(regionID)->second.cbegin(); }
	VolumePolygonsCIterType polygonsCEnd(const IDType &regionID) const { return getCPolygons(regionID)->second.cend(); }
	VolumePolygonsIterType polygonsBegin(const IDType &regionID) { return getPolygons(regionID)->second.begin(); }
	VolumePolygonsIterType polygonsEnd(const IDType &regionID) { return getPolygons(regionID)->second.end(); }

	RegionNormalsCIterType getCNormals(const IDType &regionID) const { RegionNormalsCIterType i = regionNormals.find(regionID); assert(i != regionNormals.cend()); return i; }
	RegionNormalsIterType getNormals(const IDType &regionID) { RegionNormalsIterType i = regionNormals.find(regionID); assert(i != regionNormals.end()); return i; }
	VolumeNormalsCIterType normalsCBegin(const IDType &regionID) const { return getCNormals(regionID)->second.cbegin(); }
	VolumeNormalsCIterType normalsCEnd(const IDType &regionID) const { return getCNormals(regionID)->second.cend(); }
	VolumeNormalsIterType normalsBegin(const IDType &regionID) { return getNormals(regionID)->second.begin(); }
	VolumeNormalsIterType normalsEnd(const IDType &regionID) { return getNormals(regionID)->second.end(); }

	const GridTypeCPtr volumeGrid;

	//OvdbVoxelVolume() : volumeGrid(nullptr), visitedVertexIndices(nullptr) {};
	OvdbVoxelVolume(GridTypeCPtr grid) : volumeGrid(grid) {}

	//Copy the volume and visited grid with shared trees and deep-copy the vertices, polygons, and normals
	OvdbVoxelVolume(const OvdbVoxelVolume &rhs) : volumeGrid(rhs.volumeGrid), visitedVertexIndices(rhs.visitedVertexIndices)
	{
		//Copy region trees and geometry. TODO: Safe to clear some of the held data such as tree ptrs?
		regionTrees.clear();
		regionVertices.clear();
		regionPolygons.clear();
		regionNormals.clear();
		for (int i = 0; i < CUBE_FACE_COUNT; ++i)
		{
			regionQuads[i].clear();
		}

		for (RegionCIterType i = rhs.getRegionsCBegin(); i != rhs.getRegionsCEnd(); ++i)
		{
			//Note: For now, don't copy the quads. Will just have to mesh again
			const IDType &regionID = i->first;
			openvdb::tree::TreeBase::Ptr treePtr = i->second;
			regionTrees[regionID] = boost::static_pointer_cast<TreeType>(treePtr->copy());
			regionVertices[regionID] = VolumeVerticesType();
			regionPolygons[regionID] = VolumePolygonsType();
			regionNormals[regionID] = VolumeNormalsType();
			std::copy(rhs.verticesCBegin(regionID), rhs.verticesCEnd(regionID), *regionVertices.find(regionID));
			std::copy(rhs.polygonsCBegin(regionID), rhs.polygonsCEnd(regionID), *regionPolygons.find(regionID));
			std::copy(rhs.normalsCBegin(regionID), rhs.normalsCEnd(regionID), *regionNormals.find(regionID));
		}
	}

	void addRegion(const IDType &regionID, const openvdb::CoordBBox &regionBBox)
	{
		//Tree clipping is destructive so copy the tree prior to clipping in order to preserve the original tree
		//TODO: Possibly check if the region ID already exists and if so, handle a changed bounding box (or throw an error)
		TreeTypePtr subtreePtr = volumeGrid->tree().copy();
		subtreePtr->clip(regionBBox);
		regionTrees[regionID] = subtreePtr;
		regionVertices.insert(regionID, VolumeVerticesType());
		regionPolygons.insert(regionID, VolumePolygonsType());
		regionNormals.insert(regionID, VolumeNormalsType());
		for (int i = 0; i < CUBE_FACE_COUNT; ++i)
		{
			regionQuads[i].insert(regionID, UniqueQuadsType());
		}
		//The visited vertex grid mirrors the volume grid, where the value of each voxel is the vertex index or -1 if that voxel has not been visited
		auto i = visitedVertexIndices.insert(regionID, openvdb::Int32Grid::create(UNVISITED_VERTEX_INDEX));
		i->second->setTransform(volumeGrid->transformPtr()->copy());
		i->second->topologyUnion(*subtreePtr);
	}
	
	void buildVolume(const IDType &regionID, VolumeStyle volumeStyle, float surfaceValue)
	{
		//Step through only voxels that are on
		for (GridValueOnCIterType i = valuesOnCBegin(regionID); i; ++i)
		{
			const openvdb::Coord &startCoord = i.getCoord();
			//Skip tile values and values that are not on the surface
			if (!i.isVoxelValue())
			{
				continue;
			}
			float value = getRegionGridValue(regionID, startCoord);
			if (!openvdb::math::isApproxEqual(value, surfaceValue))
			{
				continue;
			}

			//Set up the 6 quads
			if (volumeStyle == VOLUME_STYLE_CUBE)
			{
				OvdbPrimitiveCube primitiveIndices(startCoord);
				buildCubeQuads(regionID, primitiveIndices);
				const VolumeVerticesType *vertices = &(getVertices(regionID)->second);
				OvdbQuad xy0(vertices, primitiveIndices.getQuadXY0(), XY_FACE, 1);
				OvdbQuad xy1(vertices, primitiveIndices.getQuadXY1(), XY_FACE, -1);
				OvdbQuad xz0(vertices, primitiveIndices.getQuadXZ0(), XZ_FACE, 1);
				OvdbQuad xz1(vertices, primitiveIndices.getQuadXZ1(), XZ_FACE, -1);
				OvdbQuad yz0(vertices, primitiveIndices.getQuadYZ0(), YZ_FACE, 1);
				OvdbQuad yz1(vertices, primitiveIndices.getQuadYZ1(), YZ_FACE, -1);
				regionQuads[XY_FACE].find(regionID)->second[xy0.getKey()] = xy0;
				regionQuads[XY_FACE].find(regionID)->second[xy1.getKey()] = xy1;
				regionQuads[XZ_FACE].find(regionID)->second[xz0.getKey()] = xz0;
				regionQuads[XZ_FACE].find(regionID)->second[xz1.getKey()] = xz1;
				regionQuads[YZ_FACE].find(regionID)->second[yz0.getKey()] = yz0;
				regionQuads[YZ_FACE].find(regionID)->second[yz1.getKey()] = yz1;
			}
		}
	}

	void doMesh(const IDType &regionID, OvdbMeshMethod method)
	{
		//Collect the quads in a linear list and mesh them. TODO: Better way to do this than copying them all?
		std::vector<OvdbQuad*> quads;
		for (int i = 0; i < CUBE_FACE_COUNT; ++i)
		{
			UniqueQuadsType &unorderedQuads = regionQuads[i].find(regionID)->second;
			for (UniqueQuadsType::iterator j = unorderedQuads.begin(); j != unorderedQuads.end(); ++j)
			{
				quads.push_back(&(j->second));
			}
		}

		//If method is naive do nothing special
		if (method == MESHING_GREEDY)
		{
			//Merge adjacent quads in a greedy manner
			for (std::vector<OvdbQuad*>::iterator i = quads.begin(); i != quads.end(); ++i)
			{
				auto j = i;
				++j;
				for (; j != quads.end() && ((*i)->mergeQuadsByLength(**j) || (*i)->mergeQuadsByWidth(**j)); ++j);
			}
		}

		//Finally, collect polygon and normal data
		for (std::vector<OvdbQuad*>::const_iterator i = quads.begin(); i != quads.end(); ++i)
		{
			const OvdbQuad &q = **i;
			if (q.quadIsMerged())
			{
				continue;
			}
			//Collect triangle indices of the two triangles comprising this quad
			VolumePolygonsType &polys = getPolygons(regionID)->second;
			VolumeNormalsType &norms = getNormals(regionID)->second;
			polys.push_back(PolygonIndicesType(q(V0), q(V1), q(V2)));
			polys.push_back(PolygonIndicesType(q(V0), q(V2), q(V3)));
			norms.push_back(q.vertexNormal(V0));
			norms.push_back(q.vertexNormal(V1));
			norms.push_back(q.vertexNormal(V2));
			norms.push_back(q.vertexNormal(V3));
		}
	}

private:
	RegionTreesType regionTrees;
	RegionVerticesType regionVertices;
	RegionPolygonsType regionPolygons;
	RegionNormalsType regionNormals;
	RegionQuadsType regionQuads[CUBE_FACE_COUNT]; //TODO: Set up iterator and get/set functions for regionQuads
	std::map<IDType, openvdb::Int32Grid::Ptr> visitedVertexIndices;

	float getGridValue(const openvdb::Coord &coord) { return volumeGrid->getConstAccessor().getValue(coord); }
	float getRegionGridValue(const IDType &regionID, const openvdb::Coord &coord) { return getRegionTree(regionID)->getValue(coord); }
	openvdb::Int32 getVisitedVertexValue(const IDType &regionID, const openvdb::Coord &coord) { return visitedVertexIndices.find(regionID)->second->getConstAccessor().getValue(coord); }
	void setVisitedVertexValue(const IDType &regionID, const openvdb::Coord &coord, openvdb::Int32 value) { visitedVertexIndices.find(regionID)->second->getAccessor().setValue(coord, value); }

	openvdb::Int32 addRegionVertex(const IDType &regionID, const openvdb::Coord &coord)
	{
		openvdb::Int32 vertexIndex = getVisitedVertexValue(regionID, coord);
		if (vertexIndex == UNVISITED_VERTEX_INDEX)
		{
			vertexIndex = openvdb::Int32(verticesCount(regionID)); //TODO: Error check index ranges
			TreeTypeCPtr region = getCRegionTree(regionID);
			QuadVertexType vertex = volumeGrid->indexToWorld(coord);
			insertVertex(regionID, vertex);
			//Since this is a new vertex save it to the global visited vertex grid for use by any other voxels in the same region that share it
			setVisitedVertexValue(regionID, coord, vertexIndex);
		}
		return vertexIndex;
	}

	void buildCubeQuads(const IDType &regionID, OvdbPrimitiveCube &primitiveIndices)
	{
		//Make 6 quads, each of width / height 1
		for (uint32_t i = 0; i < CUBE_VERTEX_COUNT; ++i)
		{
			primitiveIndices.setVertexIndex((CubeVertex)i, addRegionVertex(regionID, primitiveIndices.getCoord((CubeVertex)i)));
		}
	}
};