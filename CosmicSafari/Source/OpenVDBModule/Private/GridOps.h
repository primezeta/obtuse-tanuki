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
		enum CubeVertex { VX0, VX1, VX2, VX3, VX4, VX5, VX6, VX7, VX8 };
		const static SIZE_T CUBE_VERTEX_COUNT = VX8 + 1;
		const static openvdb::Index32 UNVISITED_VERTEX_INDEX = UINT32_MAX;

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

			openvdb::Index32& operator[](CubeVertex v) { return primitiveIndices[v]; }
			openvdb::Coord& getCoord(CubeVertex v) { return primitiveVertices[v]; }
			//Add the vertex indices in counterclockwise order on each quad face
			openvdb::Vec4I getQuadXY0() { return openvdb::Vec4I(primitiveIndices[3], primitiveIndices[4], primitiveIndices[7], primitiveIndices[5]); }
			openvdb::Vec4I getQuadXY1() { return openvdb::Vec4I(primitiveIndices[6], primitiveIndices[2], primitiveIndices[0], primitiveIndices[1]); }
			openvdb::Vec4I getQuadXZ0() { return openvdb::Vec4I(primitiveIndices[7], primitiveIndices[4], primitiveIndices[2], primitiveIndices[6]); }
			openvdb::Vec4I getQuadXZ1() { return openvdb::Vec4I(primitiveIndices[5], primitiveIndices[1], primitiveIndices[0], primitiveIndices[3]); }
			openvdb::Vec4I getQuadYZ0() { return openvdb::Vec4I(primitiveIndices[7], primitiveIndices[6], primitiveIndices[1], primitiveIndices[5]); }
			openvdb::Vec4I getQuadYZ1() { return openvdb::Vec4I(primitiveIndices[0], primitiveIndices[2], primitiveIndices[4], primitiveIndices[3]); }
		private:
			openvdb::Coord primitiveVertices[CUBE_VERTEX_COUNT];
			openvdb::Index32 primitiveIndices[CUBE_VERTEX_COUNT];
		};

		//Helper class that holds vertex indices of the quad-face of a cube
		class OvdbQuad
		{
		public:
			OvdbQuad() {}
			OvdbQuad(openvdb::Vec4I idxs) : indices(idxs), isMerged(false) {}
			OvdbQuad(const OvdbQuad &rhs) : indices(rhs.indices), isMerged(rhs.isMerged) {}
			OvdbQuad& operator=(const OvdbQuad &rhs)
			{
				indices = rhs.indices;
				isMerged = rhs.isMerged;
				return *this;
			}
			const openvdb::Vec4I& quad() const { return indices; }
			openvdb::Vec2I quadU() const { return openvdb::Vec2I(indices[0], indices[1]); }
			openvdb::Vec2I quadV() const { return openvdb::Vec2I(indices[0], indices[3]); }
			openvdb::Vec3I quadPoly1() const { return openvdb::Vec3I(indices[0], indices[1], indices[2]); }
			openvdb::Vec3I quadPoly2() const { return openvdb::Vec3I(indices[0], indices[2], indices[3]); }
			bool quadIsMerged() const { return isMerged; }
			void setIsMerged() { isMerged = true; }
			void mergeU(OvdbQuad &rhs)
			{
				if (!rhs.quadIsMerged())
				{
					rhs.setIsMerged();
					indices = openvdb::Vec4I(indices[0], rhs.indices[1], rhs.indices[2], indices[3]);
				}
			}
			void mergeV(OvdbQuad &rhs)
			{
				if (!rhs.quadIsMerged())
				{
					rhs.setIsMerged();
					indices = openvdb::Vec4I(indices[0], indices[1], rhs.indices[2], rhs.indices[3]);
				}
			}
		private:
			openvdb::Vec4I indices;
			bool isMerged;
		};

		template<typename ValueType>
		struct BasicModifyOp
		{
			const ValueType val;
			BasicModifyOp(const ValueType &v) : val(v) {}
			inline void BasicModifyOp<ValueType>::operator()(ValueType &v) const { v = val; }
		};

		template<typename ValueType>
		struct BasicModifyActiveOp
		{
			const ValueType val;
			const bool isActive;
			BasicModifyActiveOp(const ValueType& v, const bool &active) : val(v), isActive(active) {}
			inline void operator()(ValueType& v, bool &activeState) const { v = val; activeState = isActive; }
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
				valueSource.SetOctaveCount((double)octaveCount);
			}
			inline void operator()(const IterType& iter, OutAccessorType& acc) override
			{
				const bool isInsideBoundary = iter.getValue();
				if (iter.isVoxelValue())
				{
					const openvdb::Coord &coord = iter.getCoord();
					//Set the density value from the noise source and set on if the voxel is in the active boundary
					const openvdb::Vec3d vec = inTreeXform.indexToWorld(coord);
					acc.modifyValueAndActiveState<ModifyOpType>(coord, ModifyOpType(GetDensityValue(vec), isInsideBoundary));
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
								acc.modifyValueAndActiveState<ModifyOpType>(coord, ModifyOpType(GetDensityValue(vec), isInsideBoundary));
							}
						}
					}
				}
			}
			inline OutValueType GetDensityValue(const openvdb::Vec3d &vec)
			{
				double prevLacunarity = valueSource.GetLacunarity();
				int32 prevOctaveCount = valueSource.GetOctaveCount();
				valueSource.SetLacunarity(prevLacunarity*0.004);
				valueSource.SetOctaveCount(2);
				double warp = valueSource.GetValue(vec.x(), vec.y(), vec.z()) * 8;
				valueSource.SetLacunarity(prevLacunarity);
				valueSource.SetOctaveCount(prevOctaveCount);
				return (OutValueType)(warp + valueSource.GetValue(vec.x(), vec.y(), vec.z()) - vec.z());
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
		class MeshGeometryOp : public ITransformOp<typename InTreeType::ValueOnCIter, OutTreeType>
		{
		public:
			typedef typename boost::shared_ptr<MeshGeometryOp<typename OutTreeType, typename InTreeType>> Ptr;
			MeshGeometryOp(const openvdb::math::Transform &sourceXform, TArray<FVector> &vertexBuffer, TArray<int32> &polygonBuffer, TArray<FVector> &normalBuffer)
				: ITransformOp(sourceXform), vertices(vertexBuffer), polygons(polygonBuffer), normals(normalBuffer) {}
			inline void operator()(const IterType &iter, OutAccessorType &acc) override
			{
				const openvdb::Coord coord = iter.getCoord();
				const InValueType &density = iter.getValue();
				//Mesh the voxel as a simple cube with 6 equal sized quads
				PrimitiveCube primitiveIndices(coord);
				for (uint32 i = 0; i < CUBE_VERTEX_COUNT; ++i)
				{
					const CubeVertex &vtx = (CubeVertex)i;
					const openvdb::Coord &idxCoord = primitiveIndices.getCoord(vtx);

					OutValueType vertexIndex;
					vertexMutex.Lock();
					{
						vertexIndex = acc.getValue(idxCoord);
						if (vertexIndex == acc.getTree()->background())
						{
							vertexIndex = (OutValueType)(vertices.Num()); //TODO: Error check index ranges
							const openvdb::Vec3d vtx = inTreeXform.indexToWorld(idxCoord);
							vertices.Add(FVector((float)vtx.x(), (float)vtx.y(), (float)vtx.z()));
							//Since this is a new vertex save it to the global visited vertex grid for use by any other voxels in the same region that share it
							acc.setValue(idxCoord, vertexIndex);
						}
					}
					vertexMutex.Unlock();
					primitiveIndices[vtx] = vertexIndex;
				}
				vertexMutex.Lock();
				{
					quads.Add(primitiveIndices.getQuadXY0());
					quads.Add(primitiveIndices.getQuadXY1());
					quads.Add(primitiveIndices.getQuadXZ0());
					quads.Add(primitiveIndices.getQuadXZ1());
					quads.Add(primitiveIndices.getQuadYZ0());
					quads.Add(primitiveIndices.getQuadYZ1());
				}
				vertexMutex.Unlock();
			}
			inline void initialize()
			{
				quads.Empty();
				vertices.Empty();
				polygons.Empty();
				normals.Empty();
			}
			inline void collectPolygons()
			{
				for (auto i = quads.CreateConstIterator(); i; ++i)
				{
					const openvdb::Vec4I &q = *i;
					const openvdb::Vec3I poly1 = openvdb::Vec3I(q[0], q[1], q[2]);
					const openvdb::Vec3I poly2 = openvdb::Vec3I(q[0], q[2], q[3]);
					const int32 signedPoly1[3] = { (int32)poly1[0], (int32)poly1[1], (int32)poly1[2] };
					const int32 signedPoly2[3] = { (int32)poly2[0], (int32)poly2[1], (int32)poly2[2] };
					for (int32 j = 0; j < 3; ++j)
					{
						if (signedPoly1[j] < 0)
						{
							UE_LOG(LogOpenVDBModule, Fatal, TEXT("Vertex index %d too large!"), signedPoly1[j]);
						}
						if (signedPoly2[j] < 0)
						{
							UE_LOG(LogOpenVDBModule, Fatal, TEXT("Vertex index %d too large!"), signedPoly2[j]);
						}
						polygons.Add(signedPoly1[j]);
						polygons.Add(signedPoly2[j]);
					}
					//normals.Add()
					//normals.Add()
					//normals.Add()
					//normals.Add()
				}
			}
		private:
			FCriticalSection vertexMutex;
			TArray<openvdb::Vec4I> quads;
			TArray<FVector> &vertices;
			TArray<int32> &polygons;
			TArray<FVector> &normals;
		};

		//Helper struct to hold the associated grid meshing info
		template <typename TreeType, typename IndexTreeType = openvdb::UInt32Tree>
		class BasicMesher
		{
		public:
			typedef typename boost::shared_ptr<BasicMesher<TreeType>> Ptr;
			typedef typename ExtractSurfaceOp<TreeType> ActivateValuesOpType;
			typedef typename ActivateValuesOpType::InValueType InValueType;
			typedef typename MeshGeometryOp<IndexTreeType, TreeType> MeshOpType;
			typedef typename MeshOpType::OutValueType OutValueType;
			typedef typename openvdb::Grid<TreeType> GridType;
			typedef typename openvdb::Grid<IndexTreeType> VertexIndexGridType;
			BasicMesher(TSharedPtr<GridType> grid, TArray<FVector> &vertexBuffer, TArray<int32> &polygonBuffer, TArray<FVector> &normalBuffer)
				: gridPtr(grid), meshOp(new MeshOpType(grid->transform(), vertexBuffer, polygonBuffer, normalBuffer)), activateValuesOp(new ActivateValuesOpType()) {}
			inline void doActivateValuesOp(InValueType isovalue)
			{
				activateValuesOp->SetSurfaceValue(isovalue);
				openvdb::tools::transformValues<ActivateValuesOpType::IterType, ActivateValuesOpType::OutGridType, ActivateValuesOpType>(gridPtr->beginValueOn(), *gridPtr, *activateValuesOp);
			}
			inline void doMeshOp(bool initialize)
			{
				if (initialize || visitedVertexIndices == nullptr)
				{
					visitedVertexIndices = VertexIndexGridType::create((OutValueType)UNVISITED_VERTEX_INDEX);
					visitedVertexIndices->setTransform(gridPtr->transformPtr()->copy());
					visitedVertexIndices->topologyUnion(*gridPtr);
					meshOp->initialize();
				}
				openvdb::tools::transformValues<MeshOpType::IterType, MeshOpType::OutGridType, MeshOpType>(gridPtr->cbeginValueOn(), *visitedVertexIndices, *meshOp);
				meshOp->collectPolygons();
			}
			const TSharedPtr<GridType> gridPtr;
			const typename MeshOpType::Ptr meshOp;
		private:
			typename ActivateValuesOpType::Ptr activateValuesOp;
			typename VertexIndexGridType::Ptr visitedVertexIndices;
		};
	}
}