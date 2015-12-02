#include "libovdb.h"
#include <openvdb/openvdb.h>
#include <openvdb/Exceptions.h>
#include <openvdb/tools/Dense.h>
#include <openvdb/tools/VolumeToMesh.h>
#include <fstream>
#include <map>

typedef openvdb::FloatGrid::TreeType TreeDataType;
typedef openvdb::math::Vec3s VertexType;
typedef openvdb::Vec3d PointType;
typedef openvdb::Vec4I QuadType;
typedef openvdb::Index32 IndexType;

enum PolyIndices { MXMYMZ, NXNYNZ, MXMYNZ, MXNYMZ, NXMYMZ, MXNYNZ, NXMYNZ, NXNYMZ, NUMCORNERS = NXNYMZ };

static openvdb::FloatGrid::Ptr SparseGrids = nullptr;
static std::vector<VertexType> Vertices;
static std::vector<IndexType> Triangles;
static std::vector<QuadType> Quads;

PointType CubeVertex(PolyIndices corner, const PointType &boundsMin, const PointType &boundsMax);
std::string gridNamesList(const openvdb::io::File &file);

int OvdbInitialize()
{
	int error = 0;
	try
	{
		openvdb::initialize();
	}
	catch (openvdb::Exception &e)
	{
		std::ofstream logfile;
		logfile.open("C:\\Users\\zach\\Documents\\Unreal Projects\\obtuse-tanuki\\CosmicSafari\\ThirdParty\\exceptions.log");
		logfile << "OvdbInitialize: " << e.what() << std::endl;
		logfile.close();
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
		std::ofstream logfile;
		logfile.open("C:\\Users\\zach\\Documents\\Unreal Projects\\obtuse-tanuki\\CosmicSafari\\ThirdParty\\exceptions.log");
		logfile << "OvdbUninitialize: " << e.what() << std::endl;
		logfile.close();
		error = 1;
	}
	return error;
}

int OvdbLoadVdb(const std::string &filename, const std::string &gridName)
{
	int error = 0;
	try
	{
		openvdb::GridBase::Ptr baseGrid = nullptr;
		std::string gridNames;

		openvdb::io::File file(filename);
		file.open();		
		if (file.getSize() > 0)
		{
			baseGrid = file.readGrid(gridName);
			gridNames = gridNamesList(file);
		}
		else
		{
			OPENVDB_THROW(openvdb::IoError, "Could not read " + filename);
		}
		file.close();

		if (baseGrid == nullptr)
		{
			OPENVDB_THROW(openvdb::RuntimeError, "Failed to read grid \"" + gridName + "\" from " + filename + ". Valid grid names are: " + gridNames);
		}

		SparseGrids = openvdb::gridPtrCast<openvdb::FloatGrid>(baseGrid);

		if (SparseGrids == nullptr)
		{
			OPENVDB_THROW(openvdb::RuntimeError, "Failed to cast grid \"" + gridName + "\". Valid grid names are: " + gridNames);
		}
	}
	catch (openvdb::Exception &e)
	{
		std::ofstream logfile;
		logfile.open("C:\\Users\\zach\\Documents\\Unreal Projects\\obtuse-tanuki\\CosmicSafari\\ThirdParty\\exceptions.log");
		logfile << "OvdbLoadVdb: " << e.what() << std::endl;
		logfile.close();
		error = 1;
	}
	return error;
}

struct comparePointType
{
	bool operator()(const PointType &lhs, const PointType &rhs) const
	{
		const static double eps = 0.00000001;
		return ((lhs.x() - rhs.x()) <= eps &&
			    (lhs.y() - rhs.y()) <= eps &&
			    (lhs.z() - rhs.z()) <= eps);
	}
};

PointType CubeVertex(PolyIndices corner, const PointType &boundsMin, const PointType &boundsMax)
{
	PointType v;
	if (corner == MXMYMZ)
	{
		v = boundsMax;
	}
	else if (corner == NXNYNZ)
	{
		v = boundsMin;
	}
	else if (corner == MXMYNZ)
	{
		v = PointType(boundsMax.x(), boundsMax.y(), boundsMin.z());
	}
	else if (corner == MXNYMZ)
	{
		v = PointType(boundsMax.x(), boundsMin.y(), boundsMax.z());
	}
	else if (corner == NXMYMZ)
	{
		v = PointType(boundsMin.x(), boundsMax.y(), boundsMax.z());
	}
	else if (corner == MXNYNZ)
	{
		v = PointType(boundsMax.x(), boundsMin.y(), boundsMin.z());
	}
	else if (corner == NXMYNZ)
	{
		v = PointType(boundsMin.x(), boundsMax.y(), boundsMin.z());
	}
	else //if (corner == NXNYMZ)
	{
		v = PointType(boundsMin.x(), boundsMin.y(), boundsMax.z());
	}
	return v;
}

int OvdbVolumeToMesh(double isovalue, double adaptivity)
{
	int error = 0;
	try
	{
		openvdb::CoordBBox bbox;
		IndexType vertexIndex = 0;
		std::map<PointType, IndexType, comparePointType> polyVerticesByIndex;

		//First collect all the vertices of each cube mapped to an index
		for (TreeDataType::NodeCIter i = SparseGrids->tree().cbeginNode(); i; ++i)
		{
			//From openvdb_viewer RenderModules.cc: Nodes are rendered as cell-centered
			i.getBoundingBox(bbox);
			const PointType min(bbox.min().x() - 0.5, bbox.min().y() - 0.5, bbox.min().z() - 0.5);
			const PointType max(bbox.max().x() + 0.5, bbox.max().y() + 0.5, bbox.max().z() + 0.5);

			//Each corner is shared by 3 faces
			polyVerticesByIndex.insert(std::pair<PointType, IndexType>(CubeVertex(NXNYNZ, min, max), vertexIndex++));
			polyVerticesByIndex.insert(std::pair<PointType, IndexType>(CubeVertex(NXNYNZ, min, max), vertexIndex++));
			polyVerticesByIndex.insert(std::pair<PointType, IndexType>(CubeVertex(NXNYNZ, min, max), vertexIndex++));

			polyVerticesByIndex.insert(std::pair<PointType, IndexType>(CubeVertex(NXNYMZ, min, max), vertexIndex++));
			polyVerticesByIndex.insert(std::pair<PointType, IndexType>(CubeVertex(NXNYMZ, min, max), vertexIndex++));
			polyVerticesByIndex.insert(std::pair<PointType, IndexType>(CubeVertex(NXNYMZ, min, max), vertexIndex++));

			polyVerticesByIndex.insert(std::pair<PointType, IndexType>(CubeVertex(MXNYMZ, min, max), vertexIndex++));
			polyVerticesByIndex.insert(std::pair<PointType, IndexType>(CubeVertex(MXNYMZ, min, max), vertexIndex++));
			polyVerticesByIndex.insert(std::pair<PointType, IndexType>(CubeVertex(MXNYMZ, min, max), vertexIndex++));

			polyVerticesByIndex.insert(std::pair<PointType, IndexType>(CubeVertex(MXNYNZ, min, max), vertexIndex++));
			polyVerticesByIndex.insert(std::pair<PointType, IndexType>(CubeVertex(MXNYNZ, min, max), vertexIndex++));
			polyVerticesByIndex.insert(std::pair<PointType, IndexType>(CubeVertex(MXNYNZ, min, max), vertexIndex++));

			polyVerticesByIndex.insert(std::pair<PointType, IndexType>(CubeVertex(MXMYNZ, min, max), vertexIndex++));
			polyVerticesByIndex.insert(std::pair<PointType, IndexType>(CubeVertex(MXMYNZ, min, max), vertexIndex++));
			polyVerticesByIndex.insert(std::pair<PointType, IndexType>(CubeVertex(MXMYNZ, min, max), vertexIndex++));

			polyVerticesByIndex.insert(std::pair<PointType, IndexType>(CubeVertex(MXMYMZ, min, max), vertexIndex++));
			polyVerticesByIndex.insert(std::pair<PointType, IndexType>(CubeVertex(MXMYMZ, min, max), vertexIndex++));
			polyVerticesByIndex.insert(std::pair<PointType, IndexType>(CubeVertex(MXMYMZ, min, max), vertexIndex++));

			polyVerticesByIndex.insert(std::pair<PointType, IndexType>(CubeVertex(NXMYMZ, min, max), vertexIndex++));
			polyVerticesByIndex.insert(std::pair<PointType, IndexType>(CubeVertex(NXMYMZ, min, max), vertexIndex++));
			polyVerticesByIndex.insert(std::pair<PointType, IndexType>(CubeVertex(NXMYMZ, min, max), vertexIndex++));

			polyVerticesByIndex.insert(std::pair<PointType, IndexType>(CubeVertex(NXMYNZ, min, max), vertexIndex++));
			polyVerticesByIndex.insert(std::pair<PointType, IndexType>(CubeVertex(NXMYNZ, min, max), vertexIndex++));
			polyVerticesByIndex.insert(std::pair<PointType, IndexType>(CubeVertex(NXMYNZ, min, max), vertexIndex++));
		}

		for (std::map<PointType, IndexType, comparePointType>::const_iterator i = polyVerticesByIndex.begin(); i != polyVerticesByIndex.end(); i++)
		{
			PointType p = i->first;
			IndexType x = i->second;
			Vertices.push_back(VertexType((float)p.x(), (float)p.y(), (float)p.z()));
			Triangles.push_back(IndexType(x));
		}
	}
	catch (openvdb::Exception &e)
	{
		std::ofstream logfile;
		logfile.open("C:\\Users\\zach\\Documents\\Unreal Projects\\obtuse-tanuki\\CosmicSafari\\ThirdParty\\exceptions.log");
		logfile << "OvdbVolumeToMesh: " << e.what() << std::endl;
		logfile.close();
		error = 1;
	}
	return error;
}

int OvdbGetNextMeshPoint(float &px, float &py, float &pz)
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
	return 1;
}

int OvdbGetNextMeshTriangle(uint32_t &vertexIndex)
{
	if (Triangles.empty())
	{
		return 0;
	}
	vertexIndex = Triangles.back();
	Triangles.pop_back();
	return 1;
}

int OvdbGetNextMeshQuad(uint32_t &qw, uint32_t &qx, uint32_t &qy, uint32_t &qz)
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
	return 1;
}

std::string gridNamesList(const openvdb::io::File &file)
{
	//Must call with an open file
	std::string validNames;
	openvdb::io::File::NameIterator nameIter = file.beginName();
	while (nameIter != file.endName())
	{
		validNames += nameIter.gridName();
		nameIter++;
		if (nameIter != file.endName())
		{
			validNames += ", ";
		}
	}
	return validNames;
}