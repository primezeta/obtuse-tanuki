#include "libovdb.h"
#include <openvdb/openvdb.h>
#include <openvdb/Exceptions.h>
#include <openvdb/tools/Dense.h>
#include <openvdb/tools/VolumeToMesh.h>

typedef bool TreeDataType;
typedef openvdb::BoolGrid GridDataType;
typedef openvdb::math::Vec3s VertexType;
typedef openvdb::Vec3I TriangleType;
typedef openvdb::Vec4I QuadType;

//static GridDataType::Ptr SparseGrids = nullptr;
static openvdb::GridPtrVec Grids;
static std::vector<VertexType> Vertices;
static std::vector<TriangleType> Triangles;
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
			std::string s;
			for (openvdb::GridPtrVecIter i = file.getGrids()->begin(); i < file.getGrids()->end(); i++)
			{
				GridDataType::Ptr grid = openvdb::gridPtrCast<GridDataType>(*i);
				s = grid->getName();
				//GridDataType::Ptr grid = openvdb::gridPtrCast<GridDataType>(file.readGrid(i->get()->getName()));
				//Grids.push_back(grid->copyGrid());
			}
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
		//openvdb::tools::volumeToMesh<GridDataType>(*SparseGrid, Vertices, Triangles, Quads, isovalue, adaptivity);
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

int OvdbGetNextMeshTriangle(uint32_t &tx, uint32_t &ty, uint32_t &tz)
{
	int error = 0;
	try
	{
		if (Triangles.empty())
		{
			return 0;
		}
		TriangleType ts = Triangles.back();
		Triangles.pop_back();
		tx = ts.x();
		ty = ts.y();
		tz = ts.z();
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