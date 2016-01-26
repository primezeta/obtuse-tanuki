#include "libovdb.h"
#include "OvdbQuad.h"
#include "OpenVDBIncludes.h"

const static openvdb::Int32 UNVISITED_VERTEX_INDEX = -1;
enum VolumeStyle { VOLUME_STYLE_CUBE };
const static uint32_t VOLUME_STYLE_COUNT = VOLUME_STYLE_CUBE + 1;
enum CubeVertex { VX0, VX1, VX2, VX3, VX4, VX5, VX6, VX7, VX8 };
const static uint32_t CUBE_VERTEX_COUNT = VX8+1;

LIB_OVDB_API class OvdbPrimitiveCube
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
	openvdb::Vec4I getQuadYZ1() { return openvdb::Vec4I(primitiveIndices[VX6], primitiveIndices[VX2], primitiveIndices[VX0], primitiveIndices[VX1]); }

private:
	openvdb::Coord primitiveVertices[CUBE_VERTEX_COUNT];
	openvdb::Int32 primitiveIndices[CUBE_VERTEX_COUNT];
};

LIB_OVDB_API template<typename _TreeType> class OvdbVoxelVolume
{
private:
	typedef typename _TreeType TreeType;
	typedef typename openvdb::Grid<TreeType> GridType;
	typedef typename GridType::Ptr GridTypePtr;
	typedef typename GridType::ConstPtr GridTypeConstPtr;
	typedef typename GridType::ValueOnIter GridTypeValueOnIter;
	typedef typename GridType::ValueOnCIter GridTypeValueOnCIter;
	typedef typename std::vector<QuadVertexType> VolumeVertices;
	typedef typename std::vector<PolygonIndicesType> VolumePolygons;
	typedef typename std::vector<QuadVertexType> VolumeNormals;

public:
	typedef typename VolumeVertices::const_iterator VolumeVerticesCIter;
	typedef typename VolumePolygons::const_iterator VolumePolygonsCIter;
	typedef typename VolumeNormals::const_iterator VolumeNormalsCIter;

private:
	VolumeVertices volumeVertices;
	VolumePolygons polygonIndices;
	VolumeNormals vertexNormals;
	std::set<OvdbQuad, cmpByQuad> uniqueQuads[CUBE_FACE_COUNT];
	GridTypeConstPtr volumeGrid;
	openvdb::Int32Grid::Ptr visitedVertexIndices;

	float getGridValue(const openvdb::Coord &coord) { return volumeGrid->getConstAccessor().getValue(coord); }
	openvdb::Int32 getVisitedVertexValue(const openvdb::Coord &coord) { return visitedVertexIndices->getConstAccessor().getValue(coord); }
	void setVisitedVertexValue(const openvdb::Coord &coord, openvdb::Int32 value) { visitedVertexIndices->getAccessor().setValue(coord, value); }
	openvdb::Int32 addVolumeVertex(const openvdb::Coord &coord)
	{
		openvdb::Int32 vertexIndex = getVisitedVertexValue(coord);
		if (vertexIndex == UNVISITED_VERTEX_INDEX)
		{
			//This is a new vertex. Save it to the visited vertex grid for use by any other voxels that share it
			vertexIndex = openvdb::Int32(volumeVertices.size()); //TODO: Error check index ranges
			setVisitedVertexValue(coord, vertexIndex);
			volumeVertices.push_back(volumeGrid->indexToWorld(coord));
		}
		return vertexIndex;
	}

	void buildCubeQuads(OvdbPrimitiveCube &primitiveIndices)
	{
		//Make 6 quads, each of width / height 1
		for (uint32_t i = 0; i < CUBE_VERTEX_COUNT; ++i)
		{
			primitiveIndices.setVertexIndex((CubeVertex)i, addVolumeVertex(primitiveIndices.getCoord((CubeVertex)i)));
		}
	}

public:
	OvdbVoxelVolume() : volumeGrid(nullptr), visitedVertexIndices(nullptr) {};
	OvdbVoxelVolume(GridTypeConstPtr grid) : volumeGrid(grid)
	{
		//The visited vertex grid mirrors the grid, where the value of each voxel is the vertex index or -1 if that voxel has not been visited
		visitedVertexIndices = openvdb::Int32Grid::create(UNVISITED_VERTEX_INDEX);
		visitedVertexIndices->setTransform(grid->transformPtr()->copy());
		visitedVertexIndices->topologyUnion(*grid);
	}

	OvdbVoxelVolume(const OvdbVoxelVolume &rhs)
	{
		//Only copy the smart pointers - for now, if we copy a voxel volume then meshing will have to start over
		volumeGrid = rhs.volumeGrid;
		visitedVertexIndices = rhs.visitedVertexIndices;
	}

	VolumeVerticesCIter verticesCBegin() const { return volumeVertices.cbegin(); }
	VolumeVerticesCIter verticesCEnd() const { return volumeVertices.cend(); }
	VolumePolygonsCIter polygonsCBegin() const { return polygonIndices.cbegin(); }
	VolumePolygonsCIter polygonsCEnd() const { return polygonIndices.cend(); }
	VolumeNormalsCIter normalsCBegin() const { return vertexNormals.cbegin(); }
	VolumeNormalsCIter normalsCEnd() const { return vertexNormals.cend(); }
	
	void buildVolume(VolumeStyle volumeStyle, float surfaceValue)
	{
		//Step through only voxels that are on
		for (GridTypeValueOnCIter i = volumeGrid->cbeginValueOn(); i; ++i)
		{
			const openvdb::Coord &startCoord = i.getCoord();
			//Skip tile values and values that are not on the surface
			if (!i.isVoxelValue() ||
				!openvdb::math::isApproxEqual(getGridValue(startCoord), surfaceValue))
			{
				continue;
			}

			//Set up the 6 quads
			if (volumeStyle == VOLUME_STYLE_CUBE)
			{
				OvdbPrimitiveCube primitiveIndices(startCoord);
				buildCubeQuads(primitiveIndices);
				uniqueQuads[XY_FACE].insert(OvdbQuad(volumeVertices, primitiveIndices.getQuadXY0(), XY_FACE));
				uniqueQuads[XY_FACE].insert(OvdbQuad(volumeVertices, primitiveIndices.getQuadXY1(), XY_FACE));
				uniqueQuads[XZ_FACE].insert(OvdbQuad(volumeVertices, primitiveIndices.getQuadXZ0(), XZ_FACE));
				uniqueQuads[XZ_FACE].insert(OvdbQuad(volumeVertices, primitiveIndices.getQuadXZ1(), XZ_FACE));
				uniqueQuads[YZ_FACE].insert(OvdbQuad(volumeVertices, primitiveIndices.getQuadYZ0(), YZ_FACE));
				uniqueQuads[YZ_FACE].insert(OvdbQuad(volumeVertices, primitiveIndices.getQuadYZ1(), YZ_FACE));
			}
		}
	}

	void doMesh(OvdbMeshMethod method)
	{
		//Collect the quads in a linear list and mesh them. TODO: Better way to do this than copying them all?
		std::vector<OvdbQuad> quads;
		for (int i = 0; i < CUBE_FACE_COUNT; ++i)
		{
			for (std::set<OvdbQuad, cmpByQuad>::iterator j = uniqueQuads[i].cbegin(); j != uniqueQuads[i].end(); ++j)
			{
				quads.push_back(*j);
			}
		}

		//If method is naive do nothing special
		if (method != MESHING_NAIVE)
		{
			//Merge adjacent quads in a greedy manner
			//if (method == MESHING_GREEDY)
			//{

			//}
		}

		uint32_t mergedCount = 0;
		uint32_t vertexIndex = 0;
		for (std::vector<OvdbQuad>::const_iterator i = quads.begin(); i != quads.end(); ++i)
		{
			const OvdbQuad &q = *i;
			if (q.quadIsMerged())
			{
				mergedCount++; //For debugging
				continue;
			}
			//Collect triangle indices of the two triangles comprising this quad
			polygonIndices.push_back(PolygonIndicesType(q(V0), q(V1), q(V3)));
			polygonIndices.push_back(PolygonIndicesType(q(V0), q(V2), q(V3)));
		}
	}
};