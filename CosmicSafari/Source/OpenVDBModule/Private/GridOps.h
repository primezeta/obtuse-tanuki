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
#include "tbb/mutex.h"

//The following non-class member operators are required by openvdb
template<> OPENVDBMODULE_API inline FVoxelData openvdb::zeroVal<FVoxelData>();
OPENVDBMODULE_API std::ostream& operator<<(std::ostream& os, const FVoxelData& voxelData);
OPENVDBMODULE_API FVoxelData operator+(const FVoxelData &lhs, const float &rhs);
OPENVDBMODULE_API FVoxelData operator+(const FVoxelData &lhs, const FVoxelData &rhs);
OPENVDBMODULE_API FVoxelData operator-(const FVoxelData &lhs, const FVoxelData &rhs);
OPENVDBMODULE_API bool operator<(const FVoxelData &lhs, const FVoxelData &rhs);
OPENVDBMODULE_API bool operator>(const FVoxelData &lhs, const FVoxelData &rhs);
OPENVDBMODULE_API bool operator==(const FVoxelData &lhs, const FVoxelData &rhs);
OPENVDBMODULE_API inline FVoxelData Abs(const FVoxelData &voxelData);
OPENVDBMODULE_API FVoxelData operator-(const FVoxelData &voxelData);

namespace Vdb
{
	namespace GridOps
	{
		typedef openvdb::tree::Tree4<IndexType, 5, 4, 3>::Type IndexTreeType; //Same tree configuration (5,4,3) as openvdb::FloatTree (see openvdb.h)
		typedef openvdb::tree::Tree4<MC_TriIndex::U_T, 5, 4, 3>::Type BitTreeType; //Same tree configuration (5,4,3) as openvdb::FloatTree (see openvdb.h)
		const static IndexType UNVISITED_VERTEX_INDEX = -1;

		//Static helper function since FProcMeshVertex only has the default constructor
		static FORCEINLINE FProcMeshVertex constructFProcMeshVertex(FVector vtx, FVector normal, FProcMeshTangent tangent, FColor color, FVector2D uv)
		{
			FProcMeshVertex procMeshVertex;
			procMeshVertex.Position = vtx;
			procMeshVertex.Normal = normal;
			procMeshVertex.Tangent = tangent;
			procMeshVertex.Color = color;
			procMeshVertex.UV0 = uv;
			return procMeshVertex;
		}

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
		template <typename TreeType, typename IterType>
		class PerlinNoiseFillOp
		{
		public:
			typedef typename openvdb::Grid<TreeType> GridType;
			typedef typename GridType::Ptr GridTypePtr;
			typedef typename GridType::Accessor AccessorType;
			typedef typename GridType::ConstAccessor CAccessorType;
			typedef typename TreeType::ValueType ValueType;

			PerlinNoiseFillOp(GridType &grid, const openvdb::CoordBBox &fillBBox, int32 seed, float frequency, float lacunarity, float persistence, int32 octaveCount)
				: Grid(grid), FillBBox(fillBBox)
			{
				//Expand fill box by 1 so that there are border values. The border values are turned on by the fill but will be turned off by the op
				openvdb::CoordBBox fillBBoxWithBorder = FillBBox;
				fillBBoxWithBorder.expand(1);
				Grid.fill(fillBBoxWithBorder, FVoxelData(), true);
				Grid.tree().voxelizeActiveTiles();
				//sourceGrid.tree().voxelizeActiveTiles();
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

			OPENVDB_INLINE void operator()(const IterType& iter)
			{
				check(iter.isVoxelValue() && !iter.isTileValue());
				openvdb::CoordBBox bbox;
				const bool hasVoxelVolume = iter.getBoundingBox(bbox) && bbox.hasVolume() && bbox.volume() == 1;
				check(hasVoxelVolume);

				ValueType value;
				const openvdb::Coord coord = iter.getCoord();
				GetValue(coord, value);
				iter.setValue(value);

				if (!FillBBox.isInside(coord))
				{
					//This value is a border value, turn it off so that it is not meshed but can still provide a valid value
					iter.setValueOff();
				}
			}

			OPENVDB_INLINE void GetValue(const openvdb::Coord &coord, ValueType &outValue)
			{
				const openvdb::Vec3d vec = Grid.transform().indexToWorld(coord);
				outValue.Data = (ValueType::DataType)(valueSource.GetValue(vec.x(), vec.y(), vec.z()) + vec.z());
				outValue.VoxelType = EVoxelType::VOXEL_NONE; //Initialize voxel type
			}

		private:
			GridType &Grid;
			noise::module::Perlin valueSource;
			//FastNoise valueSource;
			const openvdb::CoordBBox &FillBBox;
		};

		//Operator to extract an isosurface from a grid
		template <typename TreeType, typename IterType>
		class BasicExtractSurfaceOp
		{
		public:
			typedef typename openvdb::Grid<TreeType> GridType;
			typedef typename GridType::Ptr GridTypePtr;
			typedef typename GridType::Accessor AccessorType;
			typedef typename GridType::ConstAccessor CAccessorType;
			typedef typename TreeType::ValueType ValueType;

			BasicExtractSurfaceOp(GridType &grid)
				: Grid(grid), GridAcc(Grid.tree()), SurfaceValue(grid.tree().background())
			{
			}

			OPENVDB_INLINE void operator()(const IterType& iter)
			{
				check(iter.isVoxelValue() && !iter.isTileValue());
				openvdb::CoordBBox bbox;
				const bool hasVoxelVolume = iter.getBoundingBox(bbox) && bbox.hasVolume() && bbox.volume() == 1;
				check(hasVoxelVolume);

				const openvdb::Coord coord = iter.getCoord();
				if (coord.z() == 0)
				{
					//If at the lowest level do nothing (leaving the voxel on) so that there is a base ground floor
					check(iter.isValueOn());
					return;
				}

				const openvdb::Coord coords[8] = {
					coord,
					coord.offsetBy(0, 1, 0),
					coord.offsetBy(1, 1, 0),
					coord.offsetBy(1, 0, 0),
					coord.offsetBy(0, 0, 1),
					coord.offsetBy(0, 1, 1),
					coord.offsetBy(1, 1, 1),
					coord.offsetBy(1, 0, 1),
				};
				const ValueType values[8] = {
					GridAcc.getValue(coords[0]),
					GridAcc.getValue(coords[1]),
					GridAcc.getValue(coords[2]),
					GridAcc.getValue(coords[3]),
					GridAcc.getValue(coords[4]),
					GridAcc.getValue(coords[5]),
					GridAcc.getValue(coords[6]),
					GridAcc.getValue(coords[7]),
				};

				//Flag a vertex as inside the surface if the data value is less than the surface data value
				MC_TriIndex::U_T insideBits = (MC_TriIndex::U_T)0;
				if (values[0].Data < SurfaceValue.Data) { insideBits |= (MC_TriIndex::U_T)1; }
				if (values[1].Data < SurfaceValue.Data) { insideBits |= (MC_TriIndex::U_T)2; }
				if (values[2].Data < SurfaceValue.Data) { insideBits |= (MC_TriIndex::U_T)4; }
				if (values[3].Data < SurfaceValue.Data) { insideBits |= (MC_TriIndex::U_T)8; }
				if (values[4].Data < SurfaceValue.Data) { insideBits |= (MC_TriIndex::U_T)16; }
				if (values[5].Data < SurfaceValue.Data) { insideBits |= (MC_TriIndex::U_T)32; }
				if (values[6].Data < SurfaceValue.Data) { insideBits |= (MC_TriIndex::U_T)64; }
				if (values[7].Data < SurfaceValue.Data) { insideBits |= (MC_TriIndex::U_T)128; }
				if (insideBits == (MC_TriIndex::U_T)0 || insideBits == (MC_TriIndex::U_T)255)
				{
					//Turn off this voxel since it is completely inside or completely outside the surface
					iter.setValueOff();
				}
			}

		private:
			GridType &Grid;
			const ValueType &SurfaceValue;
			openvdb::tree::ValueAccessor<const TreeType, true, TreeType::DEPTH-1, tbb::mutex> GridAcc;
		};

		//Operator to extract an isosurface from a grid and set an output grid to contain the inside bits mask
		template <typename InTreeType, typename InIterType, typename OutTreeType>
		class ExtractSurfaceOp
		{
		public:
			typedef typename openvdb::Grid<InTreeType> SourceGridType;
			typedef typename SourceGridType::Ptr SourceGridTypePtr;
			typedef typename SourceGridType::Accessor SourceAccessorType;
			typedef typename SourceGridType::ConstAccessor CSourceAccessorType;
			typedef typename SourceGridType::ValueType SourceValueType;
			typedef typename InIterType SourceIterType;
			typedef typename openvdb::Grid<OutTreeType> DestGridType;
			typedef typename DestGridType::Ptr DestGridTypePtr;
			typedef typename DestGridType::Accessor DestAccessorType;
			typedef typename DestGridType::ConstAccessor CDestAccessorType;
			typedef typename OutTreeType::ValueType DestValueType;

			ExtractSurfaceOp(SourceGridType &sourceGrid)
				: SourceGrid(sourceGrid), SrcAcc(SourceGrid.tree()), SurfaceValue(sourceGrid.tree().background())
			{
			}

			OPENVDB_INLINE void operator()(const SourceIterType& iter, DestAccessorType& destAcc)
			{
				check(iter.isVoxelValue() && !iter.isTileValue());
				openvdb::CoordBBox bbox;
				const bool hasVoxelVolume = iter.getBoundingBox(bbox) && bbox.hasVolume() && bbox.volume() == 1;
				check(hasVoxelVolume);

				const openvdb::Coord coord = iter.getCoord();
				const openvdb::Coord coords[8] = {
					coord,
					coord.offsetBy(0, 1, 0),
					coord.offsetBy(1, 1, 0),
					coord.offsetBy(1, 0, 0),
					coord.offsetBy(0, 0, 1),
					coord.offsetBy(0, 1, 1),
					coord.offsetBy(1, 1, 1),
					coord.offsetBy(1, 0, 1),
				};
				const SourceValueType values[8] = {
					SrcAcc.getValue(coord),
					SrcAcc.getValue(coords[1]),
					SrcAcc.getValue(coords[2]),
					SrcAcc.getValue(coords[3]),
					SrcAcc.getValue(coords[4]),
					SrcAcc.getValue(coords[5]),
					SrcAcc.getValue(coords[6]),
					SrcAcc.getValue(coords[7]),
				};

				MC_TriIndex::U_T insideBits = (MC_TriIndex::U_T)0;
				if (values[0].Data < SurfaceValue.Data) { insideBits |= (MC_TriIndex::U_T)1; }
				if (values[1].Data < SurfaceValue.Data) { insideBits |= (MC_TriIndex::U_T)2; }
				if (values[2].Data < SurfaceValue.Data) { insideBits |= (MC_TriIndex::U_T)4; }
				if (values[3].Data < SurfaceValue.Data) { insideBits |= (MC_TriIndex::U_T)8; }
				if (values[4].Data < SurfaceValue.Data) { insideBits |= (MC_TriIndex::U_T)16; }
				if (values[5].Data < SurfaceValue.Data) { insideBits |= (MC_TriIndex::U_T)32; }
				if (values[6].Data < SurfaceValue.Data) { insideBits |= (MC_TriIndex::U_T)64; }
				if (values[7].Data < SurfaceValue.Data) { insideBits |= (MC_TriIndex::U_T)128; }
				if (insideBits == (MC_TriIndex::U_T)0 || insideBits == (MC_TriIndex::U_T)255)
				{
					//Voxel is completely outside or completely inside the surface, turn it off
					destAcc.setValueOff(coord, insideBits);
					iter.setValueOff();
				}
				else
				{
					//Voxel is on the surface, turn it on
					destAcc.setValueOn(coord, insideBits);
				}
			}

		private:
			SourceGridType SourceGrid;
			const SourceValueType &SurfaceValue;
			openvdb::tree::ValueAccessor<const InTreeType, true, InTreeType::DEPTH-1, tbb::mutex> SrcAcc;
		};

		//Operator to process initial voxel types of a grid after surface extraction
		template <typename TreeType, typename IterType>
		class BasicSetVoxelTypeOp
		{
		public:
			typedef typename openvdb::Grid<TreeType> GridType;
			typedef typename GridType::Ptr GridTypePtr;
			typedef typename GridType::Accessor AccessorType;
			typedef typename GridType::ConstAccessor CAccessorType;
			typedef typename TreeType::ValueType ValueType;

			BasicSetVoxelTypeOp(GridType &grid)
				: Grid(grid), GridAcc(Grid.tree())
			{
				for (int32 i = 0; i < FVoxelData::VOXEL_TYPE_COUNT; ++i)
				{
					IsVoxelTypeActive.Add(false);
				}
			}

			OPENVDB_INLINE void operator()(const IterType& iter)
			{
				check(iter.isVoxelValue() && !iter.isTileValue());
				openvdb::CoordBBox bbox;
				const bool hasVoxelVolume = iter.getBoundingBox(bbox) && bbox.hasVolume() && bbox.volume() == 1;
				check(hasVoxelVolume);

				const openvdb::Coord coord = iter.getCoord();
				ValueType value = iter.getValue();
				check(value.VoxelType == EVoxelType::VOXEL_NONE);

				if (coord.z() == 0)
				{
					value.VoxelType = EVoxelType::VOXEL_ROCK;
				}
				else if (coord.z() < 20)
				{
					value.VoxelType = EVoxelType::VOXEL_WATER;
				}
				else
				{
					if (GridAcc.isValueOn(coord.offsetBy(0, 0, 1)))
					{
						//The voxel immediately above is on which means we are on the side of a cliff or incline so set to type DIRT
						value.VoxelType = EVoxelType::VOXEL_DIRT;
					}
					else
					{
						//This voxel is right on the surface so set to type GRASS
						value.VoxelType = EVoxelType::VOXEL_GRASS;
					}
				}
				iter.setValue(value);
				check(value.VoxelType != EVoxelType::VOXEL_NONE);
				IsVoxelTypeActive[(int32)value.VoxelType] = true;
			}

			void GetActiveVoxelTypes(TArray<TEnumAsByte<EVoxelType>> &activeVoxelTypes)
			{
				for (int32 i = 0; i < FVoxelData::VOXEL_TYPE_COUNT; ++i)
				{
					if (IsVoxelTypeActive[i])
					{
						activeVoxelTypes.Add((EVoxelType)i);
					}
				}
			}

		private:
			GridType &Grid;
			TArray<bool> IsVoxelTypeActive;
			openvdb::tree::ValueAccessor<const TreeType, true, TreeType::DEPTH-1, tbb::mutex> GridAcc;
		};


		//Operator to calculate the gradient in the Data field of the FVoxel struct
		template <typename InTreeType, typename InIterType, typename OutTreeType>
		class CalcGradientOp_FVoxelData
		{
		public:
			typedef typename openvdb::Grid<InTreeType> SourceGridType;
			typedef typename SourceGridType::Ptr SourceGridTypePtr;
			typedef typename SourceGridType::Accessor SourceAccessorType;
			typedef typename SourceGridType::ConstAccessor CSourceAccessorType;
			typedef typename SourceGridType::ValueType SourceValueType;
			typedef typename InIterType SourceIterType;
			typedef typename openvdb::Grid<OutTreeType> DestGridType;
			typedef typename DestGridType::Ptr DestGridTypePtr;
			typedef typename DestGridType::Accessor DestAccessorType;
			typedef typename DestGridType::ConstAccessor CDestAccessorType;
			typedef typename OutTreeType::ValueType DestValueType;

			CalcGradientOp_FVoxelData(SourceGridType &grid)
				: SrcAcc(grid.tree()), xform(grid.transform())
			{
			}

			OPENVDB_INLINE void operator()(const SourceIterType& iter, DestAccessorType& destAcc)
			{
				typedef ISGradient_FVoxelData<openvdb::math::CD_2ND, SourceAccessorType>::Vec3Type VecType;
				check(iter.isVoxelValue() && !iter.isTileValue());
				openvdb::CoordBBox bbox;
				const bool hasVoxelVolume = iter.getBoundingBox(bbox) && bbox.hasVolume() && bbox.volume() == 1;
				check(hasVoxelVolume);
				const openvdb::Coord &coord = iter.getCoord();
				VecType grad = xform.baseMap()->applyIJT(ISGradient_FVoxelData<openvdb::math::CD_2ND, openvdb::tree::ValueAccessor<InTreeType, true, InTreeType::DEPTH - 1, tbb::mutex>>::result(SrcAcc, coord), coord.asVec3s());
				if (!grad.normalize())
				{
					grad = VecType(0, 0, 0);
				}
				destAcc.setValue(coord, grad);
			}

		protected:
			openvdb::tree::ValueAccessor<InTreeType, true, InTreeType::DEPTH-1, tbb::mutex> SrcAcc;
			const openvdb::math::Transform &xform;
		};

		//Operator to mesh the previously extracted isosurface via the Marching Cubes algorithm
		template <typename TreeType, typename IterType, typename DataTreeType>
		class MarchingCubesMeshOp
		{
		public:
			typedef typename openvdb::Grid<TreeType> GridType;
			typedef typename GridType::Ptr GridTypePtr;
			typedef typename GridType::Accessor AccessorType;
			typedef typename GridType::ConstAccessor CAccessorType;
			typedef typename GridType::ValueType ValueType;
			typedef typename IterType SourceIterType;
			typedef typename openvdb::Grid<DataTreeType> DataGridType;
			typedef typename DataGridType::Ptr DataGridTypePtr;
			typedef typename DataGridType::Accessor DataAccessorType;
			typedef typename DataGridType::ConstAccessor CDataAccessorType;
			typedef typename DataGridType::ValueType DataValueType;
			typedef typename DataValueType::DataType DataType;

			MarchingCubesMeshOp(GridType &grid, DataGridType &dataGrid, TArray<FProcMeshSection> &sectionBuffers)
				: Grid(grid), Gradient(openvdb::Vec3f()), GradAcc(Gradient.tree()), DataGrid(dataGrid), DataAcc(DataGrid.tree()), SurfaceValue(dataGrid.tree().background().Data), SectionBuffers(sectionBuffers)
			{
				Grid.setName(DataGrid.getName() + ".bits");
				Grid.setTransform(DataGrid.transformPtr());
				Gradient.setName(DataGrid.getName() + ".grad");
				Gradient.setTransform(DataGrid.transformPtr());
			}

			OPENVDB_INLINE void operator()(const IterType& iter)
			{
				check(iter.isVoxelValue() && !iter.isTileValue());
				openvdb::CoordBBox bbox;
				const bool hasVoxelVolume = iter.getBoundingBox(bbox) && bbox.volume() == 1;
				check(hasVoxelVolume);

				const openvdb::Coord coord = iter.getCoord();
				const MC_TriIndex::U_T &insideBits = (MC_TriIndex::U_T)iter.getValue();
				check(MC_EdgeTable[insideBits] > -1);
				check(insideBits > (MC_TriIndex::U_T)0 && insideBits < (MC_TriIndex::U_T)255);

				const openvdb::math::Transform &xform = DataGrid.transform();
				const openvdb::Coord p[8] =
				{
					coord,
					coord.offsetBy(0, 1, 0),
					coord.offsetBy(1, 1, 0),
					coord.offsetBy(1, 0, 0),
					coord.offsetBy(0, 0, 1),
					coord.offsetBy(0, 1, 1),
					coord.offsetBy(1, 1, 1),
					coord.offsetBy(1, 0, 1),
				};
				const DataValueType val[8] =
				{
					DataAcc.getValue(p[0]),
					DataAcc.getValue(p[1]),
					DataAcc.getValue(p[2]),
					DataAcc.getValue(p[3]),
					DataAcc.getValue(p[4]),
					DataAcc.getValue(p[5]),
					DataAcc.getValue(p[6]),
					DataAcc.getValue(p[7]),
				};
				const DataType data[8] =
				{
					val[0].Data,
					val[1].Data,
					val[2].Data,
					val[3].Data,
					val[4].Data,
					val[5].Data,
					val[6].Data,
					val[7].Data,
				};
				const openvdb::Vec3d vec[8] =
				{
					xform.indexToWorld(p[0]),
					xform.indexToWorld(p[1]),
					xform.indexToWorld(p[2]),
					xform.indexToWorld(p[3]),
					xform.indexToWorld(p[4]),
					xform.indexToWorld(p[5]),
					xform.indexToWorld(p[6]),
					xform.indexToWorld(p[7]),
				};
				const openvdb::Vec3f grad[8] =
				{
					GradAcc.getValue(p[0]),
					GradAcc.getValue(p[1]),
					GradAcc.getValue(p[2]),
					GradAcc.getValue(p[3]),
					GradAcc.getValue(p[4]),
					GradAcc.getValue(p[5]),
					GradAcc.getValue(p[6]),
					GradAcc.getValue(p[7]),
				};

				const int32 idx = (int32)val[0].VoxelType;
				check(val[0].VoxelType != EVoxelType::VOXEL_NONE && idx > -1 && idx < (int32)FVoxelData::VOXEL_TYPE_COUNT);
				FProcMeshSection &meshSection = SectionBuffers[idx];
				TArray<FProcMeshVertex> &vertices = meshSection.ProcVertexBuffer;
				TArray<int32> &polygons = meshSection.ProcIndexBuffer;
				FBox &sectionBounds = meshSection.SectionLocalBox;
				FCriticalSection &vtxCriticalSection = CriticalSections[idx];
				FCriticalSection &triCriticalSection = CriticalSections[idx+1];

				//Find the vertices where the surface intersects the cube, always using the lower coord first
				IndexType vertlist[12];
				if (MC_EdgeTable[insideBits] & 1)
				{
					vertlist[0] = VertexInterp(vec[0], vec[1], data[0], data[1], p[0], p[1], grad[0], grad[1], SurfaceValue, vtxCriticalSection, vertices, sectionBounds);
				}
				if (MC_EdgeTable[insideBits] & 2)
				{
					vertlist[1] = VertexInterp(vec[1], vec[2], data[1], data[2], p[1], p[2], grad[1], grad[2], SurfaceValue, vtxCriticalSection, vertices, sectionBounds);
				}
				if (MC_EdgeTable[insideBits] & 4)
				{
					vertlist[2] = VertexInterp(vec[2], vec[3], data[2], data[3], p[2], p[3], grad[2], grad[3], SurfaceValue, vtxCriticalSection, vertices, sectionBounds);
				}
				if (MC_EdgeTable[insideBits] & 8)
				{
					vertlist[3] = VertexInterp(vec[3], vec[0], data[3], data[0], p[3], p[0], grad[3], grad[0], SurfaceValue, vtxCriticalSection, vertices, sectionBounds);
				}
				if (MC_EdgeTable[insideBits] & 16)
				{
					vertlist[4] = VertexInterp(vec[4], vec[5], data[4], data[5], p[4], p[5], grad[4], grad[5], SurfaceValue, vtxCriticalSection, vertices, sectionBounds);
				}
				if (MC_EdgeTable[insideBits] & 32)
				{
					vertlist[5] = VertexInterp(vec[5], vec[6], data[5], data[6], p[5], p[6], grad[5], grad[6], SurfaceValue, vtxCriticalSection, vertices, sectionBounds);
				}
				if (MC_EdgeTable[insideBits] & 64)
				{
					vertlist[6] = VertexInterp(vec[6], vec[7], data[6], data[7], p[6], p[7], grad[6], grad[7], SurfaceValue, vtxCriticalSection, vertices, sectionBounds);
				}
				if (MC_EdgeTable[insideBits] & 128)
				{
					vertlist[7] = VertexInterp(vec[7], vec[4], data[7], data[4], p[7], p[4], grad[7], grad[4], SurfaceValue, vtxCriticalSection, vertices, sectionBounds);
				}
				if (MC_EdgeTable[insideBits] & 256)
				{
					vertlist[8] = VertexInterp(vec[0], vec[4], data[0], data[4], p[0], p[4], grad[0], grad[4], SurfaceValue, vtxCriticalSection, vertices, sectionBounds);
				}
				if (MC_EdgeTable[insideBits] & 512)
				{
					vertlist[9] = VertexInterp(vec[1], vec[5], data[1], data[5], p[1], p[5], grad[1], grad[5], SurfaceValue, vtxCriticalSection, vertices, sectionBounds);
				}
				if (MC_EdgeTable[insideBits] & 1024)
				{
					vertlist[10] = VertexInterp(vec[2], vec[6], data[2], data[6], p[2], p[6], grad[2], grad[6], SurfaceValue, vtxCriticalSection, vertices, sectionBounds);
				}
				if (MC_EdgeTable[insideBits] & 2048)
				{
					vertlist[11] = VertexInterp(vec[3], vec[7], data[3], data[7], p[3], p[7], grad[3], grad[7], SurfaceValue, vtxCriticalSection, vertices, sectionBounds);
				}

				//Add the polygons and each of their backfaces
				for (int32 i = 0; MC_TriTable[insideBits][i].s != -1; i += 3)
				{
					check(i > -1 && i < 16);
					check(MC_TriTable[insideBits][i].s < 12);
					check(MC_TriTable[insideBits][i + 1].s > -1 && MC_TriTable[insideBits][i + 1].s < 12);
					check(MC_TriTable[insideBits][i + 2].s > -1 && MC_TriTable[insideBits][i + 2].s < 12);

					const IndexType idxsFrontFace[3] = {
						vertlist[MC_TriTable[insideBits][i].u],
						vertlist[MC_TriTable[insideBits][i + 1].u],
						vertlist[MC_TriTable[insideBits][i + 2].u]
					};
					const IndexType idxsBackFace[3] = {
						idxsFrontFace[2] + 1,
						idxsFrontFace[1] + 1,
						idxsFrontFace[0] + 1
					};
					check(idxsFrontFace[0] != idxsFrontFace[1] && idxsFrontFace[0] != idxsFrontFace[2] && idxsFrontFace[1] != idxsFrontFace[2]);

					//Create the front face and back face polygons
					const FVector surfaceNormal = FVector::CrossProduct(vertices[idxsFrontFace[1]].Position - vertices[idxsFrontFace[0]].Position, vertices[idxsFrontFace[2]].Position - vertices[idxsFrontFace[0]].Position);
					{
						FScopeLock lock(&vtxCriticalSection);
						vertices[idxsFrontFace[0]].Normal += surfaceNormal;
						vertices[idxsFrontFace[1]].Normal += surfaceNormal;
						vertices[idxsFrontFace[2]].Normal += surfaceNormal;
						vertices[idxsBackFace[0]].Normal -= surfaceNormal;
						vertices[idxsBackFace[1]].Normal -= surfaceNormal;
						vertices[idxsBackFace[2]].Normal -= surfaceNormal;
					}
					{
						FScopeLock lock(&triCriticalSection);
						polygons.Add(idxsFrontFace[0]);
						polygons.Add(idxsFrontFace[1]);
						polygons.Add(idxsFrontFace[2]);
						polygons.Add(idxsBackFace[0]);
						polygons.Add(idxsBackFace[1]);
						polygons.Add(idxsBackFace[2]);
					}
				}
			}

			static IndexType VertexInterp(const openvdb::Vec3d &vec1,
				const openvdb::Vec3d &vec2,
				const DataType &valp1,
				const DataType &valp2,
				const openvdb::Coord &c1,
				const openvdb::Coord &c2,
				const openvdb::Vec3f &g1,
				const openvdb::Vec3f &g2,
				const DataType &surfaceValue,
				FCriticalSection &criticalSection,
				TArray<FProcMeshVertex> &vertices,
				FBox &sectionBounds)
			{
				//TODO: Try Gram-Schmidt orthogonalization for tangents? (modifies the tangent to definitely be orthogonal to the normal):
				//tangent -= normal * tangent.dot( normal );
				//tangent.normalize();
				IndexType outVertex = -1;
				if (openvdb::math::isApproxEqual(valp1, surfaceValue) || openvdb::math::isApproxEqual(valp1, valp2))
				{
					FScopeLock lock(&criticalSection);
					outVertex = (IndexType)vertices.Add(constructFProcMeshVertex(
						FVector(vec1.x(), vec1.y(), vec1.z()),
						FVector::ZeroVector, //initialize normal to 0
						FProcMeshTangent(g1.x(), g1.y(), g1.z()),
						FColor(), //TODO colors
						FVector2D::ZeroVector //TODO uvs
						));
					vertices.Add(constructFProcMeshVertex(
						FVector(vec1.x(), vec1.y(), vec1.z()),
						FVector::ZeroVector, //initialize normal to 0
						FProcMeshTangent(-g1.x(), -g1.y(), -g1.z()),
						FColor(), //TODO colors
						FVector2D::ZeroVector //TODO uvs
						));
					sectionBounds += vertices[outVertex].Position;
				}
				else if (openvdb::math::isApproxEqual(valp2, surfaceValue))
				{
					FScopeLock lock(&criticalSection);
					outVertex = (IndexType)vertices.Add(constructFProcMeshVertex(
						FVector(vec2.x(), vec2.y(), vec2.z()),
						FVector::ZeroVector, //initialize normal to 0
						FProcMeshTangent(g2.x(), g2.y(), g2.z()),
						FColor(), //TODO colors
						FVector2D::ZeroVector //TODO uvs
						));
					vertices.Add(constructFProcMeshVertex(
						FVector(vec2.x(), vec2.y(), vec2.z()),
						FVector::ZeroVector, //initialize normal to 0
						FProcMeshTangent(g2.x(), g2.y(), g2.z()),
						FColor(), //TODO colors
						FVector2D::ZeroVector //TODO uvs
						));
					sectionBounds += vertices[outVertex].Position;
				}
				else
				{
					FScopeLock lock(&criticalSection);
					const float lerpScale = (surfaceValue - valp1) / (valp2 - valp1);
					const openvdb::Vec3d vtx = vec1 + (lerpScale*(vec2 - vec1));
					const openvdb::Vec3d grad = g1; //TODO: Figure out tangents on lerp'd vertices
					outVertex = (IndexType)vertices.Add(constructFProcMeshVertex(
						FVector(vtx.x(), vtx.y(), vtx.z()),
						FVector::ZeroVector, //initialize normal to 0
						FProcMeshTangent(grad.x(), grad.y(), grad.z()),
						FColor(), //TODO colors
						FVector2D::ZeroVector //TODO uvs
						));
					vertices.Add(constructFProcMeshVertex(
						FVector(vtx.x(), vtx.y(), vtx.z()),
						FVector::ZeroVector, //initialize normal to 0
						FProcMeshTangent(-grad.x(), -grad.y(), -grad.z()),
						FColor(), //TODO colors
						FVector2D::ZeroVector //TODO uvs
						));
					sectionBounds += vertices[outVertex].Position;
				}
				check(outVertex > -1);
				return outVertex;
			}

			GridType &Grid;
			openvdb::Grid<openvdb::Vec3fTree> Gradient;

		protected:
			const DataType &SurfaceValue;
			DataGridType &DataGrid;
			openvdb::tree::ValueAccessor<DataTreeType, true, DataTreeType::DEPTH-1, tbb::mutex> DataAcc;
			openvdb::tree::ValueAccessor<const openvdb::Vec3fTree, true, openvdb::Vec3fTree::DEPTH-1, tbb::mutex> GradAcc;
			TArray<FProcMeshSection> &SectionBuffers;
			FCriticalSection CriticalSections[((int32)(FVoxelData::VOXEL_TYPE_COUNT))*2];
		};

		//Operator to mesh a cube at each active voxel from a previously extracted isosurface
		template <typename TreeType, typename IterType>
		class CubesMeshOp
		{
		public:
			typedef typename openvdb::Grid<TreeType> GridType;
			typedef typename GridType::Ptr GridTypePtr;
			typedef typename GridType::Accessor AccessorType;
			typedef typename GridType::ConstAccessor CAccessorType;
			typedef typename GridType::ValueType ValueType;
			typedef typename IterType SourceIterType;

			CubesMeshOp(GridType &grid, TArray<FProcMeshSection> &sectionBuffers)
				: Grid(grid), SectionBuffers(sectionBuffers)
			{
			}

			OPENVDB_INLINE void operator()(const SourceIterType& iter)
			{
				check(iter.isVoxelValue() && !iter.isTileValue());
				openvdb::CoordBBox bbox;
				const bool hasVoxelVolume = iter.getBoundingBox(bbox) && bbox.hasVolume() && bbox.volume() == 1;
				check(hasVoxelVolume);

				//Mesh the voxel as a simple cube with 6 equal sized quads
				bbox.expand(bbox.min(), 2);
				const openvdb::BBoxd worldBBox = Grid.transform().indexToWorld(bbox);
				const openvdb::Vec3d vtxs[8] = {
					worldBBox.min(),
					openvdb::Vec3d(worldBBox.max().x(), worldBBox.min().y(), worldBBox.min().z()),
					openvdb::Vec3d(worldBBox.min().x(), worldBBox.min().y(), worldBBox.max().z()),
					openvdb::Vec3d(worldBBox.max().x(), worldBBox.min().y(), worldBBox.max().z()),
					openvdb::Vec3d(worldBBox.min().x(), worldBBox.max().y(), worldBBox.min().z()),
					openvdb::Vec3d(worldBBox.min().x(), worldBBox.max().y(), worldBBox.max().z()),
					openvdb::Vec3d(worldBBox.max().x(), worldBBox.max().y(), worldBBox.min().z()),
					worldBBox.max()
				};

				const ValueType value = iter.getValue();
				const int32 idx = (int32)value.VoxelType;
				check(value.VoxelType != EVoxelType::VOXEL_NONE && idx > -1 && idx < FVoxelData::VOXEL_TYPE_COUNT);
				FProcMeshSection &meshSection = SectionBuffers[idx];
				TArray<FProcMeshVertex> &vertices = meshSection.ProcVertexBuffer;
				TArray<int32> &polygons = meshSection.ProcIndexBuffer;
				FBox &sectionBounds = meshSection.SectionLocalBox;
				FCriticalSection &criticalSection = CriticalSections[idx];
				{
					FScopeLock lock(&criticalSection);
					//Add polygons each with unique vertices (vertex indices added clockwise order on each quad face)
					polygons.Add(vertices.Add(constructFProcMeshVertex(
						FVector(vtxs[0].x(), vtxs[0].y(), vtxs[0].z()),
						FVector(0.0f, -1.0f, 0.0f),
						FProcMeshTangent(0.0f, 0.0f, 1.0f),
						FColor(), //TODO colors
						FVector2D(1.0f / 3.0f, 0.5f)
						)));
					polygons.Add(vertices.Add(constructFProcMeshVertex(
						FVector(vtxs[2].x(), vtxs[2].y(), vtxs[2].z()),
						FVector(0.0f, -1.0f, 0.0f),
						FProcMeshTangent(0.0f, 0.0f, 1.0f),
						FColor(), //TODO colors
						FVector2D(1.0f / 3.0f, 0.25f)
						)));
					polygons.Add(vertices.Add(constructFProcMeshVertex(
						FVector(vtxs[3].x(), vtxs[3].y(), vtxs[3].z()),
						FVector(0.0f, -1.0f, 0.0f),
						FProcMeshTangent(0.0f, 0.0f, 1.0f),
						FColor(), //TODO colors
						FVector2D(2.0f / 3.0f, 0.25f)
						)));
					sectionBounds += FVector(vtxs[0].x(), vtxs[0].y(), vtxs[0].z());
					sectionBounds += FVector(vtxs[2].x(), vtxs[2].y(), vtxs[2].z());
					sectionBounds += FVector(vtxs[3].x(), vtxs[3].y(), vtxs[3].z());

					polygons.Add(vertices.Add(constructFProcMeshVertex(
						FVector(vtxs[3].x(), vtxs[3].y(), vtxs[3].z()),
						FVector(0.0f, -1.0f, 0.0f),
						FProcMeshTangent(0.0f, 0.0f, 1.0f),
						FColor(), //TODO colors
						FVector2D(2.0f / 3.0f, 0.25f)
						)));
					polygons.Add(vertices.Add(constructFProcMeshVertex(
						FVector(vtxs[1].x(), vtxs[1].y(), vtxs[1].z()),
						FVector(0.0f, -1.0f, 0.0f),
						FProcMeshTangent(0.0f, 0.0f, 1.0f),
						FColor(), //TODO colors
						FVector2D(2.0f / 3.0f, 0.5f)
						)));
					polygons.Add(vertices.Add(constructFProcMeshVertex(
						FVector(vtxs[0].x(), vtxs[0].y(), vtxs[0].z()),
						FVector(0.0f, -1.0f, 0.0f),
						FProcMeshTangent(0.0f, 0.0f, 1.0f),
						FColor(), //TODO colors
						FVector2D(1.0f / 3.0f, 0.5f)
						)));
					sectionBounds += FVector(vtxs[3].x(), vtxs[3].y(), vtxs[3].z());
					sectionBounds += FVector(vtxs[1].x(), vtxs[1].y(), vtxs[1].z());
					sectionBounds += FVector(vtxs[0].x(), vtxs[0].y(), vtxs[0].z());

					polygons.Add(vertices.Add(constructFProcMeshVertex(
						FVector(vtxs[1].x(), vtxs[1].y(), vtxs[1].z()),
						FVector(1.0f, 0.0f, 0.0f),
						FProcMeshTangent(0.0f, 0.0f, 1.0f),
						FColor(), //TODO colors
						FVector2D(2.0f / 3.0f, 0.5f)
						)));
					polygons.Add(vertices.Add(constructFProcMeshVertex(
						FVector(vtxs[3].x(), vtxs[3].y(), vtxs[3].z()),
						FVector(1.0f, 0.0f, 0.0f),
						FProcMeshTangent(0.0f, 0.0f, 1.0f),
						FColor(), //TODO colors
						FVector2D(1.0f, 0.5f)
						)));
					polygons.Add(vertices.Add(constructFProcMeshVertex(
						FVector(vtxs[7].x(), vtxs[7].y(), vtxs[7].z()),
						FVector(1.0f, 0.0f, 0.0f),
						FProcMeshTangent(0.0f, 0.0f, 1.0f),
						FColor(), //TODO colors
						FVector2D(1.0f, 0.75f)
						)));
					sectionBounds += FVector(vtxs[1].x(), vtxs[1].y(), vtxs[1].z());
					sectionBounds += FVector(vtxs[3].x(), vtxs[3].y(), vtxs[3].z());
					sectionBounds += FVector(vtxs[7].x(), vtxs[7].y(), vtxs[7].z());

					polygons.Add(vertices.Add(constructFProcMeshVertex(
						FVector(vtxs[7].x(), vtxs[7].y(), vtxs[7].z()),
						FVector(1.0f, 0.0f, 0.0f),
						FProcMeshTangent(0.0f, 0.0f, 1.0f),
						FColor(), //TODO colors
						FVector2D(1.0f, 0.75f)
						)));
					polygons.Add(vertices.Add(constructFProcMeshVertex(
						FVector(vtxs[6].x(), vtxs[6].y(), vtxs[6].z()),
						FVector(1.0f, 0.0f, 0.0f),
						FProcMeshTangent(0.0f, 0.0f, 1.0f),
						FColor(), //TODO colors
						FVector2D(2.0f / 3.0f, 0.75f)
						)));
					polygons.Add(vertices.Add(constructFProcMeshVertex(
						FVector(vtxs[1].x(), vtxs[1].y(), vtxs[1].z()),
						FVector(1.0f, 0.0f, 0.0f),
						FProcMeshTangent(0.0f, 0.0f, 1.0f),
						FColor(), //TODO colors
						FVector2D(2.0f / 3.0f, 0.5f)
						)));
					sectionBounds += FVector(vtxs[7].x(), vtxs[7].y(), vtxs[7].z());
					sectionBounds += FVector(vtxs[6].x(), vtxs[6].y(), vtxs[6].z());
					sectionBounds += FVector(vtxs[1].x(), vtxs[1].y(), vtxs[1].z());

					polygons.Add(vertices.Add(constructFProcMeshVertex(
						FVector(vtxs[4].x(), vtxs[4].y(), vtxs[4].z()),
						FVector(0.0f, 1.0f, 0.0f),
						FProcMeshTangent(0.0f, 0.0f, 1.0f),
						FColor(), //TODO colors
						FVector2D(1.0f / 3.0f, 0.75f)
						)));
					polygons.Add(vertices.Add(constructFProcMeshVertex(
						FVector(vtxs[6].x(), vtxs[6].y(), vtxs[6].z()),
						FVector(0.0f, 1.0f, 0.0f),
						FProcMeshTangent(0.0f, 0.0f, 1.0f),
						FColor(), //TODO colors
						FVector2D(2.0f / 3.0f, 0.75f)
						)));
					polygons.Add(vertices.Add(constructFProcMeshVertex(
						FVector(vtxs[7].x(), vtxs[7].y(), vtxs[7].z()),
						FVector(0.0f, 1.0f, 0.0f),
						FProcMeshTangent(0.0f, 0.0f, 1.0f),
						FColor(), //TODO colors
						FVector2D(2.0f / 3.0f, 1.0f)
						)));
					sectionBounds += FVector(vtxs[4].x(), vtxs[4].y(), vtxs[4].z());
					sectionBounds += FVector(vtxs[6].x(), vtxs[6].y(), vtxs[6].z());
					sectionBounds += FVector(vtxs[7].x(), vtxs[7].y(), vtxs[7].z());

					polygons.Add(vertices.Add(constructFProcMeshVertex(
						FVector(vtxs[7].x(), vtxs[7].y(), vtxs[7].z()),
						FVector(0.0f, 1.0f, 0.0f),
						FProcMeshTangent(0.0f, 0.0f, 1.0f),
						FColor(), //TODO colors
						FVector2D(2.0f / 3.0f, 1.0f)
						)));
					polygons.Add(vertices.Add(constructFProcMeshVertex(
						FVector(vtxs[5].x(), vtxs[5].y(), vtxs[5].z()),
						FVector(0.0f, 1.0f, 0.0f),
						FProcMeshTangent(0.0f, 0.0f, 1.0f),
						FColor(), //TODO colors
						FVector2D(1.0f / 3.0f, 1.0f)
						)));
					polygons.Add(vertices.Add(constructFProcMeshVertex(
						FVector(vtxs[4].x(), vtxs[4].y(), vtxs[4].z()),
						FVector(0.0f, 1.0f, 0.0f),
						FProcMeshTangent(0.0f, 0.0f, 1.0f),
						FColor(), //TODO colors
						FVector2D(1.0f / 3.0f, 0.75f)
						)));
					sectionBounds += FVector(vtxs[7].x(), vtxs[7].y(), vtxs[7].z());
					sectionBounds += FVector(vtxs[5].x(), vtxs[5].y(), vtxs[5].z());
					sectionBounds += FVector(vtxs[4].x(), vtxs[4].y(), vtxs[4].z());

					polygons.Add(vertices.Add(constructFProcMeshVertex(
						FVector(vtxs[0].x(), vtxs[0].y(), vtxs[0].z()),
						FVector(-1.0f, 0.0f, 0.0f),
						FProcMeshTangent(0.0f, 0.0f, 1.0f),
						FColor(), //TODO colors
						FVector2D(1.0f / 3.0f, 0.5f)
						)));
					polygons.Add(vertices.Add(constructFProcMeshVertex(
						FVector(vtxs[4].x(), vtxs[4].y(), vtxs[4].z()),
						FVector(-1.0f, 0.0f, 0.0f),
						FProcMeshTangent(0.0f, 0.0f, 1.0f),
						FColor(), //TODO colors
						FVector2D(1.0f / 3.0f, 0.75f)
						)));
					polygons.Add(vertices.Add(constructFProcMeshVertex(
						FVector(vtxs[5].x(), vtxs[5].y(), vtxs[5].z()),
						FVector(-1.0f, 0.0f, 0.0f),
						FProcMeshTangent(0.0f, 0.0f, 1.0f),
						FColor(), //TODO colors
						FVector2D(0.0f, 0.75f)
						)));
					sectionBounds += FVector(vtxs[0].x(), vtxs[0].y(), vtxs[0].z());
					sectionBounds += FVector(vtxs[4].x(), vtxs[4].y(), vtxs[4].z());
					sectionBounds += FVector(vtxs[5].x(), vtxs[5].y(), vtxs[5].z());

					polygons.Add(vertices.Add(constructFProcMeshVertex(
						FVector(vtxs[5].x(), vtxs[5].y(), vtxs[5].z()),
						FVector(-1.0f, 0.0f, 0.0f),
						FProcMeshTangent(0.0f, 0.0f, 1.0f),
						FColor(), //TODO colors
						FVector2D(0.0f, 0.75f)
						)));
					polygons.Add(vertices.Add(constructFProcMeshVertex(
						FVector(vtxs[2].x(), vtxs[2].y(), vtxs[2].z()),
						FVector(-1.0f, 0.0f, 0.0f),
						FProcMeshTangent(0.0f, 0.0f, 1.0f),
						FColor(), //TODO colors
						FVector2D(0.0f, 0.5f)
						)));
					polygons.Add(vertices.Add(constructFProcMeshVertex(
						FVector(vtxs[0].x(), vtxs[0].y(), vtxs[0].z()),
						FVector(-1.0f, 0.0f, 0.0f),
						FProcMeshTangent(0.0f, 0.0f, 1.0f),
						FColor(), //TODO colors
						FVector2D(1.0f / 3.0f, 0.5f)
						)));
					sectionBounds += FVector(vtxs[5].x(), vtxs[5].y(), vtxs[5].z());
					sectionBounds += FVector(vtxs[2].x(), vtxs[2].y(), vtxs[2].z());
					sectionBounds += FVector(vtxs[0].x(), vtxs[0].y(), vtxs[0].z());

					polygons.Add(vertices.Add(constructFProcMeshVertex(
						FVector(vtxs[2].x(), vtxs[2].y(), vtxs[2].z()),
						FVector(0.0f, 0.0f, 1.0f),
						FProcMeshTangent(0.0f, 0.0f, 1.0f),
						FColor(), //TODO colors
						FVector2D(1.0f / 3.0f, 0.25f)
						)));
					polygons.Add(vertices.Add(constructFProcMeshVertex(
						FVector(vtxs[5].x(), vtxs[5].y(), vtxs[5].z()),
						FVector(0.0f, 0.0f, 1.0f),
						FProcMeshTangent(0.0f, 0.0f, 1.0f),
						FColor(), //TODO colors
						FVector2D(1.0f / 3.0f, 0.0f)
						)));
					polygons.Add(vertices.Add(constructFProcMeshVertex(
						FVector(vtxs[7].x(), vtxs[7].y(), vtxs[7].z()),
						FVector(0.0f, 0.0f, 1.0f),
						FProcMeshTangent(0.0f, 0.0f, 1.0f),
						FColor(), //TODO colors
						FVector2D(2.0f / 3.0f, 0.0f)
						)));
					sectionBounds += FVector(vtxs[2].x(), vtxs[2].y(), vtxs[2].z());
					sectionBounds += FVector(vtxs[5].x(), vtxs[5].y(), vtxs[5].z());
					sectionBounds += FVector(vtxs[7].x(), vtxs[7].y(), vtxs[7].z());

					polygons.Add(vertices.Add(constructFProcMeshVertex(
						FVector(vtxs[7].x(), vtxs[7].y(), vtxs[7].z()),
						FVector(0.0f, 0.0f, 1.0f),
						FProcMeshTangent(1.0f, 0.0f, 0.0f),
						FColor(), //TODO colors
						FVector2D(2.0f / 3.0f, 0.0f)
						)));
					polygons.Add(vertices.Add(constructFProcMeshVertex(
						FVector(vtxs[3].x(), vtxs[3].y(), vtxs[3].z()),
						FVector(0.0f, 0.0f, 1.0f),
						FProcMeshTangent(1.0f, 0.0f, 0.0f),
						FColor(), //TODO colors
						FVector2D(2.0f / 3.0f, 0.25f)
						)));
					polygons.Add(vertices.Add(constructFProcMeshVertex(
						FVector(vtxs[2].x(), vtxs[2].y(), vtxs[2].z()),
						FVector(0.0f, 0.0f, 1.0f),
						FProcMeshTangent(1.0f, 0.0f, 0.0f),
						FColor(), //TODO colors
						FVector2D(1.0f / 3.0f, 0.25f)
						)));
					sectionBounds += FVector(vtxs[7].x(), vtxs[7].y(), vtxs[7].z());
					sectionBounds += FVector(vtxs[3].x(), vtxs[3].y(), vtxs[3].z());
					sectionBounds += FVector(vtxs[2].x(), vtxs[2].y(), vtxs[2].z());

					polygons.Add(vertices.Add(constructFProcMeshVertex(
						FVector(vtxs[0].x(), vtxs[0].y(), vtxs[0].z()),
						FVector(0.0f, 0.0f, -1.0f),
						FProcMeshTangent(1.0f, 0.0f, 0.0f),
						FColor(), //TODO colors
						FVector2D(1.0f / 3.0f, 0.5f)
						)));
					polygons.Add(vertices.Add(constructFProcMeshVertex(
						FVector(vtxs[1].x(), vtxs[1].y(), vtxs[1].z()),
						FVector(0.0f, 0.0f, -1.0f),
						FProcMeshTangent(1.0f, 0.0f, 0.0f),
						FColor(), //TODO colors
						FVector2D(2.0f / 3.0f, 0.5f)
						)));
					polygons.Add(vertices.Add(constructFProcMeshVertex(
						FVector(vtxs[6].x(), vtxs[6].y(), vtxs[6].z()),
						FVector(0.0f, 0.0f, -1.0f),
						FProcMeshTangent(1.0f, 0.0f, 0.0f),
						FColor(), //TODO colors
						FVector2D(2.0f / 3.0f, 0.75f)
						)));
					sectionBounds += FVector(vtxs[0].x(), vtxs[0].y(), vtxs[0].z());
					sectionBounds += FVector(vtxs[1].x(), vtxs[1].y(), vtxs[1].z());
					sectionBounds += FVector(vtxs[6].x(), vtxs[6].y(), vtxs[6].z());

					polygons.Add(vertices.Add(constructFProcMeshVertex(
						FVector(vtxs[6].x(), vtxs[6].y(), vtxs[6].z()),
						FVector(0.0f, 0.0f, -1.0f),
						FProcMeshTangent(1.0f, 0.0f, 0.0f),
						FColor(), //TODO colors
						FVector2D(2.0f / 3.0f, 0.75f)
						)));
					polygons.Add(vertices.Add(constructFProcMeshVertex(
						FVector(vtxs[4].x(), vtxs[4].y(), vtxs[4].z()),
						FVector(0.0f, 0.0f, -1.0f),
						FProcMeshTangent(1.0f, 0.0f, 0.0f),
						FColor(), //TODO colors
						FVector2D(1.0f / 3.0f, 0.75f)
						)));
					polygons.Add(vertices.Add(constructFProcMeshVertex(
						FVector(vtxs[0].x(), vtxs[0].y(), vtxs[0].z()),
						FVector(0.0f, 0.0f, -1.0f),
						FProcMeshTangent(1.0f, 0.0f, 0.0f),
						FColor(), //TODO colors
						FVector2D(1.0f / 3.0f, 0.5f)
						)));
					sectionBounds += FVector(vtxs[6].x(), vtxs[6].y(), vtxs[6].z());
					sectionBounds += FVector(vtxs[4].x(), vtxs[4].y(), vtxs[4].z());
					sectionBounds += FVector(vtxs[0].x(), vtxs[0].y(), vtxs[0].z());
				}
			}

		protected:
			GridType &Grid;
			TArray<FProcMeshSection> &SectionBuffers;
			FCriticalSection CriticalSections[FVoxelData::VOXEL_TYPE_COUNT];
		};

		//Helper struct to hold the associated basic cubes meshing info
		template <typename SourceTreeType>
		class CubeMesher :
			public CubesMeshOp<SourceTreeType, typename SourceTreeType::ValueOnCIter>
		{
		public:
			CubeMesher(GridType &grid, TArray<FProcMeshSection> &sectionBuffers)
				: isChanged(true), CubesMeshOp(grid, sectionBuffers)
			{
			}

			OPENVDB_INLINE void clearGeometry()
			{
				//TODO: Clear the polygon/UV map buffer of the requisite voxel type
			}

			OPENVDB_INLINE void doMeshOp(const bool &threaded)
			{
				if (isChanged)
				{
					clearGeometry();
					openvdb::tools::valxform::SharedOpApplier<SourceIterType, CubeMesher<SourceTreeType>> proc(Grid.cbeginValueOn(), *this);
					proc.process(threaded);
				}
				isChanged = false;
			}

			OPENVDB_INLINE void markChanged()
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
			MarchingCubesMesher(DataGridType &dataGrid, TArray<FProcMeshSection> &sectionBuffers, GridTypePtr gridPtr = GridType::create(0))
				: isChanged(true), GridPtr(gridPtr), MarchingCubesMeshOp(*gridPtr, dataGrid, sectionBuffers)
			{
			}

			OPENVDB_INLINE void clearGeometry()
			{
				//TODO: Clear the polygon/UV map buffer of the requisite voxel type
			}

			OPENVDB_INLINE void doMeshOp(const bool &threaded)
			{
				if (isChanged)
				{
					clearGeometry();
					openvdb::tools::valxform::SharedOpApplier<SourceIterType, MarchingCubesMesher<SourceTreeType>> proc(Grid.cbeginValueOn(), *this);
					proc.process(threaded);

					//TODO
					////Normalize the cross product averages among the surface normal sums of each mesh section
					//for (int32 i = 0; i < (int32)FVoxelData::VOXEL_TYPE_COUNT; ++i)
					//{
					//	if (i == (int32)EVoxelType::VOXEL_NONE)
					//	{
					//		check(MeshBuffers[i].VertexBuffer.Num() == 0);
					//		check(MeshBuffers[i].NormalBuffer.Num() == 0);
					//		check(MeshBuffers[i].VertexColorBuffer.Num() == 0);
					//		check(MeshBuffers[i].TangentBuffer.Num() == 0);
					//		check(MeshBuffers[i].PolygonBuffer.Num() == 0);
					//		check(MeshBuffers[i].UVMapBuffer.Num() == 0);
					//		continue;
					//	}
					//	for (auto j = MeshBuffers[i].NormalBuffer.CreateIterator(); j; ++j)
					//	{
					//		const int32 &normIdx = j.GetIndex();
					//		check(MeshBuffers[i].NumTris[normIdx] > 0);
					//		*j = ((*j) / MeshBuffers[i].NumTris[normIdx]).GetSafeNormal();
					//	}
					//}
				}
				isChanged = false;
			}

			OPENVDB_INLINE void markChanged()
			{
				isChanged = true;
			}

			bool isChanged;

		private:
			const GridTypePtr GridPtr;
		};
	}
}