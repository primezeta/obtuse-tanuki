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
    _Quad_(openvdb::FloatGrid::ConstPtr g, QuadOrientation o, double w, double h, std::vector<openvdb::Vec3d> &vs, openvdb::Vec4I is) :
		grid(g), orientation(o), width(w), height(h), vertices(vs), indices(is), isMerged(false) {}
	openvdb::Vec4I indices;
    bool isMerged;
    double width;
    double height;
	const openvdb::Vec3d &localPos(IndexType i) const { return orient((*this)[0]); }
	openvdb::Vec3d operator[](IndexType i) const { return grid->indexToWorld(vertices[indices[i]]); }
private:
	openvdb::FloatGrid::ConstPtr grid;
    const QuadOrientation orientation;    
	const std::vector<openvdb::Vec3d> &vertices;

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
void getCubeQuads(openvdb::CoordBBox &bbox, std::vector<openvdb::Coord> &coords, std::vector<openvdb::Vec4I> &quads);

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

void getCubeQuads(openvdb::CoordBBox &bbox, std::vector<openvdb::Coord> &coords, std::vector<openvdb::Vec4I> &quads)
{
	openvdb::CoordBBox cube = bbox.createCube(bbox.min(), 1);
	coords.push_back(cube.getStart());
	coords.push_back(cube.getStart().offsetBy(1, 0, 0));
	coords.push_back(cube.getStart().offsetBy(0, 1, 0));
	coords.push_back(cube.getStart().offsetBy(0, 0, 1));
	coords.push_back(cube.getEnd().offsetBy(-1, 0, 0));
	coords.push_back(cube.getEnd().offsetBy(0, -1, 0));
	coords.push_back(cube.getEnd().offsetBy(0, 0, -1));
	coords.push_back(cube.getEnd());
	quads.push_back(openvdb::Vec4I(3, 4, 5, 7)); //XY_TOP
	quads.push_back(openvdb::Vec4I(0, 1, 2, 7)); //XY_BOT
	quads.push_back(openvdb::Vec4I(2, 4, 6, 7)); //XZ_TOP
	quads.push_back(openvdb::Vec4I(0, 1, 3, 5)); //XZ_BOT
	quads.push_back(openvdb::Vec4I(1, 5, 6, 7)); //YZ_TOP
	quads.push_back(openvdb::Vec4I(0, 2, 3, 4)); //YZ_BOT
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
		getCubeQuads(i.getBoundingBox(), cubeCoords, cubeQuads);

		//First, get the indices to each world vertex according to each coordinate
		for (auto j = cubeCoords.begin(); j != cubeCoords.end(); ++j)
		{
			openvdb::Int32 vertexIndex = visitedAcc.getValue(*j);
			if (vertexIndex < 0)
			{
				//This is a new vertex. Save it to the visited vertex grid for use by any other voxels that share it
				worldVertices.push_back(grid->indexToWorld(*j));
				vertexIndex = IndexType(worldVertices.size() - 1);
				visitedAcc.setValue(*j, vertexIndex);
			}
		}

		//Now that each coordinate has a vertex index, set the vertices of each cube quad to those indices
		for (auto j = cubeQuads.begin(); j != cubeQuads.end(); ++j)
		{
			//Get an index for each corner of the quad
			for (int k = 0; k < 4; k++)
			{
				//Initially the index references the coord...
				openvdb::Int32 cubeCoordIndex = (*j)[k];
				openvdb::Coord coord = cubeCoords[cubeCoordIndex];

				//Get the coord and replace the coord index with the world vertex index from the visited-tree
				openvdb::Int32 worldVertexIndex = visitedAcc.getValue(coord);
				(*j)[k] = worldVertexIndex;
			}
		}

		//Set up the 6 quads, each of which references the 4 vertex indices from the world vertices
        Quad q1(grid, XY_TOP, voxelSize.x(), voxelSize.y(), worldVertices, cubeQuads[XY_TOP]);
		Quad q2(grid, XY_BOT, voxelSize.x(), voxelSize.z(), worldVertices, cubeQuads[XY_BOT]);
		Quad q3(grid, XZ_TOP, voxelSize.y(), voxelSize.z(), worldVertices, cubeQuads[XZ_TOP]);
		Quad q4(grid, XZ_BOT, voxelSize.x(), voxelSize.y(), worldVertices, cubeQuads[XZ_BOT]);
		Quad q5(grid, YZ_TOP, voxelSize.x(), voxelSize.z(), worldVertices, cubeQuads[YZ_TOP]);
		Quad q6(grid, YZ_BOT, voxelSize.y(), voxelSize.z(), worldVertices, cubeQuads[YZ_BOT]);

		//Insert into the quad set to set up the total ordering when we later retrieve the quads with iterators
        uniqueQuads[XY_BOT].insert(q1);
        uniqueQuads[XZ_BOT].insert(q2);
        uniqueQuads[YZ_BOT].insert(q3);
        uniqueQuads[XY_TOP].insert(q4);
        uniqueQuads[XZ_TOP].insert(q5);
        uniqueQuads[YZ_TOP].insert(q6);
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
            triIndices.push_back(j->indices[3]);
            triIndices.push_back(j->indices[1]);
            triIndices.push_back(j->indices[0]); //Quad 2
            triIndices.push_back(j->indices[3]);
            triIndices.push_back(j->indices[2]);
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