#include "libovdb.h"
#include <openvdb/openvdb.h>
#include <openvdb/Exceptions.h>
#include <openvdb/tools/Dense.h>
#include <openvdb/tools/VolumeToMesh.h>
#include <fstream>
#include <map>

#define ADD_VEC3D(l, r) openvdb::Vec3d(l.x()+r.x(),l.y()+r.y(),l.z()+r.z())
//One direction for each of the 6 planes formed by faces of a cube
enum DIRECTIONS { POSZ, NEGZ, POSX, NEGX, POSY, NEGY, DIRECTIONS_FIRST = POSZ, DIRECTIONS_COUNT = NEGY + 1 };

typedef struct _Quad_
{
	openvdb::Vec3d vertex;
	double width;
	double height;
	bool on;
} Quad;

typedef openvdb::FloatGrid GridDataType;
typedef GridDataType::TreeType TreeDataType;
typedef openvdb::math::Vec3s VertexType;
typedef openvdb::Vec3d PointType;
typedef openvdb::Vec4I QuadType;
typedef openvdb::Vec3I IndexType;

static openvdb::FloatGrid::Ptr SparseGrids = nullptr;
static std::vector<VertexType> Vertices;
static std::vector<IndexType> Triangles;
//static std::vector<QuadType> Quads;

std::string gridNamesList(const openvdb::io::File &file);
void getPrimitiveQuads(openvdb::FloatGrid::ConstPtr grid, std::vector<Quad> &quadList, DIRECTIONS direction);

//TODO: Refactor
void mergeQuads_XYPlane(std::vector<Quad> &quads);
void mergeQuads_YZPlane(std::vector<Quad> &quads);
void mergeQuads_XZPlane(std::vector<Quad> &quads);

inline bool compareQuads_XYPlane(const Quad &l, const Quad &r)
{
	if (l.vertex.z() != r.vertex.z()) return l.vertex.z() < r.vertex.z();
	if (l.vertex.y() != r.vertex.y()) return l.vertex.y() < r.vertex.y();
	if (l.vertex.x() != r.vertex.x()) return l.vertex.x() < r.vertex.x();
	if (l.width != r.width) return l.width > r.width;
	return l.height >= r.height;
}

inline bool compareQuads_YZPlane(const Quad &l, const Quad &r)
{
	if (l.vertex.x() != r.vertex.x()) return l.vertex.x() < r.vertex.x();
	if (l.vertex.z() != r.vertex.z()) return l.vertex.z() < r.vertex.z();
	if (l.vertex.y() != r.vertex.y()) return l.vertex.y() < r.vertex.y();
	if (l.width != r.width) return l.width > r.width;
	return l.height >= r.height;
}

inline bool compareQuads_XZPlane(const Quad &l, const Quad &r)
{
	if (l.vertex.y() != r.vertex.y()) return l.vertex.y() < r.vertex.y();
	if (l.vertex.z() != r.vertex.z()) return l.vertex.z() < r.vertex.z();
	if (l.vertex.x() != r.vertex.x()) return l.vertex.x() < r.vertex.x();
	if (l.width != r.width) return l.width > r.width;
	return l.height >= r.height;
}

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

int OvdbVolumeToMesh(double isovalue, double adaptivity)
{
	int error = 0;
	try
	{
		std::vector<Quad> quadsPosz;
		std::vector<Quad> quadsNegz;
		std::vector<Quad> quadsPosx;
		std::vector<Quad> quadsNegx;
		std::vector<Quad> quadsPosy;
		std::vector<Quad> quadsNegy;
		getPrimitiveQuads(SparseGrids, quadsPosz, POSZ);
		getPrimitiveQuads(SparseGrids, quadsNegz, NEGZ);
		getPrimitiveQuads(SparseGrids, quadsPosx, POSX);
		getPrimitiveQuads(SparseGrids, quadsNegx, NEGX);
		getPrimitiveQuads(SparseGrids, quadsPosy, POSY);
		getPrimitiveQuads(SparseGrids, quadsNegy, NEGY);
		mergeQuads_XYPlane(quadsPosz);
		mergeQuads_XYPlane(quadsNegz);
		mergeQuads_YZPlane(quadsPosx);
		mergeQuads_YZPlane(quadsNegx);
		mergeQuads_XZPlane(quadsPosy);
		mergeQuads_XZPlane(quadsNegy);
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

int OvdbGetNextMeshTriangle(uint32_t &i0, uint32_t &i1, uint32_t &i2)
{
	if (Triangles.empty())
	{
		return 0;
	}
	IndexType triangleIndices = Triangles.back();
	i0 = triangleIndices.x();
	i1 = triangleIndices.y();
	i2 = triangleIndices.z();
	Triangles.pop_back();
	return 1;
}

int OvdbGetNextMeshQuad(uint32_t &qw, uint32_t &qx, uint32_t &qy, uint32_t &qz)
{
	//if (Quads.empty())
	//{
	//	return 0;
	//}
	//QuadType qs = Quads.back();
	//Quads.pop_back();
	//qw = qs.w();
	//qx = qs.x();
	//qy = qs.y();
	//qz = qs.z();
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
		++nameIter;
		if (nameIter != file.endName())
		{
			validNames += ", ";
		}
	}
	return validNames;
}

void getPrimitiveQuads(openvdb::FloatGrid::ConstPtr grid, std::vector<Quad> &quadList, DIRECTIONS direction)
{
	openvdb::Vec3d addVec;
	if (direction == POSZ)
	{
		addVec = openvdb::Vec3d(0.0, 0.0, SparseGrids->voxelSize().z());
	}
	else if (direction == NEGZ)
	{
		addVec = openvdb::Vec3d(0.0, 0.0, 0.0);
	}
	else if (direction == POSX)
	{
		addVec = openvdb::Vec3d(SparseGrids->voxelSize().x(), 0.0, 0.0);
	}
	else if (direction == NEGX)
	{
		addVec = openvdb::Vec3d(0.0, 0.0, 0.0);
	}
	else if (direction == POSY)
	{
		addVec = openvdb::Vec3d(0.0, SparseGrids->voxelSize().y(), 0.0);
	}
	else //NEGY
	{
		addVec = openvdb::Vec3d(0.0, 0.0, 0.0);
	}

	openvdb::CoordBBox boundingBox = grid->evalActiveVoxelBoundingBox();
	for (auto currentVoxel = grid->beginValueOn(); currentVoxel; ++currentVoxel)
	{
		if (!currentVoxel.isVoxelValue())
		{
			continue;
		}
		openvdb::Vec3d location = grid->indexToWorld(currentVoxel.getCoord());
		Quad quad;
		quad.vertex = ADD_VEC3D(location, addVec);
		quad.width = 1.0; //TODO: Use voxelSize()
		quad.height = 1.0;
		quad.on = true;
		quadList.push_back(quad);
	}
}

void mergeQuads_XYPlane(std::vector<Quad> &quads)
{
	std::sort(quads.begin(), quads.end(), compareQuads_XYPlane);
	for (auto i = quads.begin(); i != quads.end(); i++)
	{
		//Skip a quad that's been merged
		if (!i->on)
		{
			continue;
		}

		//Since we know that the quads have been sorted by a total ordering
		//(as per Mikola Lysenko at http://0fps.net/2012/06/30/meshing-in-a-minecraft-game)
		//we start with a quad then check each successive quad until no more can be merged,
		//then start with the next un-merged quad.
		auto j = i;
		j++;
		for (; j != quads.end(); j++)
		{
			if (!j->on)
			{
				continue; //TODO: Figure out if on/off check here is necessary. Since we're greedy meshing, it may not ever matter to check here
			}
			if ((j->vertex.z() - i->vertex.z()) > 0.00001)
			{
				//Don't try to merge if they're not on the same XY plane
				break;
			}
			//If the quad can be merged because the next one is level on the x-axis and has the same height...
			if (((j->vertex.x() - i->vertex.x()) < (i->width + 0.00001)) &&
				(fabs(j->height - i->height) <= 0.00001))
			{
				i->width += j->width;
				j->on = false;
			}
			//If the quad can be merged because the next one is level on the y-axis and has the same width...
			else if (((j->vertex.y() - i->vertex.y()) < (i->height + 0.00001)) &&
				(fabs(j->width - i->width) <= 0.00001))
			{
				i->height += j->height;
				j->on = false;
			}
			else
			{
				//Bumped into an unmergable area, so stop
				break;
			}
		}
	}
}

void mergeQuads_YZPlane(std::vector<Quad> &quads)
{
	std::sort(quads.begin(), quads.end(), compareQuads_YZPlane);
	for (auto i = quads.begin(); i != quads.end(); i++)
	{
		//Skip a quad that's been merged
		if (!i->on)
		{
			continue;
		}

		//Since we know that the quads have been sorted by a total ordering
		//(as per Mikola Lysenko at http://0fps.net/2012/06/30/meshing-in-a-minecraft-game)
		//we start with a quad then check each successive quad until no more can be merged,
		//then start with the next un-merged quad.
		auto j = i;
		j++;
		for (; j != quads.end(); j++)
		{
			if (!j->on)
			{
				continue; //TODO: Figure out if on/off check here is necessary. Since we're greedy meshing, it may not ever matter to check here
			}
			if ((j->vertex.x() - i->vertex.x()) > 0.00001)
			{
				//Don't try to merge if they're not on the same YZ plane
				break;
			}
			//If the quad can be merged because the next one is level on the y-axis and has the same height...
			if (((j->vertex.y() - i->vertex.y()) < (i->width + 0.00001)) &&
				(fabs(j->height - i->height) <= 0.00001))
			{
				i->width += j->width;
				j->on = false;
			}
			//If the quad can be merged because the next one is level on the z-axis and has the same width...
			else if (((j->vertex.z() - i->vertex.z()) < (i->height + 0.00001)) &&
				(fabs(j->width - i->width) <= 0.00001))
			{
				i->height += j->height;
				j->on = false;
			}
			else
			{
				//Bumped into an unmergable area, so stop
				break;
			}
		}
	}
}

void mergeQuads_XZPlane(std::vector<Quad> &quads)
{
	std::sort(quads.begin(), quads.end(), compareQuads_XZPlane);
	for (auto i = quads.begin(); i != quads.end(); i++)
	{
		//Skip a quad that's been merged
		if (!i->on)
		{
			continue;
		}

		//Since we know that the quads have been sorted by a total ordering
		//(as per Mikola Lysenko at http://0fps.net/2012/06/30/meshing-in-a-minecraft-game)
		//we start with a quad then check each successive quad until no more can be merged,
		//then start with the next un-merged quad.
		auto j = i;
		j++;
		for (; j != quads.end(); j++)
		{
			if (!j->on)
			{
				continue; //TODO: Figure out if on/off check here is necessary. Since we're greedy meshing, it may not ever matter to check here
			}
			if ((j->vertex.y() - i->vertex.y()) > 0.00001)
			{
				//Don't try to merge if they're not on the same XZ plane
				break;
			}
			//If the quad can be merged because the next one is level on the x-axis and has the same height...
			if (((j->vertex.x() - i->vertex.x()) < (i->width + 0.00001)) &&
				(fabs(j->height - i->height) <= 0.00001))
			{
				i->width += j->width;
				j->on = false;
			}
			//If the quad can be merged because the next one is level on the z-axis and has the same width...
			else if (((j->vertex.z() - i->vertex.z()) < (i->height + 0.00001)) &&
				     (fabs(j->width - i->width) <= 0.00001))
			{
				i->height += j->height;
				j->on = false;
			}
			else
			{
				//Bumped into an unmergable area, so stop
				break;
			}
		}
	}
}