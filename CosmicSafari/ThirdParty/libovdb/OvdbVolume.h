#include "libovdb.h"
#include "OvdbQuad.h"

const static openvdb::Int32 UNVISITED_VERTEX_INDEX = -1;

template<typename _TreeType> class OvdbVoxelVolume
{
private:
	typedef typename _TreeType TreeType;
	typedef typename openvdb::Grid<TreeType> GridType;
	typedef typename GridType::Ptr GridTypePtr;
	typedef typename GridType::ConstPtr GridTypeConstPtr;
	typedef typename GridType::ValueOnIter GridTypeValueOnIter;
	typedef typename GridType::ValueOnCIter GridTypeValueOnCIter;

	std::vector<QuadVertexType> volumeVertices;
	std::vector<PolygonIndicesType> polygonIndices;
	std::vector<QuadVertexType> vertexNormals;
	std::set<OvdbQuad, cmpByQuad> uniqueQuads;

	GridTypeConstPtr volumeGrid;
	openvdb::Int32Grid::Ptr visitedVertexIndices;

	TreeType getGridValue(openvdb::Coord &coord) { return volume->getConstAccessor()->getValue(coord); }
	openvdb::Int32 getVisitedVertexValue(openvdb::Coord &coord) { return visitedVertexIndices->getAccessor().getValue(coord); }
	void setVisitedVertexValue(openvdb::Coord &coord, openvdb::Int32 value) { visitedVertexIndices->getAccessor().setValue(coord, value); }
	void buildQuads(const openvdb::CoordBBox &bbox, std::vector<openvdb::Vec4I> &quadPrimitives);

public:
	OvdbVoxelVolume(GridTypeConstPtr grid) : volumeGrid(grid)
	{
		//The visited vertex grid mirrors the grid, where the value of each voxel is the vertex index or -1 if that voxel has not been visited
		visitedVertexIndices = openvdb::Int32Grid::create(UNVISITED_VERTEX_INDEX);
		visitedVertexIndices->setTransform(grid->transformPtr()->copy());
		visitedVertexIndices->topologyUnion(*grid);
	}
	OvdbVoxelVolume(const OvdbVoxelVolume &rhs)
	{
		//If we copy a voxel volume then meshing will have to start over
		volumeGrid = rhs.volumeGrid;
		visitedVertexIndices = rhs.visitedVertexIndices;
	}
	std::vector<QuadVertexType>::const_iterator verticesCBegin() { return volumeVertices.cbegin(); }
	std::vector<QuadVertexType>::const_iterator verticesCEnd() { return volumeVertices.cend(); }
	std::vector<PolygonIndicesType>::const_iterator polygonsCBegin() { return polygonIndices.cbegin(); }
	std::vector<PolygonIndicesType>::const_iterator polygonsCEnd() { return polygonIndices.cend(); }
	std::vector<QuadVertexType>::const_iterator normalsCBegin() { return vertexNormals.cbegin(); }
	std::vector<QuadVertexType>::const_iterator normalsCEnd() { return vertexNormals.cend(); }
	void buildVolume(const openvdb::CoordBBox &bbox, float surfaceValue);
	void doMesh(OvdbMeshMethod method);
};