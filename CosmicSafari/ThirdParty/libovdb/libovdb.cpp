#include "libovdb.h"
#include <openvdb/openvdb.h>
#include <openvdb/Exceptions.h>
#include <openvdb/tools/Dense.h>
#include <openvdb/tools/VolumeToMesh.h>
#include <openvdb/tools/Clip.h>
#include <fstream>
#include <map>

static openvdb::FloatGrid::Ptr SparseGrids = nullptr;
static std::vector<openvdb::Vec3d> Vertices;
static std::vector<openvdb::Index32> Triangles;

enum Component { cX, cY, cZ };
typedef struct _Quad
{
	const std::vector<openvdb::Vec3d> * vertices;
	openvdb::Vec4I p;
	double width;
	double height;
	bool isMerged;
	Component lz, ly, lx; //Which vertex indices to refer to when comparing quads
	_Quad(const std::vector<openvdb::Vec3d> const * v, double w, double h) : vertices(v), width(w), height(h), isMerged(false) {}
	void _Quad::setLocal(Component x, Component y, Component z) { lx = x; ly = y; lz = z; }
	//quad[0] is always the origin
	double _Quad::localZ() const { return vertices->at(p[0])[lz]; } //Get the component perpindicular to the quad face
	double _Quad::localY() const { return vertices->at(p[0])[ly]; } //Get the first component of the quad vertex
	double _Quad::localX() const { return vertices->at(p[0])[lx]; } //Get the second component of the quad vertex
} Quad;

//Sort quads by a total ordering
//(via Mikola Lysenko at http://0fps.net/2012/06/30/meshing-in-a-minecraft-game)
inline bool compareQuads(const Quad &l, const Quad &r)
{
	if (!openvdb::math::isApproxEqual(l.localZ(), r.localZ())) return l.localZ() < r.localZ();
	if (!openvdb::math::isApproxEqual(l.localY(), r.localY())) return l.localY() < r.localY();
	if (!openvdb::math::isApproxEqual(l.localX(), r.localX())) return l.localX() < r.localX();
	if (!openvdb::math::isApproxEqual(l.width, r.width)) return l.width > r.width;
	return openvdb::math::isApproxEqual(l.height, r.height) || l.height > r.height;
}

std::string gridNamesList(const openvdb::io::File &file);
void getPrimitiveQuads(openvdb::FloatGrid::ConstPtr grid, std::vector<openvdb::Vec3d> * vertices, std::vector<Quad> &xyQuads, std::vector<Quad> &yzQuads, std::vector<Quad> &xzQuads);
void mergeQuads(std::vector<Quad> &quads);
void getCornerVertices(openvdb::FloatGrid::ConstPtr grid, std::vector<openvdb::Vec3d> * cornerVertices, openvdb::Vec3I &dimensions);

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
		std::vector<Quad> xyQuads;
		std::vector<Quad> yzQuads;
		std::vector<Quad> xzQuads;
		getPrimitiveQuads(SparseGrids, &Vertices, xyQuads, yzQuads, xzQuads);
		mergeQuads(xyQuads);
		mergeQuads(yzQuads);
		mergeQuads(xzQuads);

		for (std::vector<Quad>::const_iterator i = xyQuads.begin(); i != xyQuads.end(); i++)
		{
			if (i->isMerged)
			{
				continue;
			}
			//First triangle of the quad
			Triangles.push_back(i->p[0]);
			Triangles.push_back(i->p[3]);
			Triangles.push_back(i->p[1]);
			//Second triangle of the quad
			Triangles.push_back(i->p[0]);
			Triangles.push_back(i->p[3]);
			Triangles.push_back(i->p[2]);
		}
		for (std::vector<Quad>::const_iterator i = yzQuads.begin(); i != yzQuads.end(); i++)
		{
			if (i->isMerged)
			{
				continue;
			}
			//First triangle of the quad
			Triangles.push_back(i->p[0]);
			Triangles.push_back(i->p[3]);
			Triangles.push_back(i->p[1]);
			//Second triangle of the quad
			Triangles.push_back(i->p[0]);
			Triangles.push_back(i->p[3]);
			Triangles.push_back(i->p[2]);
		}
		for (std::vector<Quad>::const_iterator i = xzQuads.begin(); i != xzQuads.end(); i++)
		{
			if (i->isMerged)
			{
				continue;
			}
			//First triangle of the quad
			Triangles.push_back(i->p[0]);
			Triangles.push_back(i->p[3]);
			Triangles.push_back(i->p[1]);
			//Second triangle of the quad
			Triangles.push_back(i->p[0]);
			Triangles.push_back(i->p[3]);
			Triangles.push_back(i->p[2]);
		}

		//if (!(regionCountX && regionCountY && regionCountZ))
		//{
		//	OPENVDB_THROW(openvdb::ValueError, "Mesh chunk sizes must be greater than 0");
		//}
		//openvdb::math::CoordBBox boundingBox = SparseGrids->evalActiveVoxelBoundingBox();
		//openvdb::Coord bounds = boundingBox.max() - boundingBox.min();
		//openvdb::Int32 spanX = bounds.x() / openvdb::Int32(regionCountX);
		//openvdb::Int32 spanY = bounds.y() / openvdb::Int32(regionCountY);
		//openvdb::Int32 spanZ = bounds.z() / openvdb::Int32(regionCountZ);
		////TODO: Get remainder region (or just constrain region sizes to evenly divide map size)
		////openvdb::Int32 remSpanX = bounds.x() % openvdb::Int32(regionCountX);
		////openvdb::Int32 remSpanY = bounds.y() % openvdb::Int32(regionCountY);
		////openvdb::Int32 remSpanZ = bounds.z() % openvdb::Int32(regionCountZ);
		//
		//openvdb::Int32 regionCount = regionCountX * regionCountY * regionCountZ;
		//std::vector< std::vector<Quad> > regionQuads;
		//for (openvdb::Int32 i = 0; i < regionCount; i++)
		//{
		//	//Create the quad vector for each region now to avoid deep copies later
		//	std::vector<Quad> q;
		//	regionQuads.push_back(q);
		//}

		//for (openvdb::Int32 x = 0; x < openvdb::Int32(regionCountX); x += 2)
		//{
		//	for (openvdb::Int32 y = 0; y < openvdb::Int32(regionCountY); y += 2)
		//	{
		//		for (openvdb::Int32 z = 0; z < openvdb::Int32(regionCountZ); z += 2)
		//		{
		//			openvdb::Vec3d regionStart = SparseGrids->indexToWorld(openvdb::Coord(x*spanX, y*spanY, z*spanZ) + boundingBox.min());
		//			openvdb::Vec3d regionEnd = SparseGrids->indexToWorld(openvdb::Coord((x+1)*spanX, (y+1)*spanY, (z+1)*spanZ) + boundingBox.min());
		//			openvdb::FloatGrid::Ptr region = openvdb::tools::clip(*SparseGrids, openvdb::BBoxd(regionStart, regionEnd));
		//			getPrimitiveQuads(region, regionQuads[x*regionCountX + y*regionCountY + z*regionCountZ]);
		//		}
		//	}
		//}

		//for (std::vector< std::vector<Quad> >::iterator i = regionQuads.begin(); i != regionQuads.end(); i++)
		//{
		//	mergeQuads(*i);
		//}
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
	if (Vertices.empty())
	{
		return 0;
	}
	openvdb::Vec3d v = Vertices.back();
	Vertices.pop_back();
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
	i0 = Triangles.back();
	Triangles.pop_back();
	i1 = Triangles.back();
	Triangles.pop_back();
	i2 = Triangles.back();
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

void getCornerVertices(openvdb::FloatGrid::ConstPtr grid, std::vector<openvdb::Vec3d> * cornerVertices, openvdb::Vec3I &dimensions)
{
	openvdb::FloatGrid::ConstAccessor accessor = grid->getAccessor();
	openvdb::math::CoordBBox bbox = grid->evalActiveVoxelBoundingBox();
	openvdb::Int32 xDim = 0;
	openvdb::Int32 yDim = 0;
	openvdb::Int32 zDim = 0;
	for (openvdb::Int32 x = bbox.min().x(); x <= bbox.max().x(); x++)
	{
		xDim = openvdb::math::Max(xDim, x);
		for (openvdb::Int32 y = bbox.min().y(); y <= bbox.max().y(); y++)
		{
			yDim = openvdb::math::Max(yDim, y);
			for (openvdb::Int32 z = bbox.min().z(); z <= bbox.max().z(); z++)
			{
				openvdb::Coord xyz(x, y, z);
				if (accessor.isVoxel(xyz) &&
					accessor.isValueOn(xyz))
				{
					zDim = openvdb::math::Max(zDim, z);
					cornerVertices->push_back(xyz.asVec3d());
				}
			}
		}
	}
	dimensions = openvdb::Vec3I(xDim, yDim, zDim);
}

void getPrimitiveQuads(openvdb::FloatGrid::ConstPtr grid, std::vector<openvdb::Vec3d> * vertices, std::vector<Quad> &xyQuads, std::vector<Quad> &yzQuads, std::vector<Quad> &xzQuads)
{
	openvdb::Vec3I dimensions;
	getCornerVertices(grid, vertices, dimensions);

	//For each corner vertex get the indices of the vertices which make up the 3 quads from the corner
	for (openvdb::Index32 x = 0; x < dimensions.x(); x++)
	{
		for (openvdb::Index32 y = 0; y < dimensions.y(); y++)
		{
			for (openvdb::Index32 z = 0; z < dimensions.z(); z++)
			{
				openvdb::Index32 i = x*(dimensions.x()) + y*(dimensions.y()) + z;

				//XY quad
				Quad xy(vertices, grid->voxelSize().x(), SparseGrids->voxelSize().y());
				xy.p[0] = i;
				xy.p[1] = i + dimensions.y()*dimensions.z(); 
				xy.p[2] = i + dimensions.z();
				xy.p[3] = i + dimensions.y()*(dimensions.z() + 1);
				xy.setLocal(cX, cY, cZ);
				//YZ quad
				Quad yz(vertices, grid->voxelSize().y(), SparseGrids->voxelSize().z());
				yz.p[0] = i;
				yz.p[1] = i + dimensions.z();
				yz.p[2] = i + 1;
				yz.p[3] = i + dimensions.z() + 1;
				yz.setLocal(cY, cZ, cX);
				//XZ quad
				Quad xz(vertices, grid->voxelSize().x(), SparseGrids->voxelSize().z());
				xz.p[0] = i;
				xz.p[1] = i + dimensions.y()*dimensions.z(); 
				xz.p[2] = i + 1;
				xz.p[3] = i + dimensions.y()*dimensions.z() + 1;
				xz.setLocal(cX, cZ, cY);

				xyQuads.push_back(xy);
				yzQuads.push_back(yz);
				xzQuads.push_back(xz);
			}
		}
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
		for (; j != quads.end() && openvdb::math::isApproxEqual(j->localZ(), i->localZ()); j++)
		{
			if (j->isMerged)
			{
				continue; //TODO: Figure out if on/off check here is necessary. Since we're greedy meshing, it may not ever matter to check here
			}
			//Check if we can merge one direction
			if (openvdb::math::isApproxEqual(j->localY(), i->localY()) && //Same vertical location...
				openvdb::math::isApproxEqual(j->localX() - i->localX(), i->width) && //Adjacent...
				openvdb::math::isApproxEqual(j->height, i->height)) //Same height
			{
				i->width += j->width;
				i->p[1] = j->p[1];
				i->p[3] = j->p[3];
			}
			//Can't merge in that direction so check if we can merge the other direction
			else if (openvdb::math::isApproxEqual(j->localX(), i->localX()) && //Same horizontal location...
					 openvdb::math::isApproxEqual(j->localY() - i->localY(), i->height) && //Adjacent...
					 openvdb::math::isApproxEqual(j->width, i->width)) //Same width
			{
				i->height += j->height;
				i->p[2] = j->p[2];
				i->p[3] = j->p[3];
			}
			else //Done with merging since we could no longer merge in either direction
			{
				break;
			}
			j->isMerged = true; //Mark this quad as merged so that it won't be meshed
		}
	}
}