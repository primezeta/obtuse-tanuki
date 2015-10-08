#include "libovdb.h"
#include <openvdb/openvdb.h>
#include <openvdb/Exceptions.h>
#include <openvdb/tools/Dense.h>
#include <openvdb/tools/VolumeToMesh.h>

//using namespace openvdb;
//using namespace tools;
static openvdb::FloatGrid::Ptr sparseGrid = nullptr;

void CreateTestVdb(const std::string &filename)
{
	openvdb::io::File file(filename);
	try
	{
		if (file.getSize() > 0)
		{
			file.open();
			sparseGrid = openvdb::gridPtrCast<openvdb::FloatGrid>(file.readGrid("TestGrid"));
			//	//sparseGrid->
		}
		else
		{
			openvdb::tools::Dense<float> denseGrid(openvdb::CoordBBox(openvdb::Coord(), openvdb::Coord(100, 100, 100)), 0.0f);
			float fillValue = 1.0f;
			denseGrid.fill(fillValue);

			sparseGrid = openvdb::FloatGrid::create();
			openvdb::FloatGrid::ValueType tolerance = 0.0f;
			copyFromDense(denseGrid, *sparseGrid, tolerance);
			sparseGrid->setName("TestGrid");

			openvdb::GridPtrVec grids;
			grids.push_back(sparseGrid);
			file.write(grids);
		}
	}
	catch (openvdb::Exception &e)
	{
		throw(e);
	}
	file.close();
}

int OvdbInitialize(std::string errorMsg, const std::string filename)
{
	int error = 0;
	try
	{
		openvdb::initialize();
		CreateTestVdb(filename);
	}
	catch (openvdb::Exception &e)
	{
		errorMsg = e.what();
		error = 1;
	}
	return error;
}

//int OvdbUninitialize(std::string &errorMsg)
//{
//	int error = 0;
//	try
//	{
//		openvdb::uninitialize();
//	}
//	catch (openvdb::Exception &e)
//	{
//		errorMsg = e.what();
//		error = 1;
//	}
//	return error;
//}

void OvdbUninitialize()
{
	sparseGrid->clear();
	openvdb::uninitialize();
}

int OvdbVolumeToMesh(std::string &errorMsg,
	                 std::vector<float> &pxs, std::vector<float> &pys, std::vector<float> &pzs,
					 std::vector<uint32_t> &qxs, std::vector<unsigned int> &qys, std::vector<unsigned int> &qzs, std::vector<unsigned int> &qws)
{
	int error = 0;
	std::vector<openvdb::math::Vec3s> points;
	std::vector<openvdb::Vec4I> quads;

	try
	{
		openvdb::tools::volumeToMesh<openvdb::FloatGrid>(*sparseGrid, points, quads, 50000.0);
	}
	catch (openvdb::Exception e)
	{
		errorMsg = e.what();
		error = 1;
	}

	for (std::vector<openvdb::math::Vec3s>::iterator i = points.begin(); i != points.end(); i++)
	{
		pxs.push_back(i->x());
		pys.push_back(i->y());
		pzs.push_back(i->z());
	}
	for (std::vector<openvdb::Vec4I>::iterator i = quads.begin(); i != quads.end(); i++)
	{
		qxs.push_back(i->x());
		qys.push_back(i->y());
		qzs.push_back(i->z());
		qws.push_back(i->w());
	}
	return error;
}