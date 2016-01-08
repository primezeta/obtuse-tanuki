#include "libovdb.h"
#include <openvdb/openvdb.h>
#include <openvdb/Exceptions.h>
#include <openvdb/tools/Dense.h>
#include <openvdb/tools/VolumeToMesh.h>
#include <openvdb/tools/Clip.h>
#include <openvdb/tools/GridOperators.h>
#include <fstream>
#include <map>

typedef int64_t IndexType;
static openvdb::FloatGrid::Ptr SparseGrids = nullptr;
static std::vector<openvdb::Vec3d> WorldVertices;
static std::vector<IndexType> TriangleIndices;
enum QuadOrientation { XY_TOP, XY_BOT, XZ_TOP, XZ_BOT, YZ_TOP, YZ_BOT, ORIENTATION_COUNT };

typedef struct _Quad_
{
    _Quad_(QuadOrientation o, openvdb::FloatGrid::ConstPtr g, std::vector<openvdb::Coord> &vs, IndexType i1, IndexType i2, IndexType i3, IndexType i4, double w, double h) :
        vertices(vs), v1(i1), v2(i2), v3(i3), v4(i4), width(w), height(h), isMerged(false) { grid = g; }
    IndexType v1;
    IndexType v2;
    IndexType v3;
    IndexType v4;
    bool isMerged;
    double width;
    double height;
    openvdb::Coord origin() const { return coordByOrientation(vertices[v1]); }
    openvdb::Coord left() const { return coordByOrientation(vertices[v2]); }
    openvdb::Coord right() const { return coordByOrientation(vertices[v3]); }
    openvdb::Coord opposite() const { return coordByOrientation(vertices[v4]); }
    openvdb::Vec3d worldOrigin() const { return grid->indexToWorld(origin()); }
    openvdb::Vec3d worldV1() const { return grid->indexToWorld(vertices[v1]); }
    openvdb::Vec3d worldV2() const { return grid->indexToWorld(vertices[v2]); }
    openvdb::Vec3d worldV3() const { return grid->indexToWorld(vertices[v3]); }
    openvdb::Vec3d worldV4() const { return grid->indexToWorld(vertices[v4]); }
private:
    QuadOrientation orientation;
    openvdb::FloatGrid::ConstPtr grid;
    const std::vector<openvdb::Coord> &vertices;
    openvdb::Coord coordByOrientation(const openvdb::Coord &c) const
    {
        if (orientation == XY_TOP || orientation == XY_BOT)
        {
            return openvdb::Coord(c.x(), c.y(), c.z());
        }
        else if (orientation == XZ_TOP || orientation == XZ_BOT)
        {
            return openvdb::Coord(c.x(), c.z(), c.y());
        }
        else //YZ_TOP or YZ_BOT
        {
            return openvdb::Coord(c.y(), c.z(), c.x());
        }
    }
} Quad;

//Sort quads by a total ordering
//(via Mikola Lysenko at http://0fps.net/2012/06/30/meshing-in-a-minecraft-game)
struct cmpByQuad
{
    bool operator()(const Quad &l, const Quad &r) const
    {
        if (l.origin().y() != r.origin().y()) return l.origin().y() < r.origin().y();
        if (l.origin().x() != r.origin().x()) return l.origin().x() < r.origin().x();
        if (l.width != r.width) return l.width > r.width;
        return l.height >= r.height;
    }
};

typedef std::set<Quad, cmpByQuad> QuadSet;
typedef std::vector<Quad> QuadVec;
std::string gridNamesList(const openvdb::io::File &file);
openvdb::Int64 getVertexIndex(openvdb::FloatGrid::ConstPtr grid, std::vector<openvdb::Vec3d> &vertices, std::vector<openvdb::Coord> &vertexCoords, openvdb::Int64Grid::Accessor &acc, const openvdb::Coord &coord);
void getMergedQuads(openvdb::FloatGrid::ConstPtr grid, std::vector<openvdb::Vec3d> &vertices, std::vector<IndexType> &triIndices);
void mergeQuads(QuadVec &quads);

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

int OvdbVolumeToMesh(int32_t regionCountX, int32_t regionCountY, int32_t regionCountZ, double isovalue, double adaptivity)
{
    int error = 0;
    try
    {
        getMergedQuads(SparseGrids, WorldVertices, TriangleIndices);
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
    if (WorldVertices.empty())
    {
        return 0;
    }
    openvdb::Vec3d v = WorldVertices.back();
    WorldVertices.pop_back();
    vx = float(v.x());
    vy = float(v.y());
    vz = float(v.z());
    return 1;
}

int OvdbGetNextMeshTriangle(uint32_t &i0, uint32_t &i1, uint32_t &i2)
{
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

openvdb::Int64 getVertexIndex(openvdb::FloatGrid::ConstPtr grid, std::vector<openvdb::Vec3d> &vertices, std::vector<openvdb::Coord> &vertexCoords, openvdb::Int64Grid::Accessor &acc, const openvdb::Coord &coord)
{
	openvdb::Int64 index = acc.getValue(coord);
	if (index < 0)
	{
		vertexCoords.push_back(coord);
		vertices.push_back(grid->indexToWorld(coord));
		index = IndexType(vertexCoords.size() - 1);
		acc.setValue(coord, index);
	}
	return index;
}

void getMergedQuads(openvdb::FloatGrid::ConstPtr grid, std::vector<openvdb::Vec3d> &vertices, std::vector<IndexType> &triIndices)
{
    QuadSet uniqueQuads[ORIENTATION_COUNT];
    std::vector<openvdb::Coord> vertexCoords;
    openvdb::Int64Grid::Ptr visitedVertexIndices = openvdb::Int64Grid::create(-1);
    visitedVertexIndices->setTransform(grid->transformPtr()->copy());
    visitedVertexIndices->topologyUnion(*grid);
    openvdb::Vec3d voxelSize = grid->voxelSize();
	openvdb::FloatGrid::ConstAccessor acc = grid->getConstAccessor();

    for (auto i = grid->tree().cbeginValueOn(); i; ++i)
    {
		if (!i.isVoxelValue())
		{
			continue;
		}
        openvdb::Coord origin = i.getCoord();
		if (!openvdb::math::isApproxEqual(acc.getValue(origin), 0.0f))
		{
			continue;
		}
        openvdb::Coord opposite = openvdb::Coord(origin.x() + 1, origin.y() + 1, origin.z() + 1);
		openvdb::Int64 indices[8];
		indices[0] = getVertexIndex(grid, vertices, vertexCoords, visitedVertexIndices->getAccessor(), origin);
        indices[1] = getVertexIndex(grid, vertices, vertexCoords, visitedVertexIndices->getAccessor(), openvdb::Coord(origin.x() + 1, origin.y(), origin.z()));
        indices[2] = getVertexIndex(grid, vertices, vertexCoords, visitedVertexIndices->getAccessor(), openvdb::Coord(origin.x(), origin.y() + 1, origin.z()));
        indices[3] = getVertexIndex(grid, vertices, vertexCoords, visitedVertexIndices->getAccessor(), openvdb::Coord(origin.x(), origin.y(), origin.z() + 1));
        indices[4] = getVertexIndex(grid, vertices, vertexCoords, visitedVertexIndices->getAccessor(), opposite);
        indices[5] = getVertexIndex(grid, vertices, vertexCoords, visitedVertexIndices->getAccessor(), openvdb::Coord(opposite.x() - 1, opposite.y(), opposite.z()));
        indices[6] = getVertexIndex(grid, vertices, vertexCoords, visitedVertexIndices->getAccessor(), openvdb::Coord(opposite.x(), opposite.y() - 1, opposite.z()));
        indices[7] = getVertexIndex(grid, vertices, vertexCoords, visitedVertexIndices->getAccessor(), openvdb::Coord(opposite.x(), opposite.y(), opposite.z() - 1));
        Quad q1(XY_BOT, grid, vertexCoords, indices[0], indices[1], indices[2], indices[7], voxelSize.x(), voxelSize.y());
        Quad q2(XZ_BOT, grid, vertexCoords, indices[0], indices[1], indices[3], indices[6], voxelSize.x(), voxelSize.z());
        Quad q3(YZ_BOT, grid, vertexCoords, indices[0], indices[2], indices[3], indices[5], voxelSize.y(), voxelSize.z());
        Quad q4(XY_TOP, grid, vertexCoords, indices[3], indices[6], indices[5], indices[4], voxelSize.x(), voxelSize.y());
        Quad q5(XZ_TOP, grid, vertexCoords, indices[2], indices[7], indices[5], indices[4], voxelSize.x(), voxelSize.z());
        Quad q6(YZ_TOP, grid, vertexCoords, indices[1], indices[7], indices[6], indices[4], voxelSize.y(), voxelSize.z());
        uniqueQuads[XY_BOT].insert(q1);
        uniqueQuads[XZ_BOT].insert(q2);
        uniqueQuads[YZ_BOT].insert(q3);
        uniqueQuads[XY_TOP].insert(q4);
        uniqueQuads[XZ_TOP].insert(q5);
        uniqueQuads[YZ_TOP].insert(q6);
    }

    for (auto i = 0; i < ORIENTATION_COUNT; i++)
    {
        QuadVec planarQuads;
        for (auto j = uniqueQuads[i].cbegin(); j != uniqueQuads[i].end(); j++)
        {
            planarQuads.push_back(*j);
        }
        mergeQuads(planarQuads);

        uint32_t mergedCount = 0;
        uint32_t vertexIndex = 0;
        for (auto j = planarQuads.begin(); j != planarQuads.end(); ++j)
        {
            if (j->isMerged)
            {
                mergedCount++; //For debugging
                continue;
            }
            //Collect triangle indices of the two triangles comprising this quad
            triIndices.push_back(j->v1); //Quad 1
            triIndices.push_back(j->v4);
            triIndices.push_back(j->v2);
            triIndices.push_back(j->v1); //Quad 2
            triIndices.push_back(j->v4);
            triIndices.push_back(j->v3);
        }
    }
}

void mergeQuads(QuadVec &quads)
{
    for (auto i = quads.begin(); i != quads.end(); ++i)
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
        for (; j != quads.end(); ++j)
        {
            //Only attempt to merge an unmerged quad that is on the same vertical level
            if (j->isMerged || //TODO: Figure out if 'isMerged' check here is necessary. Since we're greedy meshing, it may not ever matter to check here
                j->origin().z() != i->origin().z())
            {
                continue;
            }

            //Check if we can merge by width
            if (j->origin().y() == i->origin().y() && //Same location y-axis...
                openvdb::math::isApproxEqual(j->worldOrigin().x() - i->worldOrigin().x(), i->width) && //Adjacent...(note: due to the sorted ordering, don't have to use abs)
                j->height == i->height) //Same height
            {
                i->width += j->width;
                i->v3 = j->v3;
                i->v4 = j->v4;
            }
            //Check if we can merge by height
            else if (j->origin().x() == i->origin().x() && //Same location x-axis...
                     openvdb::math::isApproxEqual(j->worldOrigin().y() - i->worldOrigin().y(), i->height) && //Adjacent...(note: due to the sorted ordering, don't have to use abs)
                     j->width == i->width) //Same width
            {
                i->height += j->height;
                i->v2 = j->v2;
                i->v4 = j->v4;
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