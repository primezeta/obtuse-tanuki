#include "libovdb.h"
#include "OvdbQuad.h"
#include "OpenVDBIncludes.h"

const static openvdb::Int32 UNVISITED_VERTEX_INDEX = -1;

LIB_OVDB_API template<typename _TreeType> class OvdbVoxelVolume
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
	std::set<OvdbQuad, cmpByQuad> uniqueQuads[CUBE_FACE_COUNT];

	GridTypeConstPtr volumeGrid;
	openvdb::Int32Grid::Ptr visitedVertexIndices;

	float getGridValue(openvdb::Coord &coord) { return volumeGrid->getConstAccessor().getValue(coord); }
	openvdb::Int32 getVisitedVertexValue(const openvdb::Coord &coord) { return visitedVertexIndices->getConstAccessor().getValue(coord); }
	void setVisitedVertexValue(const openvdb::Coord &coord, openvdb::Int32 value) { visitedVertexIndices->getAccessor().setValue(coord, value); }
	void buildQuads(const openvdb::CoordBBox &bbox, std::vector<QuadIndicesType> &quadPrimitives)
	{
		//Make 6 quads, each of width / height 1
		std::vector<openvdb::Coord> primitiveVertices;
		openvdb::CoordBBox prim = bbox.createCube(bbox.min(), 1);
		primitiveVertices.push_back(prim.getStart());
		primitiveVertices.push_back(prim.getStart().offsetBy(1, 0, 0));
		primitiveVertices.push_back(prim.getStart().offsetBy(0, 1, 0));
		primitiveVertices.push_back(prim.getStart().offsetBy(0, 0, 1));
		primitiveVertices.push_back(prim.getEnd().offsetBy(-1, 0, 0));
		primitiveVertices.push_back(prim.getEnd().offsetBy(0, -1, 0));
		primitiveVertices.push_back(prim.getEnd().offsetBy(0, 0, -1));
		primitiveVertices.push_back(prim.getEnd());

		std::vector<openvdb::Int32> primitiveIndices;
		for (std::vector<openvdb::Coord>::const_iterator i = primitiveVertices.cbegin(); i != primitiveVertices.end(); ++i)
		{
			openvdb::Int32 vertexIndex = getVisitedVertexValue(*i);
			if (vertexIndex == UNVISITED_VERTEX_INDEX)
			{
				//This is a new vertex. Save it to the visited vertex grid for use by any other voxels that share it
				volumeVertices.push_back(volumeGrid->indexToWorld(*i));
				vertexIndex = openvdb::Int32(volumeVertices.size() - 1); //TODO: Error check index ranges
				setVisitedVertexValue(*i, vertexIndex);
			}
			primitiveIndices.push_back(vertexIndex);
		}

		//Add the vertex indices in counterclockwise order on each quad face
		quadPrimitives.push_back(openvdb::Vec4I(primitiveIndices[3], primitiveIndices[4], primitiveIndices[7], primitiveIndices[5]));
		quadPrimitives.push_back(openvdb::Vec4I(primitiveIndices[6], primitiveIndices[2], primitiveIndices[0], primitiveIndices[1]));
		quadPrimitives.push_back(openvdb::Vec4I(primitiveIndices[7], primitiveIndices[4], primitiveIndices[2], primitiveIndices[6]));
		quadPrimitives.push_back(openvdb::Vec4I(primitiveIndices[5], primitiveIndices[1], primitiveIndices[0], primitiveIndices[3]));
		quadPrimitives.push_back(openvdb::Vec4I(primitiveIndices[7], primitiveIndices[6], primitiveIndices[1], primitiveIndices[5]));
		quadPrimitives.push_back(openvdb::Vec4I(primitiveIndices[0], primitiveIndices[2], primitiveIndices[4], primitiveIndices[3]));
	}

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
	
	void buildVolume(const openvdb::CoordBBox &bbox, float surfaceValue)
	{
		//Step through only voxels that are on
		for (GridTypeValueOnCIter i = volumeGrid->cbeginValueOn(); i; ++i)
		{
			//Skip tile values and values that are not on the surface
			if (!i.isVoxelValue() ||
				!openvdb::math::isApproxEqual(getGridValue(i.getCoord()), surfaceValue))
			{
				continue;
			}

			//Set up the 6 quads
			std::vector<QuadIndicesType> quadPrimitives;
			buildQuads(bbox, quadPrimitives);

			for (int j = 0; j < CUBE_FACE_COUNT; ++j)
			{
				//Insert into the quad set to set up the total ordering when we later retrieve the quads with iterators;
				OvdbQuad q(volumeVertices, quadPrimitives[j], (CubeFace)j);
				uniqueQuads[j].insert(q);
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
		for (std::vector<OvdbQuad>::iterator i = quads.begin(); i != quads.end(); ++i)
		{
			OvdbQuad &q = *i;
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