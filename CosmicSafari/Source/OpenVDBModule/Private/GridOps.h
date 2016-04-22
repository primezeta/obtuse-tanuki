#pragma once
#include "EngineMinimal.h"
#pragma warning(push)
#pragma warning(1:4211 4800 4503 4146)
#include <openvdb/openvdb.h>
#include <openvdb/tools/ValueTransformer.h>
#include <noise.h>
#include <noiseutils.h>
#include <boost/utility.hpp>
#include <tbb/mutex.h>

namespace Vdb
{
	namespace GridOps
	{
		typedef int32 IndexType;
		typedef openvdb::tree::Tree4<IndexType, 5, 4, 3>::Type IndexTreeType; //Same tree configuration (5,4,3) as openvdb::FloatTree (see openvdb.h)
		const static IndexType UNVISITED_VERTEX_INDEX = -1;
		const static int32 CUBE_VERTEX_COUNT = 8;

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

			IndexType& operator[](int32 v) { return primitiveIndices[v]; }
			openvdb::Coord& getCoord(int32 v) { return primitiveVertices[v]; }
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
		template<typename InIterType,
			     typename OutTreeType,
			     typename SelfOpType,
			     typename AccessorType>
		class ITransformOp
		{
		public:
			typedef typename InIterType IterType;
			typedef typename InIterType::ValueT InValueType;
			typedef typename OutTreeType::ValueType OutValueType;
			typedef typename openvdb::Grid<OutTreeType> OutGridType;
			typedef typename AccessorType OutAccessorType;
			ITransformOp(openvdb::math::Transform::Ptr xformPtr) : GridXformPtr(xformPtr)
			{
				check(GridXformPtr != nullptr);
			}

			static void transformValues(const IterType &beginIter, OutGridType &outGrid, SelfOpType &op)
			{
				openvdb::tools::transformValues<IterType, OutGridType, SelfOpType, OutAccessorType>(beginIter, outGrid, op);
			}

			inline void operator()(const IterType& iter, OutAccessorType& acc)
			{
				if (iter.isVoxelValue())
				{
					const openvdb::Coord coord = iter.getCoord();
					DoTransform(iter, acc, coord);
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
								DoTransform(iter, acc, coord);
							}
						}
					}
				}
			}

		protected:
			const openvdb::math::Transform::Ptr GridXformPtr;

			virtual inline void DoTransform(const IterType& iter, OutAccessorType& acc, const openvdb::Coord &coord) = 0; //Here, call ModifyValue[AndActiveState][Shared]
			virtual inline void GetIsActive(const IterType& iter, OutAccessorType& acc, const openvdb::Coord &coord, bool &outIsActive) = 0; //Supply your own active/inactive logic
			virtual inline void GetValue(const IterType& iter, OutAccessorType& acc, const openvdb::Coord &coord, OutValueType &outValue) = 0; //Supply your own value generator

			inline void ModifyValueAndActiveState(const IterType& iter, OutAccessorType& acc, const openvdb::Coord &coord, OutValueType &outValue, bool &isActive)
			{
				GetValue(iter, acc, coord, outValue);
				GetIsActive(iter, acc, coord, isActive);
				acc.modifyValueAndActiveState<BasicModifyActiveOp<OutValueType>>(coord, BasicModifyActiveOp<OutValueType>(outValue, isActive));
			}

			inline void ModifyValue(const IterType& iter, OutAccessorType& acc, const openvdb::Coord &coord, OutValueType &outValue)
			{
				GetValue(iter, acc, coord, outValue);
				acc.modifyValue<BasicModifyOp<OutValueType>>(coord, BasicModifyOp<OutValueType>(outValue));
			}

			inline void ModifyActiveState(const IterType& iter, OutAccessorType& acc, const openvdb::Coord &coord, bool &isActive)
			{
				GetIsActive(iter, acc, coord, isActive);
				iter.setActiveState(isActive);
			}
		};

		//Operator to fill a grid with Perlin noise values
		template <typename OutTreeType, typename InTreeType = openvdb::BoolTree>
		class PerlinNoiseFillOp :
			public ITransformOp<typename InTreeType::ValueOnCIter,
			                    OutTreeType,
			                    typename PerlinNoiseFillOp<OutTreeType, InTreeType>,
			                    openvdb::tree::ValueAccessor<OutTreeType>>
		{
		public:
			PerlinNoiseFillOp(const openvdb::math::Transform::Ptr xformPtr, int32 seed, float frequency, float lacunarity, float persistence, int32 octaveCount)
				: ITransformOp(xformPtr)
			{
				valueSource.SetSeed(seed);
				valueSource.SetFrequency((double)frequency);
				valueSource.SetLacunarity((double)lacunarity);
				valueSource.SetPersistence((double)persistence);
				valueSource.SetOctaveCount(octaveCount);
			}
			
			virtual inline void DoTransform(const IterType& iter, OutAccessorType& acc, const openvdb::Coord &coord) override
			{
				OutValueType value = acc.tree().background();
				bool isActive = false;
				ModifyValueAndActiveState(iter, acc, coord, value, isActive);
			}

			virtual inline void GetIsActive(const IterType& iter, OutAccessorType& acc, const openvdb::Coord &coord, bool &outIsActive) override
			{
				outIsActive = iter.getValue();
			}

			virtual inline void GetValue(const IterType& iter, OutAccessorType& acc, const openvdb::Coord &coord, OutValueType &outValue) override
			{
				//double prevLacunarity = valueSource.GetLacunarity();
				//int32 prevOctaveCount = valueSource.GetOctaveCount();
				//valueSource.SetLacunarity(prevLacunarity*0.004);
				//valueSource.SetOctaveCount(2);
				//double warp = valueSource.GetValue(vec.x(), vec.y(), vec.z()) * 8;
				//valueSource.SetLacunarity(prevLacunarity);
				//valueSource.SetOctaveCount(prevOctaveCount);
				//return (OutValueType)(warp + valueSource.GetValue(vec.x(), vec.y(), vec.z()) - vec.z());
				const openvdb::Vec3d vec = GridXformPtr->indexToWorld(coord);
				outValue = (OutValueType)(valueSource.GetValue(vec.x(), vec.y(), vec.z()) - vec.z());
			}
		private:
			noise::module::Perlin valueSource;
		};

		//Operator to extract an isosurface from a grid
		template <typename OutTreeType, typename IterType>
		class ExtractSurfaceOp
		{
		public:
			typedef typename TSharedPtr<ExtractSurfaceOp<OutTreeType, IterType>> Ptr;
			typedef typename OutTreeType::ValueType InValueType;
			ExtractSurfaceOp() {}
			
			inline void SetSurfaceValue(const InValueType &isovalue)
			{
				surfaceValue = isovalue;
			}
			
			inline InValueType GetSurfaceValue()
			{
				return surfaceValue;
			}

			inline void operator()(const IterType& iter) const
			{
				openvdb::Coord coord = iter.getCoord();
				auto treePtr = iter.getTree();
				//For each neighboring value set a bit if it is inside the surface (inside = positive value)
				uint8 insideBits = 0;
				if (iter.getValue() > surfaceValue) { insideBits |= 1; }
				if (treePtr->getValue(coord.offsetBy(1, 0, 0)) > surfaceValue) { insideBits |= 2; }
				if (treePtr->getValue(coord.offsetBy(0, 1, 0)) > surfaceValue) { insideBits |= 4; }
				if (treePtr->getValue(coord.offsetBy(0, 0, 1)) > surfaceValue) { insideBits |= 8; }
				if (treePtr->getValue(coord.offsetBy(1, 1, 0)) > surfaceValue) { insideBits |= 16; }
				if (treePtr->getValue(coord.offsetBy(1, 0, 1)) > surfaceValue) { insideBits |= 32; }
				if (treePtr->getValue(coord.offsetBy(0, 1, 1)) > surfaceValue) { insideBits |= 64; }
				if (treePtr->getValue(coord.offsetBy(1, 1, 1)) > surfaceValue) { insideBits |= 128; }
				//If all vertices are inside the surface or all are outside the surface then set off in order to not mesh this voxel
				iter.setActiveState(insideBits > 0 && insideBits < 255);
			}
		private:
			InValueType surfaceValue;
		};

		template <typename OutTreeType, typename InTreeType>
		class MeshGeometryOp :
			public ITransformOp<typename InTreeType::ValueOnCIter,
			                    OutTreeType,
			                    typename MeshGeometryOp<OutTreeType, InTreeType>,
			                    openvdb::tree::ValueAccessor<OutTreeType>>
		{
		public:
			typedef typename TSharedPtr<MeshGeometryOp<typename OutTreeType, typename InTreeType>> Ptr;
			MeshGeometryOp(const openvdb::math::Transform::Ptr xformPtr, TArray<FVector> &vertexBuffer, TArray<int32> &polygonBuffer, TArray<FVector> &normalBuffer)
				: ITransformOp(xformPtr), vertices(vertexBuffer), polygons(polygonBuffer), normals(normalBuffer), VertexCriticalSection(new FCriticalSection()) {}

			virtual inline void DoTransform(const IterType& iter, OutAccessorType& acc, const openvdb::Coord &coord) override
			{
				//Mesh the voxel as a simple cube with 6 equal sized quads
				PrimitiveCube primitiveIndices(iter.getCoord());
				for (uint32 i = 0; i < CUBE_VERTEX_COUNT; ++i)
				{
					const openvdb::Coord idxCoord = primitiveIndices.getCoord(i);
					OutValueType vertexIndex;
					{
						FScopeLock lock(VertexCriticalSection.Get());
						ModifyValue(iter, acc, idxCoord, vertexIndex);
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

			virtual inline void GetIsActive(const IterType& iter, OutAccessorType& acc, const openvdb::Coord &coord, bool &outIsActive) override
			{
				outIsActive = iter.isValueOn();
			}

			virtual inline void GetValue(const IterType& iter, OutAccessorType& acc, const openvdb::Coord &coord, OutValueType &outValue) override
			{
				outValue = acc.getValue(coord);
				if (outValue == acc.tree().background())
				{
					const openvdb::Vec3d vtx = GridXformPtr->indexToWorld(coord);
					{
						outValue = (OutValueType)vertices.Add(FVector((float)vtx.x(), (float)vtx.y(), (float)vtx.z()));
					}
				}
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
			const TSharedPtr<FCriticalSection> VertexCriticalSection;
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
			typedef typename TSharedPtr<BasicMesher<TreeType, VertexIndexTreeType>> Ptr;
			typedef typename openvdb::Grid<TreeType> GridType;
			BasicMesher(typename GridType::Ptr grid, TArray<FVector> &vertexBuffer, TArray<int32> &polygonBuffer, TArray<FVector> &normalBuffer)
				: gridPtr(grid),
				  meshOp(new MeshGeometryOp<VertexIndexTreeType, TreeType>(gridPtr->transformPtr(), vertexBuffer, polygonBuffer, normalBuffer)),
				  activateValuesOp(new ExtractSurfaceOp<TreeType, typename TreeType::ValueOnIter>()),
				  isDirty(true) {}

			inline void doActivateValuesOp(typename ExtractSurfaceOp<TreeType, typename TreeType::ValueOnIter>::InValueType isovalue)
			{
				if (!openvdb::math::isApproxEqual(isovalue, activateValuesOp->GetSurfaceValue()))
				{
					isDirty = true;
					activateValuesOp->SetSurfaceValue(isovalue);
				}
				openvdb::tools::foreach(gridPtr->beginValueOn(), *activateValuesOp);
			}

			inline void doMeshOp()
			{
				if (isDirty || visitedVertexIndices == nullptr)
				{
					visitedVertexIndices = openvdb::Grid<VertexIndexTreeType>::create(UNVISITED_VERTEX_INDEX);
					visitedVertexIndices->setTransform(gridPtr->transformPtr()->copy());
					visitedVertexIndices->topologyUnion(*gridPtr);
					meshOp->initialize();
				}
				MeshGeometryOp<VertexIndexTreeType, TreeType>::transformValues(gridPtr->cbeginValueOn(), *visitedVertexIndices, *meshOp);
				meshOp->collectPolygons();
			}

			const typename GridType::Ptr gridPtr;
			const typename MeshGeometryOp<VertexIndexTreeType, TreeType>::Ptr meshOp;
			bool isDirty;
		private:
			typename ExtractSurfaceOp<TreeType, typename TreeType::ValueOnIter>::Ptr activateValuesOp;
			typename openvdb::Grid<VertexIndexTreeType>::Ptr visitedVertexIndices;
		};
	}
}