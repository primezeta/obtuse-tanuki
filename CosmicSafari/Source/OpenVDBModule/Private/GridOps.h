#pragma once
#include "EngineMinimal.h"
#pragma warning(push)
#pragma warning(1:4211 4800 4503 4146)
#include <openvdb/openvdb.h>
#include <openvdb/tools/ValueTransformer.h>
#include <noise.h>
#include <noiseutils.h>
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

			inline void operator()(const IterType& iter)
			{
				//Note that no special consideration is done for tile voxels, so the grid tiles must be voxelized prior to this op [tree().voxelizeActiveTiles()]
				uint8 insideBits = 0;
				const openvdb::Coord &coord = iter.getCoord();
				auto acc = GridPtr->getConstAccessor();
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
					iter.setValueOff();
				}
			}

		private:
			GridTypePtr GridPtr;
			const ValueType &SurfaceValue;
		};

		//Operator to extract an isosurface from a grid
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

			inline void operator()(const SourceIterType& iter, DestAccessorType& acc)
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
				//Turn the voxel off if it is completely outside or completely inside the surface
				if (insideBits == 0 || insideBits == 255)
				{
					acc.setValueOff(coord, insideBits);
				}
				else
				{
					acc.setValueOn(coord, insideBits);
				}
			}

		private:
			SourceGridTypePtr SourceGridPtr;
			const SourceValueType &SurfaceValue;
		};

		//Operator to extract an isosurface from a grid via the Marching Cubes algorithm
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
				: GridPtr(gridPtr), DataGridPtr(dataGridPtr), Acc(gridPtr->getAccessor()), Xform(dataGridPtr->transform()), SurfaceValue(dataGridPtr->tree().background().Data), DataAcc(dataGridPtr->getAccessor()), vertices(vertexBuffer), polygons(polygonBuffer), normals(normalBuffer), uvs(uvBuffer), colors(colorBuffer), tangents(tangentBuffer)
			{
			}

			inline void operator()(const IterType& iter)
			{
				const uint8 &insideBits = iter.getValue();
				if (MC_EdgeTable[insideBits] == 0)
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
				openvdb::Vec3d vertlist[12];
				if (MC_EdgeTable[insideBits] & 1)
				{
					VertexInterp(vec[0], vec[1], val[0], val[1], vertlist[0]);
				}
				if (MC_EdgeTable[insideBits] & 2)
				{
					VertexInterp(vec[1], vec[2], val[1], val[2], vertlist[1]);
				}
				if (MC_EdgeTable[insideBits] & 4)
				{
					VertexInterp(vec[2], vec[3], val[2], val[3], vertlist[2]);
				}
				if (MC_EdgeTable[insideBits] & 8)
				{
					VertexInterp(vec[3], vec[0], val[3], val[0], vertlist[3]);
				}
				if (MC_EdgeTable[insideBits] & 16)
				{
					VertexInterp(vec[4], vec[5], val[4], val[5], vertlist[4]);
				}
				if (MC_EdgeTable[insideBits] & 32)
				{
					VertexInterp(vec[5], vec[6], val[5], val[6], vertlist[5]);
				}
				if (MC_EdgeTable[insideBits] & 64)
				{
					VertexInterp(vec[6], vec[7], val[6], val[7], vertlist[6]);
				}
				if (MC_EdgeTable[insideBits] & 128)
				{
					VertexInterp(vec[7], vec[4], val[7], val[4], vertlist[7]);
				}
				if (MC_EdgeTable[insideBits] & 256)
				{
					VertexInterp(vec[0], vec[4], val[0], val[4], vertlist[8]);
				}
				if (MC_EdgeTable[insideBits] & 512)
				{
					VertexInterp(vec[1], vec[5], val[1], val[5], vertlist[9]);
				}
				if (MC_EdgeTable[insideBits] & 1024)
				{
					VertexInterp(vec[2], vec[6], val[2], val[6], vertlist[10]);
				}
				if (MC_EdgeTable[insideBits] & 2048)
				{
					VertexInterp(vec[3], vec[7], val[3], val[7], vertlist[11]);
				}

				// Create the triangle
				for (int32_t i = 0; MC_TriTable[insideBits][i] != -1; i += 3)
				{
					const openvdb::Vec3d &vertex0 = vertlist[MC_TriTable[insideBits][i]];
					const openvdb::Vec3d &vertex1 = vertlist[MC_TriTable[insideBits][i + 1]];
					const openvdb::Vec3d &vertex2 = vertlist[MC_TriTable[insideBits][i + 2]];
					{
						FScopeLock lock(&CriticalSection);
						FVector vertex;

						vertex.X = vertex0.x();
						vertex.Y = vertex0.y();
						vertex.Z = vertex0.z();
						polygons.Add(vertices.Add(vertex));

						vertex.X = vertex1.x();
						vertex.Y = vertex1.y();
						vertex.Z = vertex1.z();
						polygons.Add(vertices.Add(vertex));

						vertex.X = vertex2.x();
						vertex.Y = vertex2.y();
						vertex.Z = vertex2.z();
						polygons.Add(vertices.Add(vertex));

						//Add dummy values for now TODO
						normals.Add(FVector());
						uvs.Add(FVector2D());
						colors.Add(FColor());
						tangents.Add(FProcMeshTangent());
					}
				}
			}

			void VertexInterp(const openvdb::Vec3d &vec1, const openvdb::Vec3d &vec2, const DataType &valp1, const DataType &valp2, openvdb::Vec3d &outVertex)
			{
				if (openvdb::math::isApproxEqual(valp1, SurfaceValue))
				{
					outVertex = vec1;
				}
				else if (openvdb::math::isApproxEqual(valp2, SurfaceValue))
				{
					outVertex = vec2;
				}
				else if (openvdb::math::isApproxEqual(valp1, valp2))
				{
					outVertex = vec1;
				}
				else
				{
					const double mu = (SurfaceValue - valp1) / (valp2 - valp1);
					outVertex.x() = vec1.x() + mu * (vec2.x() - vec1.x());
					outVertex.y() = vec1.y() + mu * (vec2.y() - vec1.y());
					outVertex.z() = vec1.z() + mu * (vec2.z() - vec1.z());
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
			TArray<FVector> &vertices;
			TArray<int32> &polygons;
			TArray<FVector> &normals;
			TArray<FVector2D> &uvs;
			TArray<FColor> &colors;
			TArray<FProcMeshTangent> &tangents;
		};

		//Operator to generate geometry from active voxels (no tiles)
		template <typename TreeType, typename IterType>
		class CubesMeshOp
		{
		public:
			typedef typename openvdb::Grid<TreeType> GridType;
			typedef typename GridType::Ptr GridTypePtr;
			typedef typename GridType::Accessor AccessorType;
			typedef typename GridType::ValueType ValueType;
			typedef typename IterType SourceIterType;

			CubesMeshOp(const GridTypePtr gridPtr, TArray<FVector> &vertexBuffer, TArray<int32> &polygonBuffer, TArray<FVector> &normalBuffer, TArray<FVector2D> &uvBuffer, TArray<FColor> &colorBuffer, TArray<FProcMeshTangent> &tangentBuffer)
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
		class CubeMesher :
			public CubesMeshOp<IndexTreeType, typename SourceTreeType::ValueOnCIter>
		{
		public:
			typedef typename openvdb::Grid<SourceTreeType> SourceGridType;
			typedef typename SourceGridType::Ptr SourceGridTypePtr;
			const SourceGridTypePtr SourceGridPtr;

			CubeMesher(const SourceGridTypePtr sourceGridPtr, TArray<FVector> &vertexBuffer, TArray<int32> &polygonBuffer, TArray<FVector> &normalBuffer, TArray<FVector2D> &uvBuffer, TArray<FColor> &colorBuffer, TArray<FProcMeshTangent> &tangentBuffer)
				: SourceGridPtr(sourceGridPtr), CubesMeshOp(GridType::create(UNVISITED_VERTEX_INDEX), vertexBuffer, polygonBuffer, normalBuffer, uvBuffer, colorBuffer, tangentBuffer), isChanged(true)
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
					openvdb::tools::valxform::SharedOpApplier<SourceIterType, CubeMesher<SourceTreeType>> proc(SourceGridPtr->cbeginValueOn(), *this);
					const bool threaded = true;
					UE_LOG(LogOpenVDBModule, Display, TEXT("%s"), *FString::Printf(TEXT("%s (pre mesh op) %d active voxels"), UTF8_TO_TCHAR(GridPtr->getName().c_str()), GridPtr->activeVoxelCount()));
					proc.process(threaded);
					collectPolygons();
				}
				UE_LOG(LogOpenVDBModule, Display, TEXT("%s"), *FString::Printf(TEXT("%s (post mesh op) %d active voxels"), UTF8_TO_TCHAR(GridPtr->getName().c_str()), GridPtr->activeVoxelCount()));

				openvdb::Coord coord;
				openvdb::CoordBBox activeIndexBBox;
				GetFirstInactiveVoxelFromActive<GridType>(GridPtr, coord, activeIndexBBox); //TODO: Properly handle when no such voxels are found in the entire grid
				activeWorldBBox = GridPtr->transform().indexToWorld(activeIndexBBox);
				startWorldCoord = GridPtr->indexToWorld(coord);
				voxelSize = GridPtr->voxelSize();
				isChanged = false;
			}

			inline void markChanged()
			{
				isChanged = true;
			}

			bool isChanged;
		};

		//Helper struct to hold the associated grid meshing info
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
					//GridPtr->clear();
					GridPtr->topologyUnion(*DataGridPtr);
					openvdb::tools::valxform::SharedOpApplier<SourceIterType, MarchingCubesMesher<SourceTreeType>> proc(GridPtr->cbeginValueOn(), *this);
					const bool threaded = true;
					UE_LOG(LogOpenVDBModule, Display, TEXT("%s"), *FString::Printf(TEXT("%s (pre mesh op) %d active voxels"), UTF8_TO_TCHAR(GridPtr->getName().c_str()), GridPtr->activeVoxelCount()));
					proc.process(threaded);
				}
				UE_LOG(LogOpenVDBModule, Display, TEXT("%s"), *FString::Printf(TEXT("%s (post mesh op) %d active voxels"), UTF8_TO_TCHAR(GridPtr->getName().c_str()), GridPtr->activeVoxelCount()));

				openvdb::Coord coord;
				openvdb::CoordBBox activeIndexBBox;
				GetFirstInactiveVoxelFromActive<GridType>(GridPtr, coord, activeIndexBBox); //TODO: Properly handle when no such voxels are found in the entire grid
				activeWorldBBox = GridPtr->transform().indexToWorld(activeIndexBBox);
				startWorldCoord = GridPtr->indexToWorld(coord);
				voxelSize = GridPtr->voxelSize();
				isChanged = false;
			}

			inline void markChanged()
			{
				isChanged = true;
			}

			bool isChanged;
		};

		template<typename GridType>
		void GetFirstInactiveVoxelFromActive(typename GridType::Ptr gridPtr, openvdb::Coord &coord, openvdb::CoordBBox &activeIndexBBox)
		{
			activeIndexBBox = gridPtr->evalActiveVoxelBoundingBox();
			for (auto i = gridPtr->cbeginValueOn(); i; ++i)
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
	}
}