#pragma once
#include "EngineMinimal.h"
#pragma warning(push)
#pragma warning(1:4211 4800 4503 4146)
#include <openvdb/openvdb.h>
#include <openvdb/tools/ValueTransformer.h>
#include <noise.h>
#include <noiseutils.h>

namespace Vdb
{
	namespace GridOps
	{
		template<typename InIterT, typename OutGridT, typename XformOp>
		inline void transformValuesSharedOpOnly(const InIterT& inIter, OutGridT& outGrid, XformOp& op, bool threaded = true, openvdb::MergePolicy merge = openvdb::MERGE_ACTIVE_STATES)
		{
			typedef openvdb::TreeAdapter<OutGridT> Adapter;
			typedef typename Adapter::TreeType OutTreeT;
			typedef typename openvdb::tools::valxform::SharedOpTransformer<InIterT, OutTreeT, XformOp> Processor;
			Processor proc(inIter, Adapter::tree(outGrid), op, merge);
			proc.process(threaded);
		}

		typedef int32 IndexType;
		typedef openvdb::tree::Tree4<IndexType, 5, 4, 3>::Type IndexTreeType; //Same tree configuration (5,4,3) as openvdb::FloatTree (see openvdb.h)
		const static IndexType UNVISITED_VERTEX_INDEX = -1;
		const static uint32 CUBE_VERTEX_COUNT = 8;

		//Helper class that holds coordinates of a cube
		class PrimitiveCube
		{
		public:
			PrimitiveCube(const openvdb::Coord &cubeStart)
			{
				openvdb::CoordBBox bbox = openvdb::CoordBBox::createCube(cubeStart, 1);
				primitiveVertices[0] = bbox.getStart();
				primitiveVertices[1] = bbox.getStart().offsetBy(1, 0, 0);
				primitiveVertices[2] = bbox.getStart().offsetBy(0, 1, 0);
				primitiveVertices[3] = bbox.getStart().offsetBy(0, 0, 1);
				primitiveVertices[4] = bbox.getEnd().offsetBy(-1, 0, 0);
				primitiveVertices[5] = bbox.getEnd().offsetBy(0, -1, 0);
				primitiveVertices[6] = bbox.getEnd().offsetBy(0, 0, -1);
				primitiveVertices[7] = bbox.getEnd();
			}

			IndexType& operator[](uint32 v) { return primitiveIndices[v]; }
			openvdb::Coord& getCoord(uint32 v) { return primitiveVertices[v]; }
			//Add the vertex indices in counterclockwise order on each quad face
			openvdb::Vec4i getQuadXY0() { return openvdb::Vec4i(primitiveIndices[3], primitiveIndices[4], primitiveIndices[7], primitiveIndices[5]); }
			openvdb::Vec4i getQuadXY1() { return openvdb::Vec4i(primitiveIndices[6], primitiveIndices[2], primitiveIndices[0], primitiveIndices[1]); }
			openvdb::Vec4i getQuadXZ0() { return openvdb::Vec4i(primitiveIndices[7], primitiveIndices[4], primitiveIndices[2], primitiveIndices[6]); }
			openvdb::Vec4i getQuadXZ1() { return openvdb::Vec4i(primitiveIndices[5], primitiveIndices[1], primitiveIndices[0], primitiveIndices[3]); }
			openvdb::Vec4i getQuadYZ0() { return openvdb::Vec4i(primitiveIndices[7], primitiveIndices[6], primitiveIndices[1], primitiveIndices[5]); }
			openvdb::Vec4i getQuadYZ1() { return openvdb::Vec4i(primitiveIndices[0], primitiveIndices[2], primitiveIndices[4], primitiveIndices[3]); }
		private:
			openvdb::Coord primitiveVertices[CUBE_VERTEX_COUNT];
			IndexType primitiveIndices[CUBE_VERTEX_COUNT];
		};

		//Helper class that holds vertex indices of the quad-face of a cube
		class OvdbQuad
		{
		public:
			OvdbQuad() {}
			OvdbQuad(openvdb::Vec4i idxs) : indices(idxs), isMerged(false) {}
			OvdbQuad(const OvdbQuad &rhs) : indices(rhs.indices), isMerged(rhs.isMerged) {}
			OvdbQuad& operator=(const OvdbQuad &rhs)
			{
				indices = rhs.indices;
				isMerged = rhs.isMerged;
				return *this;
			}
			const openvdb::Vec4i& quad() const { return indices; }
			openvdb::Vec2i quadU() const { return openvdb::Vec2i(indices[0], indices[1]); }
			openvdb::Vec2i quadV() const { return openvdb::Vec2i(indices[0], indices[3]); }
			openvdb::Vec3i quadPoly1() const { return openvdb::Vec3i(indices[0], indices[1], indices[2]); }
			openvdb::Vec3i quadPoly2() const { return openvdb::Vec3i(indices[0], indices[2], indices[3]); }
			bool quadIsMerged() const { return isMerged; }
			void setIsMerged() { isMerged = true; }
			void mergeU(OvdbQuad &rhs)
			{
				if (!rhs.quadIsMerged())
				{
					rhs.setIsMerged();
					indices = openvdb::Vec4i(indices[0], rhs.indices[1], rhs.indices[2], indices[3]);
				}
			}
			void mergeV(OvdbQuad &rhs)
			{
				if (!rhs.quadIsMerged())
				{
					rhs.setIsMerged();
					indices = openvdb::Vec4i(indices[0], indices[1], rhs.indices[2], rhs.indices[3]);
				}
			}
		private:
			openvdb::Vec4i indices;
			bool isMerged;
		};

		template<typename ValueType>
		struct BasicModifyOp
		{
			const ValueType val;
			BasicModifyOp(const ValueType &v) : val(v) {}
			inline void BasicModifyOp<ValueType>::operator()(ValueType &v) const
			{
				v = val;
			}
		};

		template<typename ValueType>
		struct BasicModifyActiveOp
		{
			const ValueType val;
			const bool isActive;
			BasicModifyActiveOp(const ValueType& v, const bool &active) : val(v), isActive(active) {}
			inline void operator()(ValueType& v, bool &activeState) const
			{
				v = val;
				activeState = isActive;
			}
		};

		//Virtual interface specification for a grid transformer operator
		template<typename InIterType, typename OutTreeType>
		class ITransformOp
		{
		public:
			typedef typename InIterType IterType;
			typedef typename InIterType::ValueT InValueType;
			typedef typename OutTreeType::ValueType OutValueType;
			typedef typename openvdb::Grid<OutTreeType> OutGridType;
			typedef typename openvdb::tree::ValueAccessor<OutTreeType> OutAccessorType;
			ITransformOp(const openvdb::math::Transform &xform) : inTreeXform(xform) {}
			virtual inline void operator()(const IterType &iter, OutAccessorType& acc) = 0;
		protected:
			const openvdb::math::Transform &inTreeXform;
		};

		//Operator to fill a grid with Perlin noise values
		template <typename OutTreeType, typename InTreeType = openvdb::BoolTree>
		class PerlinNoiseFillOp : public ITransformOp<typename InTreeType::ValueOnCIter, OutTreeType>
		{
		public:
			typedef typename BasicModifyActiveOp<OutValueType> ModifyOpType;
			PerlinNoiseFillOp(const openvdb::math::Transform &xform, float frequency, float lacunarity, float persistence, int32 octaveCount) : ITransformOp(xform)
			{
				valueSource.SetFrequency((double)frequency);
				valueSource.SetLacunarity((double)lacunarity);
				valueSource.SetPersistence((double)persistence);
				valueSource.SetOctaveCount(octaveCount);
			}
			inline void operator()(const IterType& iter, OutAccessorType& acc) override
			{
				bool isInsideBoundary = iter.getValue();
				if (iter.isVoxelValue())
				{
					openvdb::Coord coord = iter.getCoord();
					//Set the density value from the noise source and set on if the voxel is in the active boundary
					openvdb::Vec3d vec = inTreeXform.indexToWorld(coord);
					//acc.modifyValueAndActiveState<ModifyOpType>(coord, ModifyOpType(GetDensityValue(vec), isInsideBoundary));
					if (isInsideBoundary)
					{
						acc.setValueOn(coord, GetDensityValue(vec));
					}
					else
					{
						acc.setValueOnly(coord, GetDensityValue(vec));
					}
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
								openvdb::Vec3d vec = inTreeXform.indexToWorld(coord);
								//acc.modifyValueAndActiveState<ModifyOpType>(coord, ModifyOpType(GetDensityValue(vec), isInsideBoundary));
								if (isInsideBoundary)
								{
									acc.setValueOn(coord, GetDensityValue(vec));
								}
								else
								{
									acc.setValueOnly(coord, GetDensityValue(vec));
								}
							}
						}
					}
				}
			}
			inline OutValueType GetDensityValue(const openvdb::Vec3d &vec)
			{
				//double prevLacunarity = valueSource.GetLacunarity();
				//int32 prevOctaveCount = valueSource.GetOctaveCount();
				//valueSource.SetLacunarity(prevLacunarity*0.004);
				//valueSource.SetOctaveCount(2);
				//double warp = valueSource.GetValue(vec.x(), vec.y(), vec.z()) * 8;
				//valueSource.SetLacunarity(prevLacunarity);
				//valueSource.SetOctaveCount(prevOctaveCount);
				//return (OutValueType)(warp + valueSource.GetValue(vec.x(), vec.y(), vec.z()) - vec.z());
				return (OutValueType)(valueSource.GetValue(vec.x(), vec.y(), vec.z()) - vec.z());
			}
		private:
			noise::module::Perlin valueSource;
		};

		//Operator to extract an isosurface from a grid
		template <typename OutTreeType, typename InTreeType = OutTreeType>
		class ExtractSurfaceOp : public ITransformOp<typename InTreeType::ValueOnIter, OutTreeType>
		{
		public:
			typedef typename boost::shared_ptr<ExtractSurfaceOp<OutTreeType>> Ptr;
			ExtractSurfaceOp() : ITransformOp(openvdb::math::Transform()) {}
			inline void SetSurfaceValue(const InValueType &isovalue)
			{
				surfaceValue = isovalue;
			}
			inline void operator()(const IterType &iter, OutAccessorType &acc) override
			{
				openvdb::Coord coord = iter.getCoord();
				uint8 insideBits = 0;
				//For each neighboring value set a bit if it is inside the surface (inside = positive value)
				if (acc.getValue(coord) > surfaceValue)
				{
					insideBits |= 1;
				}
				if (acc.getValue(coord.offsetBy(1, 0, 0)) > surfaceValue)
				{
					insideBits |= 2;
				}
				if (acc.getValue(coord.offsetBy(0, 1, 0)) > surfaceValue)
				{
					insideBits |= 4;
				}
				if (acc.getValue(coord.offsetBy(0, 0, 1)) > surfaceValue)
				{
					insideBits |= 8;
				}
				if (acc.getValue(coord.offsetBy(1, 1, 0)) > surfaceValue)
				{
					insideBits |= 16;
				}
				if (acc.getValue(coord.offsetBy(1, 0, 1)) > surfaceValue)
				{
					insideBits |= 32;
				}
				if (acc.getValue(coord.offsetBy(0, 1, 1)) > surfaceValue)
				{
					insideBits |= 64;
				}
				if (acc.getValue(coord.offsetBy(1, 1, 1)) > surfaceValue)
				{
					insideBits |= 128;
				}
				//If all vertices are inside the surface or all are outside the surface then set off in order to not mesh this voxel
				if (insideBits == 0 || insideBits == 255)
				{
					iter.setValueOff();
				}
			}
		private:
			InValueType surfaceValue;
		};

		template <typename OutTreeType, typename InTreeType>
		class MeshGeometryOp : public ITransformOp<typename InTreeType::ValueOnCIter, OutTreeType>
		{
		public:
			typedef typename boost::shared_ptr<MeshGeometryOp<typename OutTreeType, typename InTreeType>> Ptr;
			MeshGeometryOp(const openvdb::math::Transform &sourceXform, TArray<FVector> &vertexBuffer, TArray<int32> &polygonBuffer, TArray<FVector> &normalBuffer)
				: ITransformOp(sourceXform), vertices(vertexBuffer), polygons(polygonBuffer), normals(normalBuffer) {}
			inline void operator()(const IterType &iter, OutAccessorType &acc) override
			{
				openvdb::Coord coord = iter.getCoord();
				InValueType density = iter.getValue();
				//Mesh the voxel as a simple cube with 6 equal sized quads
				PrimitiveCube primitiveIndices(coord);
				for (uint32 i = 0; i < CUBE_VERTEX_COUNT; ++i)
				{
					openvdb::Coord idxCoord = primitiveIndices.getCoord(i);
					OutValueType vertexIndex;
					{
						FScopeLock Lock(&CriticalSection);
						vertexIndex = acc.getValue(idxCoord);
						if (vertexIndex == acc.getTree()->background())
						{
							vertexIndex = vertices.Num(); //TODO: Error check index ranges
							openvdb::Vec3d vtx = inTreeXform.indexToWorld(idxCoord);
							vertices.Push(FVector((float)vtx.x(), (float)vtx.y(), (float)vtx.z()));
							//Since this is a new vertex save it to the global visited vertex grid for use by any other voxels in the same region that share it
							acc.setValue(idxCoord, vertexIndex);
						}
					}
					primitiveIndices[i] = vertexIndex;
				}
				quads.Enqueue(primitiveIndices.getQuadXY0());
				quads.Enqueue(primitiveIndices.getQuadXY1());
				quads.Enqueue(primitiveIndices.getQuadXZ0());
				quads.Enqueue(primitiveIndices.getQuadXZ1());
				quads.Enqueue(primitiveIndices.getQuadYZ0());
				quads.Enqueue(primitiveIndices.getQuadYZ1());
			}
			inline void initialize()
			{
				vertices.Empty();
				polygons.Empty();
				normals.Empty();
			}
			inline void collectPolygons()
			{
				openvdb::Vec4i q;
				while (quads.Dequeue(q))
				{
					polygons.Add(q[0]);
					polygons.Add(q[1]);
					polygons.Add(q[2]);
					polygons.Add(q[0]);
					polygons.Add(q[2]);
					polygons.Add(q[3]);
					//normals.Add()
					//normals.Add()
					//normals.Add()
					//normals.Add()
				}
			}
		private:
			FCriticalSection CriticalSection;
			TQueue<openvdb::Vec4i, EQueueMode::Mpsc> quads;
			TArray<FVector> &vertices;
			TArray<int32> &polygons;
			TArray<FVector> &normals;
		};

		//Helper struct to hold the associated grid meshing info
		template <typename TreeType, typename VertexIndexTreeType>
		class BasicMesher
		{
		public:
			typedef typename boost::shared_ptr<BasicMesher<TreeType, VertexIndexTreeType>> Ptr;
			typedef typename ExtractSurfaceOp<TreeType> ActivateValuesOpType;
			typedef typename ActivateValuesOpType::InValueType InValueType;
			typedef typename MeshGeometryOp<VertexIndexTreeType, TreeType> MeshOpType;
			typedef typename MeshOpType::OutValueType OutValueType;
			typedef typename openvdb::Grid<TreeType> GridType;
			typedef typename openvdb::Grid<VertexIndexTreeType> VertexIndexGridType;
			BasicMesher(typename GridType::Ptr grid, TArray<FVector> &vertexBuffer, TArray<int32> &polygonBuffer, TArray<FVector> &normalBuffer)
				: gridPtr(grid), meshOp(new MeshOpType(grid->transform(), vertexBuffer, polygonBuffer, normalBuffer)), activateValuesOp(new ActivateValuesOpType()) {}
			inline void doActivateValuesOp(InValueType isovalue)
			{
				activateValuesOp->SetSurfaceValue(isovalue);
				transformValuesSharedOpOnly<ActivateValuesOpType::IterType, ActivateValuesOpType::OutGridType, ActivateValuesOpType>(gridPtr->beginValueOn(), *gridPtr, *activateValuesOp);
			}
			inline void doMeshOp(bool initialize)
			{
				if (initialize || visitedVertexIndices == nullptr)
				{
					visitedVertexIndices = VertexIndexGridType::create(UNVISITED_VERTEX_INDEX);
					visitedVertexIndices->setTransform(gridPtr->transformPtr()->copy());
					visitedVertexIndices->topologyUnion(*gridPtr);
					meshOp->initialize();
				}
				transformValuesSharedOpOnly<MeshOpType::IterType, MeshOpType::OutGridType, MeshOpType>(gridPtr->cbeginValueOn(), *visitedVertexIndices, *meshOp);
				meshOp->collectPolygons();
			}
			const typename GridType::Ptr gridPtr;
			const typename MeshOpType::Ptr meshOp;
		private:
			typename ActivateValuesOpType::Ptr activateValuesOp;
			typename VertexIndexGridType::Ptr visitedVertexIndices;
		};
	}
}