// openvdb_create.cpp : Defines the entry point for the console application.
//

#include <string>
#include <openvdb/openvdb.h>
#include <openvdb/Exceptions.h>
#include <openvdb/tools/LevelSetSphere.h>
#include <openvdb/tools/Dense.h>

int main(int argc, char * argv[])
{
	int exitStatus = EXIT_SUCCESS;
	try
	{
		openvdb::initialize();

		std::string gridName = std::string(argv[argc - 1]);
		std::string gridType = argv[1];
		openvdb::Vec3f center(std::stof(argv[2]), std::stof(argv[3]), std::stof(argv[4]));
		openvdb::FloatGrid::Ptr sparseGrid = openvdb::FloatGrid::create();
		sparseGrid->setName(gridName);

		std::ostringstream filename;
		if (gridType == "dense")
		{
			float fillValue = std::stof(argv[5]);
			openvdb::FloatGrid::ValueType tolerance = std::stof(argv[6]);
			filename << gridType << "_x" << center.x() << "_y" << center.y() << "_z" << center.z() << "_f" << fillValue << "_t" << tolerance;

			openvdb::CoordBBox boundingBox(openvdb::Coord(-center.x(), -center.y(), -center.z()), openvdb::Coord(center.x(), center.y(), center.z()));
			openvdb::tools::Dense<float> denseGrid(boundingBox);
			denseGrid.fill(fillValue);
			openvdb::tools::copyFromDense(denseGrid, *sparseGrid, tolerance);
		}
		else if (gridType == "sphere")
		{
			float radius = std::stof(argv[5]);
			float voxelSize = std::stof(argv[6]);
			float levelSetHalfWidth = (float)openvdb::LEVEL_SET_HALF_WIDTH;
			if (argc >= 8)
			{
				levelSetHalfWidth = std::stof(argv[7]);
			}
			filename << gridType << "_x" << center.x() << "_y" << center.y() << "_z" << center.z() << "_r" << radius << "_v" << voxelSize << "_h" << levelSetHalfWidth;
			sparseGrid = openvdb::tools::createLevelSetSphere<openvdb::FloatGrid>(radius, center, voxelSize, levelSetHalfWidth);
		}

		openvdb::GridPtrVec grids;
		grids.push_back(sparseGrid);
		openvdb::io::File file("vdbs/" + filename.str() + ".vdb");
		file.write(grids);
		file.close();
		printf("Created %s", file.filename().c_str());

		openvdb::uninitialize();
	}
	catch (const std::exception& e)
	{
		OPENVDB_LOG_FATAL(e.what());
		exitStatus = EXIT_FAILURE;
	}
	catch (...)
	{
		OPENVDB_LOG_FATAL("Exception caught (unexpected type)");
		std::unexpected();
	}
	return exitStatus;
}