#include "libovdb.h"
#include <openvdb/openvdb.h>
#include <openvdb/Exceptions.h>
#include <openvdb/tools/Dense.h>
#include <openvdb/tools/VolumeToMesh.h>
#include <fstream>
#include <map>

enum Component {cX, cY, cZ}; //Just to make the code a bit easier to follow

typedef struct _Quad_
{
	openvdb::Vec3d vertex; //The quad's corner which will be the far lower or far upper corner (each from which the corresponding 3 faces extend to total all 6 faces)
	double width;
	double height;
	bool isMerged;
	Component lz, ly, lx; //Which vertex indices to refer to when comparing quads
	void _Quad_::setLocal(Component x, Component y, Component z) { lx = x; ly = y; lz = z; }
	double _Quad_::localZ() const { return vertex[lz]; } //Get the component perpindicular to the quad face
	double _Quad_::localY() const { return vertex[ly]; } //Get the first component of the quad vertex
	double _Quad_::localX() const { return vertex[lx]; } //Get the second component of the quad vertex
} Quad;

//Sort quads by a total ordering
//(via Mikola Lysenko at http://0fps.net/2012/06/30/meshing-in-a-minecraft-game)
inline bool compareQuads(const Quad &l, const Quad &r)
{
	if (l.localZ() != r.localZ()) return l.localZ() < r.localZ(); //TODO: Compare doubles with epsilon
	if (l.localY() != r.localY()) return l.localY() < r.localY();
	if (l.localX() != r.localX()) return l.localX() < r.localX();
	if (l.width != r.width) return fabs(l.width) > fabs(r.width);
	return l.height >= r.height;
}

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
void getPrimitiveQuads(openvdb::FloatGrid::ConstPtr grid,
	std::vector<Quad> &faceLowerXY, std::vector<Quad> &faceUpperXY,
	std::vector<Quad> &faceLowerYZ, std::vector<Quad> &faceUpperYZ,
	std::vector<Quad> &faceLowerXZ, std::vector<Quad> &faceUpperXZ);
void mergeQuads(std::vector<Quad> &quads);

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
		std::vector<Quad> faceLowerXY;
		std::vector<Quad> faceUpperXY;
		std::vector<Quad> faceLowerYZ;
		std::vector<Quad> faceUpperYZ;
		std::vector<Quad> faceLowerXZ;
		std::vector<Quad> faceUpperXZ;
		getPrimitiveQuads(SparseGrids, faceLowerXY, faceUpperXY, faceLowerYZ, faceUpperYZ, faceLowerXZ, faceUpperXZ);
		mergeQuads(faceLowerXY);
		mergeQuads(faceUpperXY);
		mergeQuads(faceLowerYZ);
		mergeQuads(faceUpperYZ);
		mergeQuads(faceLowerXZ);
		mergeQuads(faceUpperXZ);
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

void getPrimitiveQuads(openvdb::FloatGrid::ConstPtr grid,
	                   std::vector<Quad> &faceLowerXY, std::vector<Quad> &faceUpperXY,
					   std::vector<Quad> &faceLowerYZ, std::vector<Quad> &faceUpperYZ,
					   std::vector<Quad> &faceLowerXZ, std::vector<Quad> &faceUpperXZ)
{
	const double edgeLength = 1.0; //Primitive quads always start with width/height of 1
	openvdb::CoordBBox boundingBox = grid->evalActiveVoxelBoundingBox();
	for (auto currentVoxel = grid->beginValueOn(); currentVoxel; ++currentVoxel)
	{
		if (!currentVoxel.isVoxelValue())
		{
			continue;
		}
		openvdb::Vec3d lowerCorner = grid->indexToWorld(currentVoxel.getCoord());
		openvdb::Vec3d upperCorner = openvdb::Vec3d(lowerCorner.x() + edgeLength, lowerCorner.y() + edgeLength, lowerCorner.z() + edgeLength);
		
		//Build the 6 quads comprising this voxel which has origin at the lower corner or upper corner
		Quad quad;
		quad.isMerged = false;

		//Lower faces
		quad.vertex = lowerCorner;
		quad.width = edgeLength;
		quad.height = edgeLength;
		//XY
		quad.setLocal(cX, cY, cZ);
		faceLowerXY.push_back(quad);
		//YZ
		quad.setLocal(cZ, cY, cX);
		faceLowerYZ.push_back(quad);
		//XZ
		quad.setLocal(cX, cZ, cY);
		faceLowerXZ.push_back(quad);

		//Upper faces
		quad.vertex = upperCorner;
		quad.width = -edgeLength;
		quad.height = -edgeLength;
		//XY
		quad.setLocal(cX, cY, cZ);
		faceUpperXY.push_back(quad);
		//YZ
		quad.setLocal(cZ, cY, cX);
		faceUpperYZ.push_back(quad);
		//XZ
		quad.setLocal(cX, cZ, cY);
		faceUpperXZ.push_back(quad);
	}
}

void mergeQuads(std::vector<Quad> &quads)
{
	//Sort quads into a total ordering
	std::sort(quads.begin(), quads.end(), compareQuads);

	for (auto i = quads.begin(); i != quads.end(); i++)
	{
		//Skip a quad that's been merged
		if (i->isMerged)
		{
			continue;
		}

		//We start with the lowest quad (since it's known they are sorted)
		//and check each successive quad until no more can be merged.
		//Then start again with the next un-merged quad.
		auto j = i;
		j++;
		//Only attempt to merge if both quads are on the same vertical level
		for (; j != quads.end() && ((j->localZ() - i->localZ()) < 0.00001); j++)
		{
			if (j->isMerged)
			{
				continue; //TODO: Figure out if on/off check here is necessary. Since we're greedy meshing, it may not ever matter to check here
			}
			//Check if we can merge one direction
			if (fabs(j->localY() - i->localY()) < 0.00001 && //Same vertical location...
				fabs(j->localX() - i->localX()) < (i->width + 0.00001) && //Adjacent...
				fabs(j->height - i->height) <= 0.00001) //Same height
			{
				i->width += j->width;
			}
			//Can't merge in that direction so check if we can merge the other direction
			else if (fabs(j->localX() - i->localX()) < 0.00001 && //Same horizontal location...
				     fabs(j->localY() - i->localY()) < (i->height + 0.00001) && //Adjacent...
				     fabs(j->width - i->width) <= 0.00001) //Same width
			{
				i->height += j->height;
			}
			else //Done with merging since we could no longer merge in either direction
			{
				break;
			}
			j->isMerged = true; //Mark this quad as merged so that it won't be meshed
		}
	}
}