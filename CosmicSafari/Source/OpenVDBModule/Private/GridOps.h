#pragma once
#include "EngineMinimal.h"
#pragma warning(push)
#pragma warning(1:4211 4800 4503 4146)
#include <openvdb/openvdb.h>
#include <openvdb/tools/ValueTransformer.h>
#include <openvdb/tools/GridOperators.h>
#include <noise.h>
#include <noiseutils.h>
#include "FastNoise.h"
#include "MarchingCubes.h"

namespace Vdb
{
	namespace GridOps
	{
		typedef int32 IndexType;
		typedef uint8 BitType;
		typedef openvdb::tree::Tree4<IndexType, 5, 4, 3>::Type IndexTreeType; //Same tree configuration (5,4,3) as openvdb::FloatTree (see openvdb.h)
		typedef openvdb::tree::Tree4<BitType, 5, 4, 3>::Type BitTreeType; //Same tree configuration (5,4,3) as openvdb::FloatTree (see openvdb.h)
		const static IndexType UNVISITED_VERTEX_INDEX = -1;

		template<openvdb::math::DScheme DiffScheme, typename Accessor>
		struct D1_FVoxelData
		{
			typedef typename Accessor AccessorType;
			typedef typename AccessorType::ValueType ValueType;
			typedef typename ValueType::DataType DataType;

			// the difference opperator
			static DataType difference(const ValueType& xp1, const ValueType& xm1) {
				return (xp1.Data - xm1.Data)*ValueType::DataType(0.5);
			}

			// random access
			static DataType inX(const AccessorType& grid, const openvdb::Coord& ijk)
			{
				return difference(
					grid.getValue(ijk.offsetBy(1, 0, 0)),
					grid.getValue(ijk.offsetBy(-1, 0, 0)));
			}

			static DataType inY(const AccessorType& grid, const openvdb::Coord& ijk)
			{
				return difference(
					grid.getValue(ijk.offsetBy(0, 1, 0)),
					grid.getValue(ijk.offsetBy(0, -1, 0)));
			}

			static DataType inZ(const AccessorType& grid, const openvdb::Coord& ijk)
			{
				return difference(
					grid.getValue(ijk.offsetBy(0, 0, 1)),
					grid.getValue(ijk.offsetBy(0, 0, -1)));
			}
		};

		template<openvdb::math::DScheme DiffScheme, typename Accessor>
		struct ISGradient_FVoxelData
		{
			typedef typename Accessor AccessorType;
			typedef typename AccessorType::ValueType::DataType ValueType;
			typedef openvdb::math::Vec3<ValueType> Vec3Type;

			// random access version
			static Vec3Type
				result(const AccessorType& grid, const openvdb::Coord& ijk)
				{
					return Vec3Type(D1_FVoxelData<DiffScheme, Accessor>::inX(grid, ijk),
						            D1_FVoxelData<DiffScheme, Accessor>::inY(grid, ijk),
						            D1_FVoxelData<DiffScheme, Accessor>::inZ(grid, ijk));
				}
		};

		//Operator to fill a grid with Perlin noise values (no tiles)
		template <typename InTreeType, typename InIterType, typename OutTreeType>
		class PerlinNoiseFillOp
		{
		public:
			typedef typename openvdb::Grid<InTreeType> SourceGridType;
			typedef typename SourceGridType::Ptr SourceGridTypePtr;
			typedef typename openvdb::Grid<OutTreeType> DestGridType;
			typedef typename DestGridType::Ptr DestGridTypePtr;
			typedef typename DestGridType::Accessor DestAccessorType;
			typedef typename OutTreeType::ValueType DestValueType;

			PerlinNoiseFillOp(const SourceGridTypePtr sourceGridPtr, const DestGridTypePtr destGridPtr, int32 seed, float frequency, float lacunarity, float persistence, int32 octaveCount)
				: DestGridPtr(destGridPtr)
			{
				sourceGridPtr->tree().voxelizeActiveTiles();
				//valueSource.SetNoiseType(FastNoise::NoiseType::GradientFractal);
				//valueSource.SetSeed(seed);
				//valueSource.SetFrequency(frequency);
				//valueSource.SetFractalLacunarity(lacunarity);
				//valueSource.SetFractalGain(persistence);
				//valueSource.SetFractalOctaves(octaveCount);
				valueSource.SetSeed(seed);
				valueSource.SetFrequency(frequency);
				valueSource.SetLacunarity(lacunarity);
				valueSource.SetPersistence(persistence);
				valueSource.SetOctaveCount(octaveCount);
			}

			FORCEINLINE void operator()(const InIterType& iter, DestAccessorType& acc)
			{
				const openvdb::Coord &coord = iter.getCoord();
				const bool &isActiveValue = iter.getValue();
				//Set active state of the mask iter, so that after the op is run, topologyUnion will set the destination grid active states to match
				if (!isActiveValue)
				{
					iter.setValueOff();
				}
				//Set the value for the destination
				DestValueType outValue;
				GetValue(coord, outValue);
				acc.setValueOnly(coord, outValue);
			}

			FORCEINLINE void GetValue(const openvdb::Coord &coord, DestValueType &outValue)
			{
				const openvdb::Vec3d vec = DestGridPtr->transform().indexToWorld(coord);
				outValue.Data = (DestValueType::DataType)(valueSource.GetValue(vec.x(), vec.y(), vec.z()) + vec.z());
				outValue.VoxelType = EVoxelType::VOXEL_NONE;
			}

		private:
			const DestGridTypePtr DestGridPtr;
			noise::module::Perlin valueSource;
			//FastNoise valueSource;
		};

		//Operator to extract an isosurface from a grid
		template <typename TreeType, typename IterType>
		class BasicExtractSurfaceOp
		{
		public:
			typedef typename openvdb::Grid<TreeType> GridType;
			typedef typename GridType::Ptr GridTypePtr;
			typedef typename GridType::Accessor AccessorType;
			typedef typename TreeType::ValueType ValueType;

			BasicExtractSurfaceOp(const GridTypePtr gridPtr)
				: GridPtr(gridPtr), SurfaceValue(gridPtr->tree().background())
			{
			}

			FORCEINLINE void operator()(const IterType& iter)
			{
				//Note that no special consideration is done for tile voxels, so the grid tiles must be voxelized prior to this op [tree().voxelizeActiveTiles()]
				auto acc = GridPtr->getAccessor();
				const openvdb::Coord &coord = iter.getCoord();
				const openvdb::Coord coords[8] = {
					iter.getCoord(),
					coord.offsetBy(1, 0, 0),
					coord.offsetBy(0, 1, 0),
					coord.offsetBy(0, 0, 1),
					coord.offsetBy(1, 1, 0),
					coord.offsetBy(1, 0, 1),
					coord.offsetBy(0, 1, 1),
					coord.offsetBy(1, 1, 1),
				};
				const ValueType values[8] = {
					iter.getValue(),
					acc.getValue(coords[1]),
					acc.getValue(coords[2]),
					acc.getValue(coords[3]),
					acc.getValue(coords[4]),
					acc.getValue(coords[5]),
					acc.getValue(coords[6]),
					acc.getValue(coords[7]),
				};

				//Flag a vertex as inside the surface if the data value is less than the surface data value
				uint8 insideBits = 0;
				if (values[0].Data < SurfaceValue.Data) { insideBits |= 1; }
				if (values[1].Data < SurfaceValue.Data) { insideBits |= 2; }
				if (values[2].Data < SurfaceValue.Data) { insideBits |= 4; }
				if (values[3].Data < SurfaceValue.Data) { insideBits |= 8; }
				if (values[4].Data < SurfaceValue.Data) { insideBits |= 16; }
				if (values[5].Data < SurfaceValue.Data) { insideBits |= 32; }
				if (values[6].Data < SurfaceValue.Data) { insideBits |= 64; }
				if (values[7].Data < SurfaceValue.Data) { insideBits |= 128; }
				if (insideBits == 0 || insideBits == 255)
				{
					//Completely inside or completely outside surface - turn voxel off
					iter.setValueOff();
				}
			}

		private:
			GridTypePtr GridPtr;
			const ValueType &SurfaceValue;
		};

		//Operator to process initial voxel types of a grid after surface extraction
		template <typename TreeType, typename IterType>
		class BasicSetVoxelTypeOp
		{
		public:
			typedef typename openvdb::Grid<TreeType> GridType;
			typedef typename GridType::Ptr GridTypePtr;
			typedef typename GridType::Accessor AccessorType;
			typedef typename TreeType::ValueType ValueType;

			BasicSetVoxelTypeOp(const GridTypePtr gridPtr)
				: GridPtr(gridPtr)
			{
				for (int32 i = 0; i < FVoxelData::NumMaterials(); ++i)
				{
					IsMaterialActive.Add(false);
				}
			}

			FORCEINLINE void operator()(const IterType& iter)
			{
				//Note that no special consideration is done for tile voxels, so the grid tiles must be voxelized prior to this op [tree().voxelizeActiveTiles()]
				auto acc = GridPtr->getAccessor();
				const openvdb::Coord &coord = iter.getCoord();
				const openvdb::Coord coords[8] = {
					coord,
					coord.offsetBy(1, 0, 0),
					coord.offsetBy(0, 1, 0),
					coord.offsetBy(0, 0, 1),
					coord.offsetBy(1, 1, 0),
					coord.offsetBy(1, 0, 1),
					coord.offsetBy(0, 1, 1),
					coord.offsetBy(1, 1, 1),
				};
				const bool isActive[8] = {
					iter.isValueOn(),
					acc.isValueOn(coords[1]),
					acc.isValueOn(coords[2]),
					acc.isValueOn(coords[3]),
					acc.isValueOn(coords[4]),
					acc.isValueOn(coords[5]),
					acc.isValueOn(coords[6]),
					acc.isValueOn(coords[7]),
				};
				ValueType values[8] = {
					iter.getValue(),
					acc.getValue(coords[1]),
					acc.getValue(coords[2]),
					acc.getValue(coords[3]),
					acc.getValue(coords[4]),
					acc.getValue(coords[5]),
					acc.getValue(coords[6]),
					acc.getValue(coords[7]),
				};

				if (isActive[3])
				{
					//The voxel immediately above is on which means we are on the side of a cliff or incline so set to type DIRT
					values[0].VoxelType = EVoxelType::VOXEL_DIRT;
				}
				else
				{
					//This voxel is right on the surface so set to type GRASS
					values[0].VoxelType = EVoxelType::VOXEL_GRASS;
				}
				IsMaterialActive[(int32)values[0].VoxelType] = true;
			}

			void GetActiveMaterials(TArray<TEnumAsByte<EVoxelType>> &activeMaterials)
			{
				for (int32 i = 0; i < FVoxelData::NumMaterials(); ++i)
				{
					activeMaterials.Add((EVoxelType)i);
				}
			}

		private:
			GridTypePtr GridPtr;
			TArray<bool> IsMaterialActive;
		};

		//Operator to extract an isosurface from a grid and set an output grid to contain the inside bits mask
		template <typename InTreeType, typename InIterType, typename OutTreeType>
		class ExtractSurfaceOp
		{
		public:
			typedef typename openvdb::Grid<InTreeType> SourceGridType;
			typedef typename SourceGridType::Ptr SourceGridTypePtr;
			typedef typename SourceGridType::Accessor SourceAccessorType;
			typedef typename SourceGridType::ValueType SourceValueType;
			typedef typename InIterType SourceIterType;
			typedef typename openvdb::Grid<OutTreeType> DestGridType;
			typedef typename DestGridType::Ptr DestGridTypePtr;
			typedef typename DestGridType::Accessor DestAccessorType;
			typedef typename OutTreeType::ValueType DestValueType;

			ExtractSurfaceOp(const SourceGridTypePtr sourceGridPtr)
				: SourceGridPtr(sourceGridPtr), SurfaceValue(sourceGridPtr->tree().background())
			{
			}

			FORCEINLINE void operator()(const SourceIterType& iter, DestAccessorType& acc)
			{
				//Note that no special consideration is done for tile voxels, so the grid tiles must be voxelized prior to this op [tree().voxelizeActiveTiles()]
				uint8 insideBits = 0;
				const openvdb::Coord &coord = iter.getCoord();
				auto srcAcc = SourceGridPtr->getAccessor();
				if (srcAcc.getValue(coord).Data < SurfaceValue.Data) { insideBits |= 1; }
				if (srcAcc.getValue(coord.offsetBy(1, 0, 0)).Data < SurfaceValue.Data) { insideBits |= 2; }
				if (srcAcc.getValue(coord.offsetBy(0, 1, 0)).Data < SurfaceValue.Data) { insideBits |= 4; }
				if (srcAcc.getValue(coord.offsetBy(0, 0, 1)).Data < SurfaceValue.Data) { insideBits |= 8; }
				if (srcAcc.getValue(coord.offsetBy(1, 1, 0)).Data < SurfaceValue.Data) { insideBits |= 16; }
				if (srcAcc.getValue(coord.offsetBy(1, 0, 1)).Data < SurfaceValue.Data) { insideBits |= 32; }
				if (srcAcc.getValue(coord.offsetBy(0, 1, 1)).Data < SurfaceValue.Data) { insideBits |= 64; }
				if (srcAcc.getValue(coord.offsetBy(1, 1, 1)).Data < SurfaceValue.Data) { insideBits |= 128; }
				if (insideBits == 0 || insideBits == 255)
				{
					//Voxel is completely outside or completely inside the surface, turn it off
					acc.setValueOff(coord, insideBits);
				}
				else
				{
					//Voxel is on the surface, turn it on
					acc.setValueOn(coord, insideBits);
				}
			}

		private:
			SourceGridTypePtr SourceGridPtr;
			const SourceValueType &SurfaceValue;
		};

		//Operator to mesh the previously extracted isosurface via the Marching Cubes algorithm
		template <typename TreeType, typename IterType, typename DataTreeType>
		class MarchingCubesMeshOp
		{
		public:
			typedef typename openvdb::Grid<TreeType> GridType;
			typedef typename GridType::Ptr GridTypePtr;
			typedef typename GridType::Accessor AccessorType;
			typedef typename GridType::ValueType ValueType;
			typedef typename IterType SourceIterType;
			typedef typename openvdb::Grid<DataTreeType> DataGridType;
			typedef typename DataGridType::Ptr DataGridTypePtr;
			typedef typename DataGridType::Accessor DataAccessorType;
			typedef typename DataGridType::ValueType::DataType DataType;

			MarchingCubesMeshOp(const GridTypePtr gridPtr, const DataGridTypePtr dataGridPtr, TArray<FVector> &vertexBuffer, TArray<int32> &polygonBuffer, TArray<FVector> &normalBuffer, TArray<FVector2D> &uvBuffer, TArray<FColor> &colorBuffer, TArray<FProcMeshTangent> &tangentBuffer)
				: GridPtr(gridPtr), DataGridPtr(dataGridPtr), Acc(gridPtr->getAccessor()), Xform(dataGridPtr->transform()), SurfaceValue(dataGridPtr->tree().background().Data), DataAcc(dataGridPtr->getAccessor()), VisitedVertexIndicesPtr(openvdb::Grid<IndexTreeType>::create(UNVISITED_VERTEX_INDEX)), vertices(vertexBuffer), polygons(polygonBuffer), normals(normalBuffer), uvs(uvBuffer), colors(colorBuffer), tangents(tangentBuffer)
			{
				GradientGridPtr = openvdb::Vec3fGrid::create();
				GradientGridPtr->setName(DataGridPtr->getName() + ".gradient");
				GradientGridPtr->setTransform(DataGridPtr->transformPtr());
				GradientGridPtr->setVectorType(openvdb::VEC_COVARIANT);
			}

			FORCEINLINE void operator()(const IterType& iter)
			{
				const uint8 &insideBits = iter.getValue();
				if (insideBits == 0 || insideBits == 255)
				{
					//This voxel is not on the surface so do nothing
					return;
				}

				const openvdb::Coord &coord = iter.getCoord();
				const openvdb::Coord p[8] =
				{
					coord,
					coord.offsetBy(1, 0, 0),
					coord.offsetBy(0, 1, 0),
					coord.offsetBy(0, 0, 1),
					coord.offsetBy(1, 1, 0),
					coord.offsetBy(1, 0, 1),
					coord.offsetBy(0, 1, 1),
					coord.offsetBy(1, 1, 1)
				};
				const openvdb::Vec3d vec[8] =
				{
					Xform.indexToWorld(p[0]),
					Xform.indexToWorld(p[1]),
					Xform.indexToWorld(p[2]),
					Xform.indexToWorld(p[3]),
					Xform.indexToWorld(p[4]),
					Xform.indexToWorld(p[5]),
					Xform.indexToWorld(p[6]),
					Xform.indexToWorld(p[7])
				};
				const DataType val[8] =
				{
					DataAcc.getValue(p[0]).Data,
					DataAcc.getValue(p[1]).Data,
					DataAcc.getValue(p[2]).Data,
					DataAcc.getValue(p[3]).Data,
					DataAcc.getValue(p[4]).Data,
					DataAcc.getValue(p[5]).Data,
					DataAcc.getValue(p[6]).Data,
					DataAcc.getValue(p[7]).Data
				};

				//Find the vertices where the surface intersects the cube
				IndexType vertlist[12];
				if (MC_EdgeTable[insideBits] & 1)
				{
					VertexInterp(vec[0], vec[1], val[0], val[1], p[0], p[1], vertlist[0]);
				}
				if (MC_EdgeTable[insideBits] & 2)
				{
					VertexInterp(vec[1], vec[2], val[1], val[2], p[1], p[2], vertlist[1]);
				}
				if (MC_EdgeTable[insideBits] & 4)
				{
					VertexInterp(vec[2], vec[3], val[2], val[3], p[2], p[3], vertlist[2]);
				}
				if (MC_EdgeTable[insideBits] & 8)
				{
					VertexInterp(vec[3], vec[0], val[3], val[0], p[3], p[0], vertlist[3]);
				}
				if (MC_EdgeTable[insideBits] & 16)
				{
					VertexInterp(vec[4], vec[5], val[4], val[5], p[4], p[5], vertlist[4]);
				}
				if (MC_EdgeTable[insideBits] & 32)
				{
					VertexInterp(vec[5], vec[6], val[5], val[6], p[5], p[6], vertlist[5]);
				}
				if (MC_EdgeTable[insideBits] & 64)
				{
					VertexInterp(vec[6], vec[7], val[6], val[7], p[6], p[7], vertlist[6]);
				}
				if (MC_EdgeTable[insideBits] & 128)
				{
					VertexInterp(vec[7], vec[4], val[7], val[4], p[7], p[4], vertlist[7]);
				}
				if (MC_EdgeTable[insideBits] & 256)
				{
					VertexInterp(vec[0], vec[4], val[0], val[4], p[0], p[4], vertlist[8]);
				}
				if (MC_EdgeTable[insideBits] & 512)
				{
					VertexInterp(vec[1], vec[5], val[1], val[5], p[1], p[5], vertlist[9]);
				}
				if (MC_EdgeTable[insideBits] & 1024)
				{
					VertexInterp(vec[2], vec[6], val[2], val[6], p[2], p[6], vertlist[10]);
				}
				if (MC_EdgeTable[insideBits] & 2048)
				{
					VertexInterp(vec[3], vec[7], val[3], val[7], p[3], p[7], vertlist[11]);
				}

				//Calculate the gradient of this point
				openvdb::Vec3f Gradient;
				{
					FScopeLock lock(&CriticalSection);
					auto gradAcc = GradientGridPtr->getAccessor();
					if (!gradAcc.isValueOn(p[0]))
					{
						//TODO: Get the real map type (for now I know that the grid has a ScaleTranslateMap)
						openvdb::Vec3f iGradient(ISGradient_FVoxelData<openvdb::math::CD_2ND, DataAccessorType>::result(DataAcc, p[0]));
						Gradient = DataGridPtr->transform().baseMap()->applyIJT(iGradient, p[0].asVec3d());
					}
					else
					{
						Gradient = gradAcc.getValue(p[0]);
					}
				}

				// Create the triangle
				for (int32_t i = 0; MC_TriTable[insideBits][i] != -1; i += 3)
				{
					{
						FScopeLock lock(&CriticalSection);
						const IndexType &vertex0 = vertlist[MC_TriTable[insideBits][i]];
						const IndexType &vertex1 = vertlist[MC_TriTable[insideBits][i + 1]];
						const IndexType &vertex2 = vertlist[MC_TriTable[insideBits][i + 2]];
						polygons.Add(vertex0);
						polygons.Add(vertex1);
						polygons.Add(vertex2);

						//Add dummy values for now TODO
						//Nx = UyVz - UzVy
						//Ny = UzVx - UxVz
						//Nz = UxVy - UyVx
						//const FVector u = vertices[vertex1] - vertices[vertex0];
						//const FVector v = vertices[vertex2] - vertices[vertex0];
						//normals.Add(FVector(u.Y*v.Z - u.Z*v.Y, u.Z*v.X - u.X*v.Z, u.X*v.Y - u.Y*v.X));
						normals.Add(FVector(Gradient.x(), Gradient.y(), Gradient.z()));
						uvs.Add(FVector2D());
						colors.Add(FColor());
						tangents.Add(FProcMeshTangent());
					}
				}
			}

			FORCEINLINE void VertexInterp(const openvdb::Vec3d &vec1, const openvdb::Vec3d &vec2, const DataType &valp1, const DataType &valp2, const openvdb::Coord &c1, const openvdb::Coord &c2, IndexType &outVertex)
			{
				auto acc = VisitedVertexIndicesPtr->getAccessor();
				if (openvdb::math::isApproxEqual(valp1, SurfaceValue))
				{
					FScopeLock lock(&CriticalSection);
					if (acc.isValueOn(c1))
					{
						outVertex = acc.getValue(c1);
					}
					else
					{
						outVertex = vertices.Add(FVector(vec1.x(), vec1.y(), vec1.z()));
						acc.setValueOn(c1, outVertex);
					}
				}
				else if (openvdb::math::isApproxEqual(valp2, SurfaceValue))
				{
					FScopeLock lock(&CriticalSection);
					if (acc.isValueOn(c2))
					{
						outVertex = acc.getValue(c2);
					}
					else
					{
						outVertex = vertices.Add(FVector(vec2.x(), vec2.y(), vec2.z()));
						acc.setValueOn(c2, outVertex);
					}
				}
				else if (openvdb::math::isApproxEqual(valp1, valp2))
				{
					FScopeLock lock(&CriticalSection);
					if (acc.isValueOn(c1))
					{
						outVertex = acc.getValue(c1);
					}
					else
					{
						outVertex = vertices.Add(FVector(vec1.x(), vec1.y(), vec1.z()));
						acc.setValueOn(c1, outVertex);
					}
				}
				else
				{
					FScopeLock lock(&CriticalSection);
					if (acc.isValueOn(c1))
					{
						outVertex = acc.getValue(c1);
					}
					else
					{
						const double mu = ((double)(SurfaceValue - valp1)) / (double)(valp2 - valp1);
						outVertex = vertices.Add(FVector(vec1.x() + (mu * (vec2.x() - vec1.x())), vec1.y() + (mu * (vec2.y() - vec1.y())), vec1.z() + (mu * (vec2.z() - vec1.z()))));
						acc.setValueOn(c1, outVertex);
					}
				}
			}
			
			const GridTypePtr GridPtr;

		protected:
			FCriticalSection CriticalSection;
			AccessorType Acc;
			const openvdb::math::Transform Xform;
			const DataType &SurfaceValue;
			const DataGridTypePtr DataGridPtr;
			DataAccessorType DataAcc;
			openvdb::Grid<IndexTreeType>::Ptr VisitedVertexIndicesPtr;
			openvdb::Vec3fGrid::Ptr GradientGridPtr;
			TArray<FVector> &vertices;
			TArray<int32> &polygons;
			TArray<FVector> &normals;
			TArray<FVector2D> &uvs;
			TArray<FColor> &colors;
			TArray<FProcMeshTangent> &tangents;
		};

		//Operator to mesh a cube at each active voxel from a previously extracted isosurface
		template <typename TreeType, typename IterType>
		class CubesMeshOp
		{
		public:
			typedef typename openvdb::Grid<TreeType> GridType;
			typedef typename GridType::Ptr GridTypePtr;
			typedef typename GridType::Accessor AccessorType;
			typedef typename GridType::ValueType ValueType;
			typedef typename IterType SourceIterType;

			CubesMeshOp(const GridTypePtr gridPtr, TArray<FGridMeshBuffers> &meshBuffers)
				: GridPtr(gridPtr), GridAcc(gridPtr->tree()), UnvisitedVertexIndex(GridPtr->tree().background()), MeshBuffers(meshBuffers)
			{
			}

			FORCEINLINE void operator()(const SourceIterType& iter)
			{
				//Mesh the voxel as a simple cube with 6 equal sized quads
				const openvdb::BBoxd bbox = GridPtr->transform().indexToWorld(openvdb::CoordBBox::createCube(iter.getCoord(), 2));
				const openvdb::Vec3d vtxs[8] = {
					bbox.min(),
					openvdb::Vec3d(bbox.max().x(), bbox.min().y(), bbox.min().z()),
					openvdb::Vec3d(bbox.min().x(), bbox.min().y(), bbox.max().z()),
					openvdb::Vec3d(bbox.max().x(), bbox.min().y(), bbox.max().z()),
					openvdb::Vec3d(bbox.min().x(), bbox.max().y(), bbox.min().z()),
					openvdb::Vec3d(bbox.min().x(), bbox.max().y(), bbox.max().z()),
					openvdb::Vec3d(bbox.max().x(), bbox.max().y(), bbox.min().z()),
					bbox.max()
				};

				const auto value = iter.getValue();
				VertexBufferType &vertices = MeshBuffers[(int32)value.VoxelType].VertexBuffer;
				PolygonBufferType &polygons = MeshBuffers[(int32)value.VoxelType].PolygonBuffer;
				NormalBufferType &normals = MeshBuffers[(int32)value.VoxelType].NormalBuffer;
				UVMapBufferType &uvs = MeshBuffers[(int32)value.VoxelType].UVMapBuffer;
				VertexColorBufferType &colors = MeshBuffers[(int32)value.VoxelType].VertexColorBuffer;
				TangentBufferType &tangents = MeshBuffers[(int32)value.VoxelType].TangentBuffer;
				{
					FScopeLock lock(&CriticalSection);
					//Add polygons each with unique vertices (vertex indices added clockwise order on each quad face)
					polygons.Add(vertices.Add(FVector(vtxs[0].x(), vtxs[0].y(), vtxs[0].z())));//Front face
					polygons.Add(vertices.Add(FVector(vtxs[2].x(), vtxs[2].y(), vtxs[2].z())));
					polygons.Add(vertices.Add(FVector(vtxs[3].x(), vtxs[3].y(), vtxs[3].z())));
					normals.Add(FVector(0.0f, -1.0f, 0.0f));
					normals.Add(FVector(0.0f, -1.0f, 0.0f));
					normals.Add(FVector(0.0f, -1.0f, 0.0f));
					uvs.Add(FVector2D(1.0f/3.0f, 0.5f));
					uvs.Add(FVector2D(1.0f/3.0f, 0.25f));
					uvs.Add(FVector2D(2.0f/3.0f, 0.25f));

					polygons.Add(vertices.Add(FVector(vtxs[3].x(), vtxs[3].y(), vtxs[3].z())));//Front face
					polygons.Add(vertices.Add(FVector(vtxs[1].x(), vtxs[1].y(), vtxs[1].z())));
					polygons.Add(vertices.Add(FVector(vtxs[0].x(), vtxs[0].y(), vtxs[0].z())));
					normals.Add(FVector(0.0f, -1.0f, 0.0f));
					normals.Add(FVector(0.0f, -1.0f, 0.0f));
					normals.Add(FVector(0.0f, -1.0f, 0.0f));
					uvs.Add(FVector2D(2.0f/3.0f, 0.25f));
					uvs.Add(FVector2D(2.0f/3.0f, 0.5f));
					uvs.Add(FVector2D(1.0f/3.0f, 0.5f));

					polygons.Add(vertices.Add(FVector(vtxs[1].x(), vtxs[1].y(), vtxs[1].z())));//Right face
					polygons.Add(vertices.Add(FVector(vtxs[3].x(), vtxs[3].y(), vtxs[3].z())));
					polygons.Add(vertices.Add(FVector(vtxs[7].x(), vtxs[7].y(), vtxs[7].z())));
					normals.Add(FVector(1.0f, 0.0f, 0.0f));
					normals.Add(FVector(1.0f, 0.0f, 0.0f));
					normals.Add(FVector(1.0f, 0.0f, 0.0f));
					uvs.Add(FVector2D(2.0f/3.0f, 0.5f));
					uvs.Add(FVector2D(1.0f, 0.5f));
					uvs.Add(FVector2D(1.0f, 0.75f));

					polygons.Add(vertices.Add(FVector(vtxs[7].x(), vtxs[7].y(), vtxs[7].z())));//Right face
					polygons.Add(vertices.Add(FVector(vtxs[6].x(), vtxs[6].y(), vtxs[6].z())));
					polygons.Add(vertices.Add(FVector(vtxs[1].x(), vtxs[1].y(), vtxs[1].z())));
					normals.Add(FVector(1.0f, 0.0f, 0.0f));
					normals.Add(FVector(1.0f, 0.0f, 0.0f));
					normals.Add(FVector(1.0f, 0.0f, 0.0f));
					uvs.Add(FVector2D(1.0f, 0.75f));
					uvs.Add(FVector2D(2.0f/3.0f, 0.75f));
					uvs.Add(FVector2D(2.0f/3.0f, 0.5f));

					polygons.Add(vertices.Add(FVector(vtxs[4].x(), vtxs[4].y(), vtxs[4].z())));//Back face
					polygons.Add(vertices.Add(FVector(vtxs[6].x(), vtxs[6].y(), vtxs[6].z())));
					polygons.Add(vertices.Add(FVector(vtxs[7].x(), vtxs[7].y(), vtxs[7].z())));
					normals.Add(FVector(0.0f, 1.0f, 0.0f));
					normals.Add(FVector(0.0f, 1.0f, 0.0f));
					normals.Add(FVector(0.0f, 1.0f, 0.0f));
					uvs.Add(FVector2D(1.0f/3.0f, 0.75f));
					uvs.Add(FVector2D(2.0f/3.0f, 0.75f));
					uvs.Add(FVector2D(2.0f/3.0f, 1.0f));

					polygons.Add(vertices.Add(FVector(vtxs[7].x(), vtxs[7].y(), vtxs[7].z())));//Back face
					polygons.Add(vertices.Add(FVector(vtxs[5].x(), vtxs[5].y(), vtxs[5].z())));
					polygons.Add(vertices.Add(FVector(vtxs[4].x(), vtxs[4].y(), vtxs[4].z())));
					normals.Add(FVector(0.0f, 1.0f, 0.0f));
					normals.Add(FVector(0.0f, 1.0f, 0.0f));
					normals.Add(FVector(0.0f, 1.0f, 0.0f));
					uvs.Add(FVector2D(2.0f/3.0f, 1.0f));
					uvs.Add(FVector2D(1.0f/3.0f, 1.0f));
					uvs.Add(FVector2D(1.0f/3.0f, 0.75f));

					polygons.Add(vertices.Add(FVector(vtxs[0].x(), vtxs[0].y(), vtxs[0].z())));//Left face
					polygons.Add(vertices.Add(FVector(vtxs[4].x(), vtxs[4].y(), vtxs[4].z())));
					polygons.Add(vertices.Add(FVector(vtxs[5].x(), vtxs[5].y(), vtxs[5].z())));
					normals.Add(FVector(-1.0f, 0.0f, 0.0f));
					normals.Add(FVector(-1.0f, 0.0f, 0.0f));
					normals.Add(FVector(-1.0f, 0.0f, 0.0f));
					uvs.Add(FVector2D(1.0f/3.0f, 0.5f));
					uvs.Add(FVector2D(1.0f/3.0f, 0.75f));
					uvs.Add(FVector2D(0.0f, 0.75f));

					polygons.Add(vertices.Add(FVector(vtxs[5].x(), vtxs[5].y(), vtxs[5].z())));//Left face
					polygons.Add(vertices.Add(FVector(vtxs[2].x(), vtxs[2].y(), vtxs[2].z())));
					polygons.Add(vertices.Add(FVector(vtxs[0].x(), vtxs[0].y(), vtxs[0].z())));
					normals.Add(FVector(-1.0f, 0.0f, 0.0f));
					normals.Add(FVector(-1.0f, 0.0f, 0.0f));
					normals.Add(FVector(-1.0f, 0.0f, 0.0f));
					uvs.Add(FVector2D(0.0f, 0.75f));
					uvs.Add(FVector2D(0.0f, 0.5f));
					uvs.Add(FVector2D(1.0f/3.0f, 0.5f));

					polygons.Add(vertices.Add(FVector(vtxs[2].x(), vtxs[2].y(), vtxs[2].z())));//Top face
					polygons.Add(vertices.Add(FVector(vtxs[5].x(), vtxs[5].y(), vtxs[5].z())));
					polygons.Add(vertices.Add(FVector(vtxs[7].x(), vtxs[7].y(), vtxs[7].z())));
					normals.Add(FVector(0.0f, 0.0f, 1.0f));
					normals.Add(FVector(0.0f, 0.0f, 1.0f));
					normals.Add(FVector(0.0f, 0.0f, 1.0f));
					uvs.Add(FVector2D(1.0f/3.0f, 0.25f));
					uvs.Add(FVector2D(1.0f/3.0f, 0.0f));
					uvs.Add(FVector2D(2.0f/3.0f, 0.0f));

					polygons.Add(vertices.Add(FVector(vtxs[7].x(), vtxs[7].y(), vtxs[7].z())));//Top face
					polygons.Add(vertices.Add(FVector(vtxs[3].x(), vtxs[3].y(), vtxs[3].z())));
					polygons.Add(vertices.Add(FVector(vtxs[2].x(), vtxs[2].y(), vtxs[2].z())));
					normals.Add(FVector(0.0f, 0.0f, 1.0f));
					normals.Add(FVector(0.0f, 0.0f, 1.0f));
					normals.Add(FVector(0.0f, 0.0f, 1.0f));
					uvs.Add(FVector2D(2.0f/3.0f, 0.0f));
					uvs.Add(FVector2D(2.0f/3.0f, 0.25f));
					uvs.Add(FVector2D(1.0f/3.0f, 0.25f));

					polygons.Add(vertices.Add(FVector(vtxs[0].x(), vtxs[0].y(), vtxs[0].z())));//Bottom face
					polygons.Add(vertices.Add(FVector(vtxs[1].x(), vtxs[1].y(), vtxs[1].z())));
					polygons.Add(vertices.Add(FVector(vtxs[6].x(), vtxs[6].y(), vtxs[6].z())));
					normals.Add(FVector(0.0f, 0.0f, -1.0f));
					normals.Add(FVector(0.0f, 0.0f, -1.0f));
					normals.Add(FVector(0.0f, 0.0f, -1.0f));
					uvs.Add(FVector2D(1.0f/3.0f, 0.5f));
					uvs.Add(FVector2D(2.0f/3.0f, 0.5f));
					uvs.Add(FVector2D(2.0f/3.0f, 0.75f));

					polygons.Add(vertices.Add(FVector(vtxs[6].x(), vtxs[6].y(), vtxs[6].z())));//Bottom face
					polygons.Add(vertices.Add(FVector(vtxs[4].x(), vtxs[4].y(), vtxs[4].z())));
					polygons.Add(vertices.Add(FVector(vtxs[0].x(), vtxs[0].y(), vtxs[0].z())));
					normals.Add(FVector(0.0f, 0.0f, -1.0f));
					normals.Add(FVector(0.0f, 0.0f, -1.0f));
					normals.Add(FVector(0.0f, 0.0f, -1.0f));
					uvs.Add(FVector2D(2.0f/3.0f, 0.75f));
					uvs.Add(FVector2D(1.0f/3.0f, 0.75f));
					uvs.Add(FVector2D(1.0f/3.0f, 0.5f));

					//Add dummy values for now TODO
					colors.Add(FColor());
					tangents.Add(FProcMeshTangent());
				}
			}

		protected:
			FCriticalSection CriticalSection;
			const GridTypePtr GridPtr;
			AccessorType GridAcc;
			const ValueType &UnvisitedVertexIndex;
			TArray<FGridMeshBuffers> &MeshBuffers;
		};

		//Helper struct to hold the associated basic cubes meshing info
		template <typename SourceTreeType>
		class CubeMesher :
			public CubesMeshOp<IndexTreeType, typename SourceTreeType::ValueOnCIter>
		{
		public:
			typedef typename openvdb::Grid<SourceTreeType> SourceGridType;
			typedef typename SourceGridType::Ptr SourceGridTypePtr;
			typedef typename SourceGridType::ValueType SourceValueType;
			const SourceGridTypePtr SourceGridPtr;

			CubeMesher(const SourceGridTypePtr sourceGridPtr, TArray<FGridMeshBuffers> &meshBuffers)
				: isChanged(true), SourceGridPtr(sourceGridPtr), CubesMeshOp(GridType::create(UNVISITED_VERTEX_INDEX), meshBuffers)
			{
				GridPtr->setName(SourceGridPtr->getName() + ".indices");
				GridPtr->setTransform(SourceGridPtr->transformPtr());
			}

			FORCEINLINE void clearBuffers()
			{
				for (auto i = MeshBuffers.CreateIterator(); i; ++i)
				{
					i->ClearBuffers();
				}
			}

			FORCEINLINE void doMeshOp(const bool &threaded)
			{
				if (isChanged)
				{
					clearBuffers();
					GridPtr->clear();
					GridPtr->topologyUnion(*SourceGridPtr);
					openvdb::tools::valxform::SharedOpApplier<SourceIterType, CubeMesher<SourceTreeType>> proc(SourceGridPtr->cbeginValueOn(), *this);
					UE_LOG(LogOpenVDBModule, Display, TEXT("%s"), *FString::Printf(TEXT("%s (pre basic mesh op) %d active voxels"), UTF8_TO_TCHAR(GridPtr->getName().c_str()), GridPtr->activeVoxelCount()));
					proc.process(threaded);
				}
				UE_LOG(LogOpenVDBModule, Display, TEXT("%s"), *FString::Printf(TEXT("%s (post basic mesh op) %d active voxels"), UTF8_TO_TCHAR(GridPtr->getName().c_str()), GridPtr->activeVoxelCount()));
				isChanged = false;
			}

			FORCEINLINE void markChanged()
			{
				isChanged = true;
			}

			bool isChanged;
		};

		//Helper struct to hold the associated marching cubes meshing info
		template <typename SourceTreeType>
		class MarchingCubesMesher :
			public MarchingCubesMeshOp<BitTreeType, typename BitTreeType::ValueOnCIter, SourceTreeType>
		{
		public:
			MarchingCubesMesher(const DataGridTypePtr dataGridPtr, TArray<FVector> &vertexBuffer, TArray<int32> &polygonBuffer, TArray<FVector> &normalBuffer, TArray<FVector2D> &uvBuffer, TArray<FColor> &colorBuffer, TArray<FProcMeshTangent> &tangentBuffer)
				: MarchingCubesMeshOp(GridType::create(0), dataGridPtr, vertexBuffer, polygonBuffer, normalBuffer, uvBuffer, colorBuffer, tangentBuffer), isChanged(true)
			{
				GridPtr->setName(DataGridPtr->getName() + ".bits");
				GridPtr->setTransform(DataGridPtr->transformPtr());
			}

			FORCEINLINE void clearBuffers()
			{
				vertices.Empty();
				polygons.Empty();
				normals.Empty();
				uvs.Empty();
				colors.Empty();
				tangents.Empty();
			}

			FORCEINLINE void doMeshOp(const bool &threaded)
			{
				if (isChanged)
				{
					clearBuffers();
					//GridPtr->clear();
					GridPtr->topologyUnion(*DataGridPtr);
					openvdb::tools::valxform::SharedOpApplier<SourceIterType, MarchingCubesMesher<SourceTreeType>> proc(GridPtr->cbeginValueOn(), *this);
					UE_LOG(LogOpenVDBModule, Display, TEXT("%s"), *FString::Printf(TEXT("%s (pre mesh op) %d active voxels"), UTF8_TO_TCHAR(GridPtr->getName().c_str()), GridPtr->activeVoxelCount()));
					proc.process(threaded);
				}
				UE_LOG(LogOpenVDBModule, Display, TEXT("%s"), *FString::Printf(TEXT("%s (post mesh op) %d active voxels"), UTF8_TO_TCHAR(GridPtr->getName().c_str()), GridPtr->activeVoxelCount()));
				isChanged = false;
			}

			FORCEINLINE void markChanged()
			{
				isChanged = true;
			}

			bool isChanged;
		};
	}
}