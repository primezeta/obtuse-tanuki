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
		typedef int32 IndexType;
		typedef openvdb::tree::Tree4<IndexType, 5, 4, 3>::Type IndexTreeType; //Same tree configuration (5,4,3) as openvdb::FloatTree (see openvdb.h)
		const static IndexType UNVISITED_VERTEX_INDEX = -1;

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

		//Operator to fill a grid with Perlin noise values (no tiles)
		template <typename InTreeType, typename InIterType, typename OutTreeType>
		class PerlinNoiseFillOp
		{
		public:
			typedef typename openvdb::Grid<OutTreeType> DestGridType;
			typedef typename DestGridType::Ptr DestGridTypePtr;
			typedef typename DestGridType::Accessor DestAccessorType;
			typedef typename OutTreeType::ValueType DestValueType;

			PerlinNoiseFillOp(const DestGridTypePtr destGridPtr, int32 seed, float frequency, float lacunarity, float persistence, int32 octaveCount)
				: DestGridPtr(destGridPtr)
			{
				valueSource.SetSeed(seed);
				valueSource.SetFrequency((double)frequency);
				valueSource.SetLacunarity((double)lacunarity);
				valueSource.SetPersistence((double)persistence);
				valueSource.SetOctaveCount(octaveCount);
			}

			inline void operator()(const InIterType& iter, DestAccessorType& acc)
			{
				const openvdb::Coord &coord = iter.getCoord();
				const bool &isMaskActive = iter.getValue();
				DestValueType outValue;
				GetValue(coord, outValue);
				if (!isMaskActive)
				{
					acc.setValueOff(coord, outValue);
				}
				else
				{
					acc.setValueOn(coord, outValue);
				}
			}

			inline void GetValue(const openvdb::Coord &coord, DestValueType &outValue)
			{
				//double prevLacunarity = valueSource.GetLacunarity();
				//int32 prevOctaveCount = valueSource.GetOctaveCount();
				//valueSource.SetLacunarity(prevLacunarity*0.004);
				//valueSource.SetOctaveCount(2);
				//const openvdb::Vec3d vec = DestGridPtr->transform().indexToWorld(coord);
				//double warp = valueSource.GetValue(vec.x(), vec.y(), vec.z()) * 8;
				//valueSource.SetLacunarity(prevLacunarity);
				//valueSource.SetOctaveCount(prevOctaveCount);
				//outValue.Data = (DestValueType::DataType)(warp + valueSource.GetValue(vec.x(), vec.y(), vec.z()) + vec.z());
				const openvdb::Vec3d vec = DestGridPtr->transform().indexToWorld(coord);
				outValue.Data = (DestValueType::DataType)(valueSource.GetValue(vec.x(), vec.y(), vec.z()) + vec.z());
			}

		private:
			const DestGridTypePtr DestGridPtr;
			noise::module::Perlin valueSource;
		};

		//Operator to extract an isosurface from a grid
		template <typename TreeType, typename IterType>
		class ExtractSurfaceOp
		{
		public:
			typedef typename openvdb::Grid<TreeType> GridType;
			typedef typename GridType::Ptr GridTypePtr;
			typedef typename GridType::ConstAccessor AccessorType;
			typedef typename GridType::ValueType ValueType;
			typedef typename IterType SourceIterType;

			ExtractSurfaceOp(const GridTypePtr gridPtr)
				: GridPtr(gridPtr), GridAcc(gridPtr->getConstAccessor()), SurfaceValue(GridPtr->tree().background())
			{
			}

			inline void operator()(const SourceIterType& iter)
			{
				//Note that no special consideration is done for tile voxels, so the grid tiles must be voxelized prior to this op [tree().voxelizeActiveTiles()]
				uint8 insideBits = 0;
				const openvdb::Coord &coord = iter.getCoord();
				auto acc = GridPtr->getAccessor();
				if (acc.getValue(coord).Data < SurfaceValue.Data) { insideBits |= 1; }
				if (acc.getValue(coord.offsetBy(1, 0, 0)).Data < SurfaceValue.Data) { insideBits |= 2; }
				if (acc.getValue(coord.offsetBy(0, 1, 0)).Data < SurfaceValue.Data) { insideBits |= 4; }
				if (acc.getValue(coord.offsetBy(0, 0, 1)).Data < SurfaceValue.Data) { insideBits |= 8; }
				if (acc.getValue(coord.offsetBy(1, 1, 0)).Data < SurfaceValue.Data) { insideBits |= 16; }
				if (acc.getValue(coord.offsetBy(1, 0, 1)).Data < SurfaceValue.Data) { insideBits |= 32; }
				if (acc.getValue(coord.offsetBy(0, 1, 1)).Data < SurfaceValue.Data) { insideBits |= 64; }
				if (acc.getValue(coord.offsetBy(1, 1, 1)).Data < SurfaceValue.Data) { insideBits |= 128; }
				//Turn the voxel off if it is completely outside or completely inside the surface
				if (insideBits == 0 || insideBits == 255)
				{
					acc.setValueOff(coord);
				}
			}

		private:
			const GridTypePtr GridPtr;
			AccessorType GridAcc;
			const ValueType &SurfaceValue;
		};

		//Operator to generate geometry from active voxels (no tiles)
		template <typename TreeType, typename IterType>
		class MeshGeometryOp
		{
		public:
			typedef typename openvdb::Grid<TreeType> GridType;
			typedef typename GridType::Ptr GridTypePtr;
			typedef typename GridType::Accessor AccessorType;
			typedef typename GridType::ValueType ValueType;
			typedef typename IterType SourceIterType;

			MeshGeometryOp(const GridTypePtr gridPtr, TArray<FVector> &vertexBuffer, TArray<int32> &polygonBuffer, TArray<FVector> &normalBuffer, TArray<FVector2D> &uvBuffer, TArray<FColor> &colorBuffer, TArray<FProcMeshTangent> &tangentBuffer)
				: GridPtr(gridPtr), GridAcc(gridPtr->tree()), UnvisitedVertexIndex(GridPtr->tree().background()), vertices(vertexBuffer), polygons(polygonBuffer), normals(normalBuffer), uvs(uvBuffer), colors(colorBuffer), tangents(tangentBuffer)
			{
			}

			inline void operator()(const SourceIterType& iter)
			{
				//Mesh the voxel as a simple cube with 6 equal sized quads
				const openvdb::Coord coord = iter.getCoord();
				const openvdb::CoordBBox bbox = openvdb::CoordBBox::createCube(coord, 2);
				const int32_t &minX = coord.x();
				const int32_t &minY = coord.y();
				const int32_t &minZ = coord.z();
				const int32_t &maxX = bbox.max().x();
				const int32_t &maxY = bbox.max().y();
				const int32_t &maxZ = bbox.max().z();
				ValueType outValues[8];
				openvdb::Coord vertexCoord;
				for (int32_t x = minX; x <= maxX; ++x)
				{
					vertexCoord.setX(x);
					for (int32_t y = minY; y <= maxY; ++y)
					{
						vertexCoord.setY(y);
						for (int32_t z = minZ; z <= maxZ; ++z)
						{
							vertexCoord.setZ(z);
							ValueType outValue;
							{
								FScopeLock lock(&CriticalSection);
								GetAndSetValue(vertexCoord, outValue);
							}
							outValues[(z - minZ) + ((y - minY) << 1) + ((x - minX) << 2)] = outValue;
						}
					}
				}

				//Add the vertex indices in counterclockwise order on each quad face
				quads.Enqueue(openvdb::Vec4i(outValues[1], outValues[3], outValues[7], outValues[5])); //Upper XY
				quads.Enqueue(openvdb::Vec4i(outValues[6], outValues[2], outValues[0], outValues[4])); //Lower XY
				quads.Enqueue(openvdb::Vec4i(outValues[7], outValues[3], outValues[2], outValues[6])); //Back XZ
				quads.Enqueue(openvdb::Vec4i(outValues[5], outValues[4], outValues[0], outValues[1])); //Front XZ
				quads.Enqueue(openvdb::Vec4i(outValues[7], outValues[6], outValues[4], outValues[5])); //Right YZ
				quads.Enqueue(openvdb::Vec4i(outValues[0], outValues[2], outValues[3], outValues[1])); //Left YZ
			}

			inline void GetAndSetValue(const openvdb::Coord &coord, ValueType &outValue)
			{
				//The vertex value may have already been calculated by someone else since voxels share vertices so first check if the vertex exists
				if (GridAcc.getValue(coord) != UnvisitedVertexIndex)
				{
					//Get the saved vertex index
					outValue = GridAcc.getValue(coord);
				}
				else
				{
					//Add to the vertex array and save the index of the new vertex
					const openvdb::Vec3d vtx = GridPtr->transform().indexToWorld(coord);
					outValue = (ValueType)(vertices.Add(FVector((float)vtx.x(), (float)vtx.y(), (float)vtx.z())));
					GridAcc.setValueOnly(coord, outValue);
				}
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

				//Add dummy values for now TODO
				normals.Add(FVector());
				uvs.Add(FVector2D());
				colors.Add(FColor());
				tangents.Add(FProcMeshTangent());
			}

		protected:
			FCriticalSection CriticalSection;
			const GridTypePtr GridPtr;
			AccessorType GridAcc;
			const ValueType &UnvisitedVertexIndex;
			TQueue<openvdb::Vec4i, EQueueMode::Mpsc> quads;
			TArray<FVector> &vertices;
			TArray<int32> &polygons;
			TArray<FVector> &normals;
			TArray<FVector2D> &uvs;
			TArray<FColor> &colors;
			TArray<FProcMeshTangent> &tangents;
		};

		//Helper struct to hold the associated grid meshing info
		template <typename SourceTreeType>
		class BasicMesher :
			public MeshGeometryOp<IndexTreeType, typename SourceTreeType::ValueOnCIter>
		{
		public:
			typedef typename openvdb::Grid<SourceTreeType> SourceGridType;
			typedef typename SourceGridType::Ptr SourceGridTypePtr;
			const SourceGridTypePtr SourceGridPtr;

			BasicMesher(const SourceGridTypePtr sourceGridPtr, TArray<FVector> &vertexBuffer, TArray<int32> &polygonBuffer, TArray<FVector> &normalBuffer, TArray<FVector2D> &uvBuffer, TArray<FColor> &colorBuffer, TArray<FProcMeshTangent> &tangentBuffer)
				: SourceGridPtr(sourceGridPtr), MeshGeometryOp(GridType::create(UNVISITED_VERTEX_INDEX), vertexBuffer, polygonBuffer, normalBuffer, uvBuffer, colorBuffer, tangentBuffer), isChanged(true)
			{
				GridPtr->setName(SourceGridPtr->getName() + ".indices");
				GridPtr->setTransform(SourceGridPtr->transformPtr());
			}

			inline void clearBuffers()
			{
				vertices.Empty();
				polygons.Empty();
				normals.Empty();
				uvs.Empty();
				colors.Empty();
				tangents.Empty();
			}

			inline void doMeshOp(openvdb::BBoxd &activeWorldBBox, openvdb::Vec3d &startWorldCoord, openvdb::Vec3d &voxelSize)
			{
				if (isChanged)
				{
					clearBuffers();
					GridPtr->clear();
					GridPtr->topologyUnion(*SourceGridPtr);
					openvdb::tools::valxform::SharedOpApplier<SourceIterType, BasicMesher<SourceTreeType>> proc(SourceGridPtr->cbeginValueOn(), *this);
					const bool threaded = true;
					UE_LOG(LogOpenVDBModule, Display, TEXT("%s"), *FString::Printf(TEXT("%s (pre mesh op) %d active voxels"), UTF8_TO_TCHAR(GridPtr->getName().c_str()), GridPtr->activeVoxelCount()));
					proc.process(threaded);
					collectPolygons();
				}
				UE_LOG(LogOpenVDBModule, Display, TEXT("%s"), *FString::Printf(TEXT("%s (post mesh op) %d active voxels"), UTF8_TO_TCHAR(GridPtr->getName().c_str()), GridPtr->activeVoxelCount()));

				openvdb::Coord coord;
				openvdb::CoordBBox activeIndexBBox;
				GetFirstInactiveVoxelFromActive(coord, activeIndexBBox); //TODO: Properly handle when no such voxels are found in the entire grid
				activeWorldBBox = GridPtr->transform().indexToWorld(activeIndexBBox);
				startWorldCoord = GridPtr->indexToWorld(coord);
				voxelSize = GridPtr->voxelSize();
				isChanged = false;
			}

			inline void markChanged()
			{
				isChanged = true;
			}

			void GetFirstInactiveVoxelFromActive(openvdb::Coord &coord, openvdb::CoordBBox &activeIndexBBox) const
			{
				activeIndexBBox = GridPtr->evalActiveVoxelBoundingBox();
				for (auto i = GridPtr->cbeginValueOn(); i; ++i)
				{
					if (i.isVoxelValue())
					{
						coord = i.getCoord();
						//Find the first voxel above that is off
						for (int32_t z = i.getCoord().z(); z <= activeIndexBBox.max().z(); ++z)
						{
							coord.setZ(z);
							if (i.getTree()->isValueOff(coord))
							{
								return;
							}
						}
					}
				}
				coord = openvdb::Coord(0, 0, 0);
			}

			bool isChanged;
		};
	}
}