#include "libovdb.h"
#include <openvdb/openvdb.h>
#include <openvdb/Exceptions.h>
#include <openvdb/tools/Dense.h>
#include <openvdb/tools/VolumeToMesh.h>

//using namespace openvdb;
//using namespace tools;

void CreateTestVdb(const std::string &filename)
{
	openvdb::FloatGrid::Ptr sparseGrid = nullptr;
	openvdb::io::File file(filename);
	//file.open();
	//if (file.isOpen())
	//{
	//	sparseGrid = openvdb::gridPtrCast<openvdb::FloatGrid>(file.readGrid("TestGrid"));
	//	//sparseGrid->

	//}
	//else
	//{
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
	//}
	file.close();
}

void OvdbInitialize(const std::string filename)
{
	openvdb::initialize();
	CreateTestVdb(filename);
}

void OvdbUninitialize()
{
	openvdb::uninitialize();
}