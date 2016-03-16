#pragma once
#include "OvdbTypes.h"
#include "OvdbQuad.h"
#include "tbb/mutex.h"

using namespace ovdb; //TODO: Remove 'using' to match UE4 coding standards
using namespace ovdb::meshing; //TODO: Remove 'using' to match UE4 coding standards

template<typename ValueType>
struct BasicModifyOp
{
	const ValueType val;
	BasicModifyOp(const ValueType& v) : val(v) {}
	virtual inline void operator()(ValueType& v) const { v = val; }
};

template<typename ValueType>
struct BasicModifyActiveOp
{
	const ValueType val;
	const bool isActive;
	BasicModifyActiveOp(const ValueType& v, const bool &active) : val(v), isActive(active) {}
	virtual inline void operator()(ValueType& v, bool &activeState) const { v = val; activeState = isActive; }
};

template<typename InIterType, typename OutTreeType>
class TransformOp
{
public:
	typedef typename InIterType IterType;
	typedef typename InIterType::ValueT InValueType;
	typedef typename OutTreeType::ValueType OutValueType;
	typedef typename openvdb::Grid<OutTreeType> OutGridType;
	typedef typename openvdb::tree::ValueAccessor<OutTreeType> OutAccessorType;
	virtual void operator()(const IterType &iter, OutAccessorType& acc) = 0;
};

//Operator to create a grid of vectors from a scalar
template <typename OutTreeType, typename InTreeType = openvdb::BoolTree>
class PerlinNoiseFillOp : public TransformOp<typename InTreeType::ValueOnCIter, OutTreeType>
{
public:
	typedef typename BasicModifyActiveOp<OutValueType> ModifyOpType;
	PerlinNoiseFillOp(openvdb::math::Transform &xform, double frequency, double lacunarity, double persistence, int octaveCount)
		: inTreeXform(xform)
	{
		valueSource.SetFrequency(frequency);
		valueSource.SetLacunarity(lacunarity);
		valueSource.SetPersistence(persistence);
		valueSource.SetOctaveCount(octaveCount);
	}
	void operator()(const IterType& iter, OutAccessorType& acc) override
	{
		const bool isInsideBoundary = iter.getValue();
		if (iter.isVoxelValue())
		{
			const openvdb::Coord &coord = iter.getCoord();
			//Set the density value from the noise source and set on if the voxel is in the active boundary
			const openvdb::Vec3d vec = inTreeXform.indexToWorld(coord);
			acc.modifyValueAndActiveState<ModifyOpType>(coord, ModifyOpType((OutValueType)(valueSource.GetValue(vec.x(), vec.y(), vec.z()) - vec.z()), isInsideBoundary));
		}
		else
		{
			openvdb::CoordBBox bbox;
			iter.getBoundingBox(bbox);
			openvdb::Coord coord;
			for (auto x = bbox.min().x(); x <= bbox.max().x(); ++x)
			{
				coord.setX(x);
				for (auto y = bbox.min().y(); y <= bbox.max().y(); ++y)
				{
					coord.setY(y);
					for (auto z = bbox.min().z(); z <= bbox.max().z(); ++z)
					{
						coord.setZ(z);
						const openvdb::Vec3d vec = inTreeXform.indexToWorld(coord);
						acc.modifyValueAndActiveState<ModifyOpType>(coord, ModifyOpType((OutValueType)(valueSource.GetValue(vec.x(), vec.y(), vec.z()) - vec.z()), isInsideBoundary));
					}
				}
			}
		}
	}
private:
	const openvdb::math::Transform &inTreeXform;
	noise::module::Perlin valueSource;
};

//Operator to extract an isosurface from a grid
template <typename OutTreeType, typename InTreeType = OutTreeType>
class ExtractSurfaceOp : public TransformOp<typename InTreeType::ValueOnIter, OutTreeType>
{
public:
	typedef typename boost::shared_ptr<ExtractSurfaceOp<OutTreeType>> Ptr;
	void SetSurfaceValue(InValueType isovalue)
	{
		surfaceValue = isovalue;
	}
	void operator()(const IterType& iter, OutAccessorType& acc) override
	{
		const openvdb::Coord &coord = iter.getCoord();
		uint8_t insideBits = 0;
		//For each neighboring value set a bit if it is inside the surface (inside = positive value)
		if (iter.getValue() > surfaceValue) { insideBits |= 1; }
		if (acc.getValue(coord.offsetBy(1, 0, 0)) > surfaceValue) { insideBits |= 2; }
		if (acc.getValue(coord.offsetBy(0, 1, 0)) > surfaceValue) { insideBits |= 4; }
		if (acc.getValue(coord.offsetBy(0, 0, 1)) > surfaceValue) { insideBits |= 8; }
		if (acc.getValue(coord.offsetBy(1, 1, 0)) > surfaceValue) { insideBits |= 16; }
		if (acc.getValue(coord.offsetBy(1, 0, 1)) > surfaceValue) { insideBits |= 32; }
		if (acc.getValue(coord.offsetBy(0, 1, 1)) > surfaceValue) { insideBits |= 64; }
		if (acc.getValue(coord.offsetBy(1, 1, 1)) > surfaceValue) { insideBits |= 128; }
		//If all vertices are inside the surface or all are outside the surface then set off in order to not mesh this voxel
		iter.setActiveState(insideBits > 0 && insideBits < 255);
	}
private:
	InValueType surfaceValue;
};

template <typename OutTreeType, typename InTreeType>
class MeshGeometryOp : public TransformOp<typename InTreeType::ValueOnCIter, OutTreeType>
{
public:
	typedef typename boost::shared_ptr<MeshGeometryOp<typename OutTreeType, typename InTreeType>> Ptr;

	MeshGeometryOp(const openvdb::math::Transform &sourceXform, tbb::mutex &mutex) : xform(sourceXform), vertexMutex(mutex)
	{
	}
	
	void operator()(const IterType& iter, OutAccessorType& acc) override
	{
		const openvdb::Coord coord = iter.getCoord();
		const InValueType &density = iter.getValue();
		//Mesh the voxel as a simple cube with 6 equal sized quads
		PrimitiveCube primitiveIndices(coord);
		for (uint32_t i = 0; i < CUBE_VERTEX_COUNT; ++i)
		{
			const CubeVertex &vtx = (CubeVertex)i;
			const openvdb::Coord &idxCoord = primitiveIndices.getCoord(vtx);
			{
				tbb::mutex::scoped_lock lock(vertexMutex); //Lock the mutex for the duration of this scope
				OutValueType vertexIndex = acc.getValue(idxCoord);
				if (vertexIndex == acc.getTree()->background())
				{
					vertexIndex = (OutValueType)(vertices.size()); //TODO: Error check index ranges
					vertices.push_back(xform.indexToWorld(idxCoord));
					//Since this is a new vertex save it to the global visited vertex grid for use by any other voxels in the same region that share it
					acc.setValue(idxCoord, vertexIndex);
				}
				primitiveIndices[vtx] = vertexIndex;
			}
		}
		quads.push_back(OvdbQuad(primitiveIndices.getQuadXY0()));
		quads.push_back(OvdbQuad(primitiveIndices.getQuadXY1()));
		quads.push_back(OvdbQuad(primitiveIndices.getQuadXZ0()));
		quads.push_back(OvdbQuad(primitiveIndices.getQuadXZ1()));
		quads.push_back(OvdbQuad(primitiveIndices.getQuadYZ0()));
		quads.push_back(OvdbQuad(primitiveIndices.getQuadYZ1()));
	}

	void initialize()
	{
		vertices.clear();
		polygons.clear();
		normals.clear();
		quads.clear();
	}

	void collectGeometry()
	{
		//Collect geometry into geometry containers
		//for (UniqueQuadsType::const_iterator i = quads.begin(); i != quads.end(); ++i)
		for (tbb::concurrent_vector<OvdbQuad>::const_iterator i = quads.begin(); i != quads.end(); ++i)
		{
			//const OvdbQuad &q = i->second;
			const OvdbQuad &q = *i;
			if (q.quadIsMerged())
			{
				continue;
			}
			//Collect triangle indices of the two triangles comprising this quad
			polygons.push_back(q.quadPoly1());
			polygons.push_back(q.quadPoly2());
			//normals.push_back();
			//normals.push_back();
			//normals.push_back();
			//normals.push_back();
		}
		//Clear the quads list to be ready for next mesh
		quads.clear();

		//Reset container iterators
		currentVertex = vertices.begin();
		currentPolygon = polygons.begin();
		currentNormal = normals.begin();
	}

	size_t vertexCount()
	{
		return vertices.size();
	}

	bool nextVertex(double &v1, double &v2, double &v3)
	{
		if (currentVertex == vertices.end())
		{
			return false;
		}
		v1 = (*currentVertex)[0];
		v2 = (*currentVertex)[1];
		v3 = (*currentVertex)[2];
		return ++currentVertex != vertices.end();
	}

	size_t polygonCount()
	{
		return polygons.size();
	}

	bool nextPolygon(uint32_t &p1, uint32_t &p2, uint32_t &p3)
	{
		if (currentPolygon == polygons.end())
		{
			return false;
		}
		p1 = (*currentPolygon)[0];
		p2 = (*currentPolygon)[1];
		p3 = (*currentPolygon)[2];
		return ++currentPolygon != polygons.end();
	}

	size_t normalCount()
	{
		return normals.size();
	}

	bool nextNormal(double &n1, double &n2, double &n3)
	{
		if (currentNormal == normals.end())
		{
			return false;
		}
		n1 = (*currentNormal)[0];
		n2 = (*currentNormal)[1];
		n3 = (*currentNormal)[2];
		return ++currentNormal != normals.end();
	}

private:
	const openvdb::math::Transform &xform;
	tbb::concurrent_vector<OvdbQuad> quads;
	tbb::mutex &vertexMutex;
	VolumeVerticesType vertices;
	VolumePolygonsType polygons;
	VolumeNormalsType normals;
	VolumeVerticesType::const_iterator currentVertex;
	VolumePolygonsType::const_iterator currentPolygon;
	VolumeNormalsType::const_iterator currentNormal;
};

//Helper struct to hold the associated grid meshing info
template <typename TreeType, typename IndexTreeType = openvdb::UInt32Tree>
class BasicMesher
{
	//collectGeometry()
public:
	typedef typename boost::shared_ptr<BasicMesher<TreeType>> Ptr;
	typedef typename ExtractSurfaceOp<TreeType> ActivateValuesOpType;
	typedef typename MeshGeometryOp<IndexTreeType, TreeType> MeshOpType;
	typedef typename openvdb::Grid<TreeType> GridType;
	typedef typename openvdb::Grid<IndexTreeType> VertexIndexGridType;
	typedef typename GridType::Ptr GridPtrType;	
	const GridPtrType gridPtr;
	const typename MeshOpType::Ptr meshOp;

	BasicMesher(GridPtrType grid)
		: gridPtr(grid), meshOp(new MeshOpType(grid->transform(), meshOpMutex)), activateValuesOp(new ActivateValuesOpType()) {}

	void doActivateValuesOp(typename ActivateValuesOpType::InValueType isovalue)
	{
		activateValuesOp->SetSurfaceValue(isovalue);
		openvdb::tools::transformValues<ActivateValuesOpType::IterType, ActivateValuesOpType::OutGridType, ActivateValuesOpType>(gridPtr->beginValueOn(), *gridPtr, *activateValuesOp);
	}

	void doMeshOp(bool initialize = false)
	{
		if (initialize || visitedVertexIndices == nullptr)
		{
			visitedVertexIndices = VertexIndexGridType::create((MeshOpType::OutValueType)UNVISITED_VERTEX_INDEX);
			visitedVertexIndices->setTransform(gridPtr->transformPtr()->copy());
			visitedVertexIndices->topologyUnion(*gridPtr);
			meshOp->initialize();
		}
		openvdb::tools::transformValues<MeshOpType::IterType, MeshOpType::OutGridType, MeshOpType>(gridPtr->cbeginValueOn(), *visitedVertexIndices, *meshOp);
		meshOp->collectGeometry();
	}

private:
	typename ActivateValuesOpType::Ptr activateValuesOp;
	typename VertexIndexGridType::Ptr visitedVertexIndices;
	tbb::mutex meshOpMutex;
};