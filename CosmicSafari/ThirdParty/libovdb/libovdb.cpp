#include "libovdb.h"
#include <openvdb/openvdb.h>
#include <openvdb/Exceptions.h>
#include <openvdb/tools/Dense.h>
#include <openvdb/tools/VolumeToMesh.h>
#include <openvdb/tools/Clip.h>
#include <openvdb/tools/GridOperators.h>
#include <fstream>
#include <map>

typedef int32_t IndexType;
static openvdb::FloatGrid::Ptr SparseGrids = nullptr;
static std::vector<openvdb::Vec3d> WorldVertices;
static std::vector<IndexType> TriangleIndices;
enum QuadOrientation { XY_TOP, XY_BOT, XZ_TOP, XZ_BOT, YZ_TOP, YZ_BOT, ORIENTATION_COUNT };

typedef struct _Quad_
{
    _Quad_(QuadOrientation o, double w, double h, std::vector<openvdb::Vec3d> &vs, openvdb::Vec4I is) :
		orientation(o), width(w), height(h), vertices(vs), indices(is), isMerged(false) {}
	_Quad_(const _Quad_ &rhs) : orientation(rhs.orientation), vertices(rhs.vertices)
	{
		indices = rhs.indices;
		isMerged = rhs.isMerged;
		width = rhs.width;
		height = rhs.height;
	}

	const QuadOrientation orientation;
	const std::vector<openvdb::Vec3d> &vertices;
	openvdb::Vec4I indices;
    bool isMerged;
    double width;
    double height;

	const openvdb::Vec3d &localPos(IndexType i) const { return orient((*this)[0]); }
	openvdb::Vec3d operator[](IndexType i) const { return vertices[indices[i]]; }
    const openvdb::Vec3d &orient(const openvdb::Vec3d &v) const
    {
		static openvdb::Vec3d local; //Possibly unecessary optimization, and possibly error-prone
        if (orientation == XY_TOP || orientation == XY_BOT)
        {
			local = openvdb::Vec3d(v.x(), v.y(), v.z());
        }
        else if (orientation == XZ_TOP || orientation == XZ_BOT)
        {
			local = openvdb::Vec3d(v.x(), v.z(), v.y());
        }
        else //YZ_TOP or YZ_BOT
        {
			local = openvdb::Vec3d(v.y(), v.z(), v.x());
        }
		return local;
    }
} Quad;

//Sort quads by a total ordering
//(via Mikola Lysenko at http://0fps.net/2012/06/30/meshing-in-a-minecraft-game)
struct cmpByQuad
{
    bool operator()(const Quad &l, const Quad &r) const
    {
		//Index 0 is the origin vertex
		openvdb::Vec3d lvec = l.localPos(0);
		openvdb::Vec3d rvec = r.localPos(0);
		if (!openvdb::math::isApproxEqual(lvec.z(), rvec.z())) return lvec.z() < rvec.z();
		if (!openvdb::math::isApproxEqual(lvec.y(), rvec.y())) return lvec.y() < rvec.y();
        if (!openvdb::math::isApproxEqual(lvec.x(), rvec.x())) return lvec.x() < rvec.x();
        if (!openvdb::math::isApproxEqual(l.width, r.width)) return l.width > r.width;
        return openvdb::math::isApproxEqual(l.height, r.height) || l.height > r.height;
    }
};

typedef std::set<Quad, cmpByQuad> QuadSet;
typedef std::vector<Quad> QuadVec;
std::string gridNamesList(const openvdb::io::File &file);
void getMesh(openvdb::FloatGrid::ConstPtr grid, std::vector<openvdb::Vec3d> &vertices, std::vector<IndexType> &triIndices, OvdbMeshMethod meshMethod);
void greedyMergeQuads(QuadVec &sortedQuads);
openvdb::Index32 getAndSetWorldVertexIndex(openvdb::FloatGrid::ConstPtr grid, openvdb::Int32Grid::Accessor &visitedVertexAcc, std::vector<openvdb::Vec3d> &worldVertices, const openvdb::Coord &coord);
void getCubeQuads(openvdb::FloatGrid::ConstPtr grid, openvdb::Int32Grid::Ptr visitedVertexIndices, openvdb::CoordBBox &bbox, std::vector<openvdb::Vec3d> &worldVertices, std::vector<openvdb::Coord> &coords, std::vector<openvdb::Vec4I> &quads);

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

int OvdbVolumeToMesh(OvdbMeshMethod meshMethod, int32_t regionCountX, int32_t regionCountY, int32_t regionCountZ, double isovalue, double adaptivity)
{
    int error = 0;
    try
    {
        getMesh(SparseGrids, WorldVertices, TriangleIndices, meshMethod);
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

int OvdbGetNextMeshPoint(float &vx, float &vy, float &vz)
{
	static size_t index = 0;
	//Gather points from the start to the end to preserve order of vertex indices
    if (index == WorldVertices.size())
    {
		//None left - clear to save memory
		WorldVertices.clear();
        return 0;
    }
    openvdb::Vec3d v = WorldVertices[index];
    vx = float(v.x());
    vy = float(v.y());
    vz = float(v.z());
	index++;
    return 1;
}

int OvdbGetNextMeshTriangle(uint32_t &i0, uint32_t &i1, uint32_t &i2)
{
	//Order of indices doesn't matter
    if (TriangleIndices.empty())
    {
        return 0;
    }
    i0 = uint32_t(TriangleIndices.back()); //TODO: error check index ranges
    TriangleIndices.pop_back();
    i1 = uint32_t(TriangleIndices.back());
    TriangleIndices.pop_back();
    i2 = uint32_t(TriangleIndices.back());
    TriangleIndices.pop_back();
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

openvdb::Index32 getAndSetWorldVertexIndex(openvdb::FloatGrid::ConstPtr grid, openvdb::Int32Grid::Accessor &visitedVertexAcc, std::vector<openvdb::Vec3d> &worldVertices, const openvdb::Coord &coord)
{
	openvdb::Int32 vertexIndex = visitedVertexAcc.getValue(coord);
	if (vertexIndex == -1)
	{
		//This is a new vertex. Save it to the visited vertex grid for use by any other voxels that share it
		worldVertices.push_back(grid->indexToWorld(coord));
		vertexIndex = IndexType(worldVertices.size() - 1);
		visitedVertexAcc.setValue(coord, vertexIndex);
	}
	return vertexIndex;
}

void getCubeQuads(openvdb::FloatGrid::ConstPtr grid, openvdb::Int32Grid::Ptr visitedVertexIndices, openvdb::CoordBBox &bbox, std::vector<openvdb::Vec3d> &worldVertices, std::vector<openvdb::Coord> &coords, std::vector<openvdb::Vec4I> &quads)
{
	openvdb::CoordBBox cube = bbox.createCube(bbox.min(), 1);
	openvdb::Int32 indices[8];
	openvdb::Int32Grid::Accessor acc = visitedVertexIndices->getAccessor();
	indices[0] = getAndSetWorldVertexIndex(grid, acc, worldVertices, cube.getStart());
	indices[1] = getAndSetWorldVertexIndex(grid, acc, worldVertices, cube.getStart().offsetBy(1, 0, 0));
	indices[2] = getAndSetWorldVertexIndex(grid, acc, worldVertices, cube.getStart().offsetBy(0, 1, 0));
	indices[3] = getAndSetWorldVertexIndex(grid, acc, worldVertices, cube.getStart().offsetBy(0, 0, 1));
	indices[4] = getAndSetWorldVertexIndex(grid, acc, worldVertices, cube.getEnd().offsetBy(-1, 0, 0));
	indices[5] = getAndSetWorldVertexIndex(grid, acc, worldVertices, cube.getEnd().offsetBy(0, -1, 0));
	indices[6] = getAndSetWorldVertexIndex(grid, acc, worldVertices, cube.getEnd().offsetBy(0, 0, -1));
	indices[7] = getAndSetWorldVertexIndex(grid, acc, worldVertices, cube.getEnd());
	//The following indices reference the above coordinates
	quads.push_back(openvdb::Vec4I(indices[3], indices[4], indices[5], indices[7])); //XY_TOP
	quads.push_back(openvdb::Vec4I(indices[0], indices[1], indices[2], indices[6])); //XY_BOT
	quads.push_back(openvdb::Vec4I(indices[2], indices[4], indices[6], indices[7])); //XZ_TOP
	quads.push_back(openvdb::Vec4I(indices[0], indices[1], indices[3], indices[5])); //XZ_BOT
	quads.push_back(openvdb::Vec4I(indices[1], indices[5], indices[6], indices[7])); //YZ_TOP
	quads.push_back(openvdb::Vec4I(indices[0], indices[2], indices[3], indices[4])); //YZ_BOT
}

void getMesh(openvdb::FloatGrid::ConstPtr grid, std::vector<openvdb::Vec3d> &worldVertices, std::vector<IndexType> &triIndices, OvdbMeshMethod meshMethod)
{
    QuadSet uniqueQuads[ORIENTATION_COUNT];
    openvdb::Int32Grid::Ptr visitedVertexIndices = openvdb::Int32Grid::create(-1);
    openvdb::Vec3d voxelSize = grid->voxelSize();
	openvdb::FloatGrid::ConstAccessor gridAcc = grid->getConstAccessor();
	openvdb::Int32Grid::Accessor visitedAcc = visitedVertexIndices->getAccessor();

	//The visited vertex grid mirrors the grid, where the value of each voxel is the vertex index or -1 if that voxel has not been visited
	visitedVertexIndices->setTransform(grid->transformPtr()->copy());
	visitedVertexIndices->topologyUnion(*grid);

	for (auto i = grid->tree().cbeginValueOn(); i; ++i)
	{
		//Skip tile values and values that are not on the surface i.e. equal to 0
		if (!i.isVoxelValue() ||
			!openvdb::math::isApproxEqual(gridAcc.getValue(i.getCoord()), 0.0f))
		{
			continue;
		}

		//Build a cube from the current voxel
		std::vector<openvdb::Coord> cubeCoords;
		std::vector<openvdb::Vec4I> cubeQuads;
		getCubeQuads(grid, visitedVertexIndices, i.getBoundingBox(), worldVertices, cubeCoords, cubeQuads);

		//Set up the 6 quads each of width/height 1, and each of which references the 4 vertex indices from the world vertices
		for (auto j = 0; j < ORIENTATION_COUNT; ++j)
		{
			//Insert into the quad set to set up the total ordering when we later retrieve the quads with iterators;
			uniqueQuads[j].insert(Quad((QuadOrientation)j, 1.0, 1.0, worldVertices, cubeQuads[j]));
		}
    }

	//Collect the quads in a linear list and mesh them
    for (auto i = 0; i < ORIENTATION_COUNT; i++)
    {
        QuadVec quads;
        for (auto j = uniqueQuads[i].cbegin(); j != uniqueQuads[i].end(); j++)
        {
			quads.push_back(*j);
        }

		if (meshMethod == MESHING_GREEDY)
		{
			greedyMergeQuads(quads);
		}

        uint32_t mergedCount = 0;
        uint32_t vertexIndex = 0;
        for (auto j = quads.begin(); j != quads.end(); ++j)
        {
            if (j->isMerged)
            {
                mergedCount++; //For debugging
                continue;
            }
            //Collect triangle indices of the two triangles comprising this quad
            triIndices.push_back(j->indices[0]); //Quad 1
            triIndices.push_back(j->indices[1]);
            triIndices.push_back(j->indices[3]);
            triIndices.push_back(j->indices[0]); //Quad 2
            triIndices.push_back(j->indices[2]);
            triIndices.push_back(j->indices[3]);
        }
    }
}

void greedyMergeQuads(QuadVec &sortedQuads)
{
    for (auto i = sortedQuads.begin(); i != sortedQuads.end(); ++i)
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
        for (; j != sortedQuads.end(); ++j)
        {
            //Only attempt to merge an unmerged quad that is on the same vertical level
            if (j->isMerged || //TODO: Figure out if 'isMerged' check here is necessary. Since we're greedy meshing, it may not ever matter to check here
				openvdb::math::isApproxEqual(j->localPos(0).z(), i->localPos(0).z()))
            {
                continue;
            }

            //Check if we can merge by width
            if (openvdb::math::isApproxEqual(j->localPos(0).y(), i->localPos(0).y()) && //Same location y-axis...
                openvdb::math::isApproxEqual(j->localPos(0).x() - i->localPos(0).x(), i->width) && //Adjacent...(note: due to the sorted ordering, don't have to use abs)
				openvdb::math::isApproxEqual(j->height, i->height)) //Same height
            {
                i->width += j->width;
                (*i)[2] = (*j)[2];
				(*i)[3] = (*j)[3];
            }
            //Check if we can merge by height
            else if (openvdb::math::isApproxEqual(j->localPos(0).x(), i->localPos(0).x()) && //Same location x-axis...
					 openvdb::math::isApproxEqual(j->localPos(0).y() - i->localPos(0).y(), i->height) && //Adjacent...(note: due to the sorted ordering, don't have to use abs)
					 openvdb::math::isApproxEqual(j->width, i->width)) //Same width
            {
                i->height += j->height;
				(*i)[1] = (*j)[1];
				(*i)[3] = (*j)[3];
            }
            else
            {
                //Done with merging since we could no longer merge in either direction
                break;
            }
            j->isMerged = true; //Mark this quad as merged so that it won't be meshed
        }
    }
}