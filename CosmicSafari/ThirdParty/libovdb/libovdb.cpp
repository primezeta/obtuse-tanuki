#include "libovdb.h"
#include <openvdb/openvdb.h>
#include <openvdb/Exceptions.h>
#include <openvdb/tools/Dense.h>
#include <openvdb/tools/VolumeToMesh.h>

typedef openvdb::FloatGrid GridDataType;
typedef GridDataType::TreeType TreeDataType;
typedef openvdb::math::Vec3s VertexType;
typedef openvdb::Vec3d PointType;
typedef openvdb::Vec4I QuadType;

enum PolyVertices { STARTCORNERS, MXMYMZ = STARTCORNERS, NXNYNZ, MXMYNZ, MXNYMZ, NXMYMZ, MXNYNZ, NXMYNZ, NXNYMZ, NUMCORNERS };
struct Vertex
{
	PointType v;
	openvdb::Index32 i;
};
struct CubeFaces
{
	Vertex f1[NUMCORNERS];
	Vertex f2[NUMCORNERS];
	Vertex f3[NUMCORNERS];
	Vertex f4[NUMCORNERS];
	Vertex f5[NUMCORNERS];
	Vertex f6[NUMCORNERS];
};

static GridDataType::Ptr SparseGrids = nullptr;
static openvdb::GridPtrVec Grids;
static std::vector<VertexType> Vertices;
static std::vector<openvdb::Index32> Triangles;
static std::vector<QuadType> Quads;

int OvdbInitialize()
{
	int error = 0;
	try
	{
		openvdb::initialize();
	}
	catch (openvdb::Exception &e)
	{
		//TODO: Log openvdb exception messages to file
		//errorMsg = e.what();
		error = 1;
	}
	return error;
}

int OvdbUninitialize()
{
	int error = 0;
	try
	{
		openvdb::uninitialize();
	}
	catch (openvdb::Exception &e)
	{
		//TODO: Log openvdb exception messages to file
		//errorMsg = e.what();
		error = 1;
	}
	return error;
}

int OvdbLoadVdb(const std::string &filename)
{
	int error = 0;
	try
	{
		openvdb::io::File file(filename);
		file.open();
		if (file.getSize() > 0)
		{
			//std::string s;
			//for (openvdb::GridPtrVecIter i = file.getGrids()->begin(); i < file.getGrids()->end(); i++)
			//{
			//	//GridDataType::Ptr grid = openvdb::gridPtrCast<GridDataType>(*i);
			//	//s = grid->getName();
			//	SparseGrids = openvdb::gridPtrCast<GridDataType>(file.readGrid(i->get()->getName()));
			//	Grids.push_back(SparseGrids->copyGrid());
			//}
			SparseGrids = openvdb::gridPtrCast<GridDataType>(file.readGrid("noise"));
			Grids.push_back(SparseGrids->copyGrid());
			file.close();
		}
	}
	catch (openvdb::Exception &e)
	{
		//TODO: Log openvdb exception messages to file
		//errorMsg = e.what();
		error = 1;
	}
	return error;
}

int OvdbVolumeToMesh(double isovalue, double adaptivity)
{
	int error = 0;
	try
	{
		//for (GridIterType i = Grids.begin(); i != Grids.end(); i++)
		//{
		//	GridTreeType tree = static_cast<GridTreeType>i->get()->;
		//	openvdb::tools::volumeToMesh<GridDataType>(, Vertices, Triangles, Quads, isovalue, adaptivity);
		//}
		openvdb::CoordBBox bbox;
		openvdb::Index32 vertexIndex = 0;
		for (TreeDataType::NodeCIter i = SparseGrids->tree().cbeginNode(); i; ++i)
		{
			//From openvdb_viewer RenderModules.cc: Nodes are rendered as cell-centered
			const openvdb::Vec3d min(bbox.min().x() - 0.5, bbox.min().y() - 0.5, bbox.min().z() - 0.5);
			const openvdb::Vec3d max(bbox.max().x() + 0.5, bbox.max().y() + 0.5, bbox.max().z() + 0.5);
			i.getBoundingBox(bbox);

			//Get the 8 vertices of the cube
			PointType corners[NUMCORNERS];
			corners[MXMYMZ] = SparseGrids->indexToWorld(max);
			corners[NXNYNZ] = SparseGrids->indexToWorld(min);
			corners[MXMYNZ] = SparseGrids->indexToWorld(PointType(max.x(), max.y(), min.z()));
			corners[MXNYMZ] = SparseGrids->indexToWorld(PointType(max.x(), min.y(), max.z()));
			corners[NXMYMZ] = SparseGrids->indexToWorld(PointType(min.x(), max.y(), max.z()));
			corners[MXNYNZ] = SparseGrids->indexToWorld(PointType(max.x(), min.y(), min.z()));
			corners[NXMYNZ] = SparseGrids->indexToWorld(PointType(min.x(), max.y(), min.z()));
			corners[NXNYMZ] = SparseGrids->indexToWorld(PointType(min.x(), min.y(), max.z()));
			
			CubeFaces cubefaces;
			cubefaces.f1[NXNYMZ].v = corners[NXNYMZ];
			cubefaces.f1[NXNYMZ].i = vertexIndex;
			cubefaces.f4[NXNYMZ].v = corners[NXNYMZ];
			cubefaces.f4[NXNYMZ].i = vertexIndex;
			cubefaces.f5[NXNYMZ].v = corners[NXNYMZ];
			cubefaces.f5[NXNYMZ].i = vertexIndex;
			Vertices.push_back(corners[NXNYMZ]);

			vertexIndex++;
			cubefaces.f1[NXNYNZ].v = corners[NXNYNZ];
			cubefaces.f1[NXNYNZ].i = vertexIndex;
			cubefaces.f4[NXNYNZ].v = corners[NXNYNZ];
			cubefaces.f4[NXNYNZ].i = vertexIndex;
			cubefaces.f6[NXNYNZ].v = corners[NXNYNZ];
			cubefaces.f6[NXNYNZ].i = vertexIndex;
			Vertices.push_back(corners[NXNYNZ]);

			vertexIndex++;
			cubefaces.f1[MXNYNZ].v = corners[MXNYNZ];
			cubefaces.f1[MXNYNZ].i = vertexIndex;
			cubefaces.f2[MXNYNZ].v = corners[MXNYNZ];
			cubefaces.f2[MXNYNZ].i = vertexIndex;
			cubefaces.f6[MXNYNZ].v = corners[MXNYNZ];
			cubefaces.f6[MXNYNZ].i = vertexIndex;
			Vertices.push_back(corners[MXNYNZ]);

			vertexIndex++;
			cubefaces.f1[MXNYMZ].v = corners[MXNYMZ];
			cubefaces.f1[MXNYMZ].i = vertexIndex;
			cubefaces.f2[MXNYMZ].v = corners[MXNYMZ];
			cubefaces.f2[MXNYMZ].i = vertexIndex;
			cubefaces.f5[MXNYMZ].v = corners[MXNYMZ];
			cubefaces.f5[MXNYMZ].i = vertexIndex;
			Vertices.push_back(corners[MXNYMZ]);

			vertexIndex++;
			cubefaces.f2[MXMYNZ].v = corners[MXMYNZ];
			cubefaces.f2[MXMYNZ].i = vertexIndex;
			cubefaces.f3[MXMYNZ].v = corners[MXMYNZ];
			cubefaces.f3[MXMYNZ].i = vertexIndex;
			cubefaces.f6[MXMYNZ].v = corners[MXMYNZ];
			cubefaces.f6[MXMYNZ].i = vertexIndex;
			Vertices.push_back(corners[MXMYNZ]);

			vertexIndex++;
			cubefaces.f2[MXMYMZ].v = corners[MXMYMZ];
			cubefaces.f2[MXMYMZ].i = vertexIndex;
			cubefaces.f3[MXMYMZ].v = corners[MXMYMZ];
			cubefaces.f3[MXMYMZ].i = vertexIndex;
			cubefaces.f5[MXMYMZ].v = corners[MXMYMZ];
			cubefaces.f5[MXMYMZ].i = vertexIndex;
			Vertices.push_back(corners[MXMYMZ]);

			vertexIndex++;
			cubefaces.f3[NXMYNZ].v = corners[NXMYNZ];
			cubefaces.f3[NXMYNZ].i = vertexIndex;
			cubefaces.f4[NXMYNZ].v = corners[NXMYNZ];
			cubefaces.f4[NXMYNZ].i = vertexIndex;
			cubefaces.f6[NXMYNZ].v = corners[NXMYNZ];
			cubefaces.f6[NXMYNZ].i = vertexIndex;
			Vertices.push_back(corners[NXMYNZ]);

			vertexIndex++;
			cubefaces.f3[NXMYMZ].v = corners[NXMYMZ];
			cubefaces.f3[NXMYMZ].i = vertexIndex;
			cubefaces.f4[NXMYMZ].v = corners[NXMYMZ];
			cubefaces.f4[NXMYMZ].i = vertexIndex;
			cubefaces.f5[NXMYMZ].v = corners[NXMYMZ];
			cubefaces.f5[NXMYMZ].i = vertexIndex;
			Vertices.push_back(corners[NXMYMZ]);

			//Get the indices of the two polygons on each face of the cube
			PolyVertices F1[4] = { NXNYMZ, NXNYNZ, MXNYNZ, MXNYMZ };
			PolyVertices F2[4] = { MXNYMZ, MXNYNZ, MXMYNZ, MXMYMZ };
			PolyVertices F3[4] = { MXMYMZ, MXMYNZ, NXMYNZ, NXMYMZ };
			PolyVertices F4[4] = { NXMYMZ, NXMYNZ, NXNYNZ, NXNYMZ };
			PolyVertices F5[4] = { NXMYMZ, NXNYMZ, MXNYMZ, MXMYMZ };
			PolyVertices F6[4] = { NXNYNZ, NXMYNZ, MXMYNZ, MXNYNZ };

			//Face #1
			Triangles.push_back(cubefaces.f1[F1[0]].i);
			Triangles.push_back(cubefaces.f1[F1[1]].i);
			Triangles.push_back(cubefaces.f1[F1[2]].i);
			Triangles.push_back(cubefaces.f1[F1[0]].i);
			Triangles.push_back(cubefaces.f1[F1[2]].i);
			Triangles.push_back(cubefaces.f1[F1[3]].i);
			//Face #2
			Triangles.push_back(cubefaces.f2[F2[0]].i);
			Triangles.push_back(cubefaces.f2[F2[1]].i);
			Triangles.push_back(cubefaces.f2[F2[2]].i);
			Triangles.push_back(cubefaces.f2[F2[0]].i);
			Triangles.push_back(cubefaces.f2[F2[2]].i);
			Triangles.push_back(cubefaces.f2[F2[3]].i);
			//Face #3
			Triangles.push_back(cubefaces.f3[F3[0]].i);
			Triangles.push_back(cubefaces.f3[F3[1]].i);
			Triangles.push_back(cubefaces.f3[F3[2]].i);
			Triangles.push_back(cubefaces.f3[F3[0]].i);
			Triangles.push_back(cubefaces.f3[F3[2]].i);
			Triangles.push_back(cubefaces.f3[F3[3]].i);
			//Face #4
			Triangles.push_back(cubefaces.f4[F4[0]].i);
			Triangles.push_back(cubefaces.f4[F4[1]].i);
			Triangles.push_back(cubefaces.f4[F4[2]].i);
			Triangles.push_back(cubefaces.f4[F4[0]].i);
			Triangles.push_back(cubefaces.f4[F4[2]].i);
			Triangles.push_back(cubefaces.f4[F4[3]].i);
			//Face #5
			Triangles.push_back(cubefaces.f5[F6[0]].i);
			Triangles.push_back(cubefaces.f5[F6[1]].i);
			Triangles.push_back(cubefaces.f5[F6[2]].i);
			Triangles.push_back(cubefaces.f5[F6[0]].i);
			Triangles.push_back(cubefaces.f5[F6[2]].i);
			Triangles.push_back(cubefaces.f5[F6[3]].i);
			//Face #6
			Triangles.push_back(cubefaces.f6[F6[0]].i);
			Triangles.push_back(cubefaces.f6[F6[1]].i);
			Triangles.push_back(cubefaces.f6[F6[2]].i);
			Triangles.push_back(cubefaces.f6[F6[0]].i);
			Triangles.push_back(cubefaces.f6[F6[2]].i);
			Triangles.push_back(cubefaces.f6[F6[3]].i);
		}

		//Reverse the 
	}
	catch (openvdb::Exception &e)
	{
		//TODO: Log openvdb exception messages to file
		//errorMsg = e.what();
		error = 1;
	}
	return error;
}

int OvdbGetNextMeshPoint(float &px, float &py, float &pz)
{
	int error = 0;
	try
	{
		if (Vertices.empty())
		{
			return 0;
		}
		VertexType ps = Vertices.back();
		Vertices.pop_back();
		px = ps.x();
		py = ps.y();
		pz = ps.z();
	}
	catch (openvdb::Exception &e)
	{
		//TODO: Log openvdb exception messages to file
		//errorMsg = e.what();
		error = 1;
	}
	return error;
}

int OvdbGetNextMeshTriangle(uint32_t &vertexIndex)
{
	int error = 0;
	try
	{
		if (Triangles.empty())
		{
			return 0;
		}
		vertexIndex = Triangles.back();
		Triangles.pop_back();
	}
	catch (openvdb::Exception &e)
	{
		//TODO: Log openvdb exception messages to file
		//errorMsg = e.what();
		error = 1;
	}
	return error;
}

int OvdbGetNextMeshQuad(uint32_t &qw, uint32_t &qx, uint32_t &qy, uint32_t &qz)
{
	int error = 0;
	try
	{
		if (Quads.empty())
		{
			return 0;
		}
		QuadType qs = Quads.back();
		Quads.pop_back();
		qw = qs.w();
		qx = qs.x();
		qy = qs.y();
		qz = qs.z();
	}
	catch (openvdb::Exception &e)
	{
		//TODO: Log openvdb exception messages to file
		//errorMsg = e.what();
		error = 1;
	}
	return error;
}