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
static std::vector<IndexType> Triangles;
enum QuadOrientation { XY, XZ, YZ, ORIENTATION_COUNT };

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
    openvdb::Vec3d worldLeft() const { return grid->indexToWorld(left()); }
    openvdb::Vec3d worldRight() const { return grid->indexToWorld(right()); }
    openvdb::Vec3d worldOpposite() const { return grid->indexToWorld(opposite()); }
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
        if (orientation == XY)
        {
            return openvdb::Coord(c.x(), c.y(), c.z());
        }
        else if (orientation == XZ)
        {
            return openvdb::Coord(c.x(), c.z(), c.y());
        }
        else //YZ
        {
            return openvdb::Coord(c.y(), c.z(), c.x());
        }
    }
} Quad;


typedef struct _VertexByIndex_
{
	openvdb::Coord coord;
    IndexType index;
    _VertexByIndex_(openvdb::Coord c, IndexType i) : coord(c), index(i) {};
} VertexByIndex;

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
void getVoxelQuads(openvdb::FloatGrid::ConstPtr grid, QuadVec &quads, std::vector<openvdb::Coord> &vertices);
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
        QuadVec voxelQuads;
        std::vector<openvdb::Coord> voxelVertexIndices;
		getVoxelQuads(SparseGrids, voxelQuads, voxelVertexIndices);
		mergeQuads(voxelQuads);

		uint32_t mergedCount = 0;
		uint32_t vertexIndex = 0;
		for (auto i = voxelQuads.begin(); i != voxelQuads.end(); ++i)
		{
			if (i->isMerged)
			{
				mergedCount++; //For debugging
			}
			else
			{
				//4 vertices for this quad
				WorldVertices.push_back(i->worldV1());
				WorldVertices.push_back(i->worldV2());
				WorldVertices.push_back(i->worldV3());
				WorldVertices.push_back(i->worldV4());
				//Collect triangle indices of the two triangles comprising this quad
				Triangles.push_back(i->v1); //Quad 1
				Triangles.push_back(i->v4);
				Triangles.push_back(i->v2);
				Triangles.push_back(i->v1); //Quad 2
				Triangles.push_back(i->v4);
				Triangles.push_back(i->v3);
			}
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
	if (Triangles.empty())
	{
		return 0;
	}
	i0 = uint32_t(Triangles.back()); //TODO: error check index ranges
	Triangles.pop_back();
	i1 = uint32_t(Triangles.back());
	Triangles.pop_back();
	i2 = uint32_t(Triangles.back());
	Triangles.pop_back();
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

void getVoxelQuads(openvdb::FloatGrid::ConstPtr grid, QuadVec &quads, std::vector<openvdb::Coord> &vertices)
{
    QuadSet uniqueQuads[ORIENTATION_COUNT];
    openvdb::Int64Grid::Ptr visitedVertexIndices = openvdb::Int64Grid::create(-1);
	visitedVertexIndices->setTransform(grid->transformPtr()->copy());
    visitedVertexIndices->topologyUnion(*grid);
	//visitedVertexIndices->setTree(grid->treePtr()->copy());
    openvdb::Vec3d voxelSize = grid->voxelSize();

	for (auto i = grid->cbeginValueOn(); i; ++i)
	{
		if (i.isVoxelValue())
		{
			std::vector<VertexByIndex> cubeVertices;
			openvdb::Coord origin = i.getCoord();
			openvdb::Coord opposite = openvdb::Coord(origin.x() + 1, origin.y() + 1, origin.z() + 1);

			cubeVertices.push_back(VertexByIndex(origin, -1));
			cubeVertices.push_back(VertexByIndex(openvdb::Coord(origin.x() + 1, origin.y(), origin.z()), -1));
			cubeVertices.push_back(VertexByIndex(openvdb::Coord(origin.x(), origin.y() + 1, origin.z()), -1));
			cubeVertices.push_back(VertexByIndex(openvdb::Coord(origin.x(), origin.y(), origin.z() + 1), -1));
			cubeVertices.push_back(VertexByIndex(opposite, -1));
			cubeVertices.push_back(VertexByIndex(openvdb::Coord(opposite.x() - 1, opposite.y(), opposite.z()), -1));
			cubeVertices.push_back(VertexByIndex(openvdb::Coord(opposite.x(), opposite.y() - 1, opposite.z()), -1));
			cubeVertices.push_back(VertexByIndex(openvdb::Coord(opposite.x(), opposite.y(), opposite.z() - 1), -1));

            openvdb::Int64Grid::Accessor visited = visitedVertexIndices->getAccessor();
			for (auto c = cubeVertices.begin(); c != cubeVertices.end(); c++)
			{
                IndexType value = -1;
				if (visited.getValue(c->coord) < 0)
				{
					vertices.push_back(c->coord);
                    value = IndexType(vertices.size() - 1);
					visited.setValue(c->coord, value); //TODO:: error check index ranges                    
				}
				else
				{
                    value = visited.getValue(c->coord);
				}
                c->index = value;
			}

			Quad q1(XY, grid, vertices, cubeVertices[0].index, cubeVertices[1].index, cubeVertices[2].index, cubeVertices[7].index, voxelSize.x(), voxelSize.y());
			Quad q2(XZ, grid, vertices, cubeVertices[0].index, cubeVertices[3].index, cubeVertices[1].index, cubeVertices[6].index, voxelSize.x(), voxelSize.z());
			Quad q3(YZ, grid, vertices, cubeVertices[0].index, cubeVertices[2].index, cubeVertices[3].index, cubeVertices[6].index, voxelSize.y(), voxelSize.z());
			Quad q4(XY, grid, vertices, cubeVertices[4].index, cubeVertices[5].index, cubeVertices[6].index, cubeVertices[3].index, voxelSize.x(), voxelSize.y());
			Quad q5(XZ, grid, vertices, cubeVertices[4].index, cubeVertices[5].index, cubeVertices[7].index, cubeVertices[2].index, voxelSize.x(), voxelSize.z());
			Quad q6(YZ, grid, vertices, cubeVertices[4].index, cubeVertices[6].index, cubeVertices[7].index, cubeVertices[1].index, voxelSize.y(), voxelSize.z());
			uniqueQuads[XY].insert(q1);
			uniqueQuads[XZ].insert(q2);
			uniqueQuads[YZ].insert(q3);
			uniqueQuads[XY].insert(q4);
			uniqueQuads[XZ].insert(q5);
			uniqueQuads[YZ].insert(q6);
		}
	}

    for (auto i = 0; i < ORIENTATION_COUNT; i++)
    {
        for (auto j = uniqueQuads[i].cbegin(); j != uniqueQuads[i].end(); j++)
        {
            quads.push_back(*j);
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
			if (j->isMerged ||
				!openvdb::math::isApproxEqual(j->worldOrigin().z(), i->worldOrigin().z()))
			{
				continue; //TODO: Figure out if 'isMerged' check here is necessary. Since we're greedy meshing, it may not ever matter to check here
			}

			//Check if we can merge by width
			if (openvdb::math::isApproxEqual(j->worldOrigin().y(), i->worldOrigin().y()) && //Same location y-axis...
                openvdb::math::isApproxEqual(j->worldOrigin().x() - i->worldOrigin().x(), i->width) && //Adjacent...
                openvdb::math::isApproxEqual(j->height, i->height)) //Same height
			{
				i->width += j->width;
				i->v3 = j->v3;
				i->v4 = j->v4;
			}
            //Check if we can merge by height
			else if (openvdb::math::isApproxEqual(j->worldOrigin().x(), i->worldOrigin().x()) && //Same location x-axis...
                     openvdb::math::isApproxEqual(j->worldOrigin().y() - i->worldOrigin().y(), i->height) && //Adjacent...
                     openvdb::math::isApproxEqual(j->width, i->width)) //Same width
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