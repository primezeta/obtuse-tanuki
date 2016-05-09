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
				 typename InTreeType,
			     typename OutTreeType,
			     typename SelfOpType,
				 bool ModifyTiles = true,
				 bool Threaded = true,
				 bool InIsSafe = true,
				 openvdb::Index InCacheLevels = InTreeType::DEPTH - 1,
				 typename InMutexType = tbb::null_mutex,
  				 bool OutIsSafe = true,
				 openvdb::Index OutCacheLevels = OutTreeType::DEPTH - 1,
				 typename OutMutexType = tbb::null_mutex>
		class ITransformOp
		{
		public:
			typedef typename InIterType SourceIterType;
			typedef typename InTreeType SourceTreeType;
			typedef typename OutTreeType DestTreeType;
			typedef typename openvdb::Grid<SourceTreeType> SourceGridType;
			typedef typename openvdb::Grid<DestTreeType> DestGridType;
			typedef typename SourceGridType::Ptr SourceGridTypePtr;
			typedef typename openvdb::Grid<DestTreeType>::Ptr DestGridTypePtr;
			typedef typename SourceGridType::ConstPtr SourceGridTypeCPtr;
			typedef typename openvdb::Grid<DestTreeType>::ConstPtr DestGridTypeCPtr;
			typedef typename openvdb::tree::ValueAccessor<SourceTreeType, InIsSafe, InCacheLevels, InMutexType> SourceAccessorType;
			typedef typename openvdb::tree::ValueAccessor<const SourceTreeType, InIsSafe, InCacheLevels, InMutexType> SourceCAccessorType;
			typedef typename openvdb::tree::ValueAccessor<DestTreeType, OutIsSafe, OutCacheLevels, OutMutexType> DestAccessorType;
			typedef typename openvdb::tree::ValueAccessor<const DestTreeType, OutIsSafe, OutCacheLevels, OutMutexType> DestCAccessorType;
			typedef typename SourceTreeType::ValueType SourceValueType;
			typedef typename DestTreeType::ValueType DestValueType;
			typedef typename TSharedPtr<SelfOpType> Ptr;

			ITransformOp(const SourceGridTypePtr sourceGridPtr, const DestGridTypePtr destGridPtr)
				: SourceGridPtr(sourceGridPtr), SourceAcc(sourceGridPtr->tree()), DestGridPtr(destGridPtr)
			{
				if (ModifyTiles == false)
				{
					SourceGridPtr->tree().voxelizeActiveTiles();
					DestGridPtr->tree().voxelizeActiveTiles();
				}
			}

			bool IsThreaded()
			{
				return Threaded;
			}

			static void DoTransformValues(const SourceIterType &beginIter, DestGridType &outGrid, SelfOpType &op)
			{
				//openvdb transformValues requires that ops are copyable even if shared = true, so instead call only the shared op applier here (code adapted from openvdb transformValues() in ValueTransformer.h)
				typedef openvdb::TreeAdapter<DestGridType> Adapter;
				typedef typename Adapter::TreeType OutTreeType;
				typedef typename openvdb::tools::valxform::SharedOpTransformer<SourceIterType, OutTreeType, SelfOpType, DestAccessorType> Processor;
				Processor proc(beginIter, Adapter::tree(outGrid), op, openvdb::MERGE_ACTIVE_STATES);
				UE_LOG(LogOpenVDBModule, Display, TEXT("%s"), *FString::Printf(TEXT("%s (pre transform) %d active voxels"), UTF8_TO_TCHAR(outGrid.getName().c_str()), outGrid.activeVoxelCount()));
				proc.process(op.IsThreaded());
				UE_LOG(LogOpenVDBModule, Display, TEXT("%s"), *FString::Printf(TEXT("%s (post transform) %d active voxels"), UTF8_TO_TCHAR(outGrid.getName().c_str()), outGrid.activeVoxelCount()));
			}

			inline void operator()(const SourceIterType& iter, DestAccessorType& acc)
			{
				if (iter.isVoxelValue())
				{
					//Do the transformation operation for any coord as a single voxel
					TransformCoord(iter, acc);
				}
				else
				{
					//Do the transformation operation for each voxel in the tile
					SourceIterType tileIter = iter;
					while (tileIter.next())
					{
						if (tileIter.isVoxelValue())
						{
							TransformCoord(tileIter, acc);
						}
					}
				}
			}

		protected:
			const SourceGridTypePtr SourceGridPtr;
			const DestGridTypePtr DestGridPtr;
			SourceAccessorType SourceAcc;

			virtual inline void TransformCoord(const SourceIterType& iter, DestAccessorType& acc) = 0;
			//Override the value production used by ModifyValue[AndActiveState]
			virtual inline bool GetValue(DestAccessorType& acc, const openvdb::Coord &coord, DestValueType &outValue) = 0;

			//Modify the value without changing active state and return the value
			inline void ModifyValueOnly(DestAccessorType& acc, const openvdb::Coord &coord, DestValueType &outValue)
			{
				if (Threaded)
				{
					FScopeLock lock(&CriticalSection);
					DoModifyValueOnly(acc, coord, outValue);
				}
				else
				{
					DoModifyValueOnly(acc, coord, outValue);
				}
			}

			//Modify the value, set active, and return the value
			inline void ModifyValue(DestAccessorType& acc, const openvdb::Coord &coord, DestValueType &outValue)
			{
				if (Threaded)
				{
					FScopeLock lock(&CriticalSection);
					DoModifyValue(acc, coord, outValue);
				}
				else
				{
					DoModifyValue(acc, coord, outValue);
				}
			}

		private:
			FCriticalSection CriticalSection;

			//Modify the value and return the value
			inline void DoModifyValueOnly(DestAccessorType& acc, const openvdb::Coord &coord, DestValueType &outValue)
			{
				bool isValueChanged = GetValue(acc, coord, outValue);
				if (isValueChanged)
				{
					acc.setValueOnly(coord, outValue);
				}
			}

			//Modify the value, set active, and return the value
			inline void DoModifyValue(DestAccessorType& acc, const openvdb::Coord &coord, DestValueType &outValue)
			{
				bool isValueChanged = GetValue(acc, coord, outValue);
				if (isValueChanged)
				{
					//According to openvdb ValueTransformer.h modify-in-place operations:
					//"are typically significantly faster than calling getValue() followed by setValue()."
					acc.modifyValue<BasicModifyOp<DestValueType>>(coord, BasicModifyOp<DestValueType>(outValue));
				}
			}
		};

		//Operator to fill a grid with Perlin noise values (no tiles)
		template <typename InTreeType, typename OutTreeType>
		class PerlinNoiseFillOp :
			public ITransformOp<typename InTreeType::ValueOnIter,
								InTreeType,
			                    OutTreeType,
			                    typename PerlinNoiseFillOp<InTreeType, OutTreeType>,
			                    false>
		{
		public:
			PerlinNoiseFillOp(const SourceGridTypePtr sourceGridPtr, const DestGridTypePtr destGridPtr, int32 seed, float frequency, float lacunarity, float persistence, int32 octaveCount)
				: ITransformOp(sourceGridPtr, destGridPtr)
			{
				valueSource.SetSeed(seed);
				valueSource.SetFrequency((double)frequency);
				valueSource.SetLacunarity((double)lacunarity);
				valueSource.SetPersistence((double)persistence);
				valueSource.SetOctaveCount(octaveCount);
				UE_LOG(LogOpenVDBModule, Display, TEXT("%s"), *FString::Printf(TEXT("%s (source pre perlin op) %d active voxels"), UTF8_TO_TCHAR(SourceGridPtr->getName().c_str()), SourceGridPtr->activeVoxelCount()));
				UE_LOG(LogOpenVDBModule, Display, TEXT("%s"), *FString::Printf(TEXT("%s (dest pre perlin op) %d active voxels"), UTF8_TO_TCHAR(DestGridPtr->getName().c_str()), DestGridPtr->activeVoxelCount()));
			}

			virtual inline void TransformCoord(const SourceIterType& iter, DestAccessorType& acc) override
			{
				//Set the density value of this voxel and values of each voxel that correspond to vertices of the cube with origin at this voxel
				const openvdb::Coord &coord = iter.getCoord();
				const DestValueType backgroundValue = DestGridPtr->background();
				const openvdb::CoordBBox bbox = openvdb::CoordBBox::createCube(coord, 2);
				std::vector<DestValueType> outValues;
				openvdb::Coord vertexCoord;
				for (int32 x = bbox.min().x(); x <= bbox.max().x(); ++x)
				{
					vertexCoord.setX(x);
					for (int32 y = bbox.min().y(); y <= bbox.max().y(); ++y)
					{
						vertexCoord.setY(y);
						for (int32 z = bbox.min().z(); z <= bbox.max().z(); ++z)
						{
							vertexCoord.setZ(z);
							//Modify the value without changing the active state
							DestValueType outValue;
							ModifyValueOnly(acc, vertexCoord, outValue);
							outValues.push_back(outValue);
						}
					}
				}
				
				//A voxel is active if it is on the surface:
				//i.e. use the values of each vertex of the cube to check if the voxel is partially inside and partially outside the surface
				uint8 bitValue = 1;
				uint8 insideBits = 0;
				for (std::vector<DestValueType>::const_iterator i = outValues.begin(); i != outValues.end(); ++i)
				{
					if (i->Data < backgroundValue.Data) //Convention of positive => above surface
					{
						insideBits |= bitValue;
						bitValue = bitValue << 1;
					}
				}
				bool isOnSurface = insideBits > 0 && insideBits < 255;
				acc.setActiveState(coord, isOnSurface);
			}

			virtual inline bool GetValue(DestAccessorType& acc, const openvdb::Coord &coord, DestValueType &outValue) override
			{
				//double prevLacunarity = valueSource.GetLacunarity();
				//int32 prevOctaveCount = valueSource.GetOctaveCount();
				//valueSource.SetLacunarity(prevLacunarity*0.004);
				//valueSource.SetOctaveCount(2);
				//double warp = valueSource.GetValue(vec.x(), vec.y(), vec.z()) * 8;
				//valueSource.SetLacunarity(prevLacunarity);
				//valueSource.SetOctaveCount(prevOctaveCount);
				//return (DataType)(warp + valueSource.GetValue(vec.x(), vec.y(), vec.z()) - vec.z());

				bool isChanged = false;
				//If the value was already set during this operation use that value
				if (SourceAcc.getValue(coord) == true)
				{
					outValue.Data = acc.getValue(coord).Data;
				}
				else
				{
					isChanged = true;
					SourceAcc.setValueOnly(coord, true);
					const openvdb::Vec3d vec = DestGridPtr->transform().indexToWorld(coord);
					outValue.Data = (OutTreeType::ValueType::DataType)(valueSource.GetValue(vec.x(), vec.y(), vec.z()) + vec.z());
				}
				return isChanged;
			}
		private:
			noise::module::Perlin valueSource;
		};

		//Operator to generate geometry from active voxels (no tiles)
		template <typename InTreeType, typename OutTreeType>
		class MeshGeometryOp :
			public ITransformOp<typename InTreeType::ValueOnCIter,
			                    InTreeType,
			                    OutTreeType,
			                    typename MeshGeometryOp<InTreeType, OutTreeType>,
			                    false>
		{
		public:
			MeshGeometryOp(const SourceGridTypePtr sourceGridPtr, const DestGridTypePtr destGridPtr, TArray<FVector> &vertexBuffer, TArray<int32> &polygonBuffer, TArray<FVector> &normalBuffer, TArray<FVector2D> &uvBuffer, TArray<FColor> &colorBuffer, TArray<FProcMeshTangent> &tangentBuffer)
				: ITransformOp(sourceGridPtr, destGridPtr), vertices(vertexBuffer), polygons(polygonBuffer), normals(normalBuffer), uvs(uvBuffer), colors(colorBuffer), tangents(tangentBuffer)
			{
			}

			virtual inline void TransformCoord(const SourceIterType& iter, DestAccessorType& acc) override
			{
				//Mesh the voxel as a simple cube with 6 equal sized quads
				const openvdb::Coord &coord = iter.getCoord();
				const openvdb::CoordBBox bbox = openvdb::CoordBBox::createCube(coord, 2);
				std::vector<DestValueType> outValues;
				openvdb::Coord vertex;
				for (int32 x = bbox.min().x(); x <= bbox.max().x(); ++x)
				{
					vertex.setX(x);
					for (int32 y = bbox.min().y(); y <= bbox.max().y(); ++y)
					{
						vertex.setY(y);
						for (int32 z = bbox.min().z(); z <= bbox.max().z(); ++z)
						{
							vertex.setZ(z);
							DestValueType outValue;
							//Modify the value and set to active
							ModifyValue(acc, vertex, outValue);
							outValues.push_back(outValue);
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

			virtual inline bool GetValue(DestAccessorType& acc, const openvdb::Coord &coord, DestValueType &outValue) override
			{
				const DestValueType backgroundValue = DestGridPtr->background();
				bool isChanged = false;
				//The vertex value may have already been calculated by someone else since voxels share vertices so first check if the vertex exists
				if (acc.getValue(coord) != backgroundValue)
				{
					//Get the saved vertex index
					outValue = acc.getValue(coord);
				}
				else
				{
					//Add to the vertex array and save the index of the new vertex
					isChanged = true;
					const openvdb::Vec3d vtx = DestGridPtr->transform().indexToWorld(coord);
					outValue = (DestValueType)(vertices.Add(FVector((float)vtx.x(), (float)vtx.y(), (float)vtx.z())));
				}
				return isChanged;
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

		protected:
			TQueue<openvdb::Vec4i, EQueueMode::Mpsc> quads;
			TArray<FVector> &vertices;
			TArray<int32> &polygons;
			TArray<FVector> &normals;
			TArray<FVector2D> &uvs;
			TArray<FColor> &colors;
			TArray<FProcMeshTangent> &tangents;
		};

		//Helper struct to hold the associated grid meshing info
		template <typename TreeType>
		class BasicMesher :
			public MeshGeometryOp<TreeType, IndexTreeType>
		{
		public:
			BasicMesher(const SourceGridTypePtr sourceGridPtr, TArray<FVector> &vertexBuffer, TArray<int32> &polygonBuffer, TArray<FVector> &normalBuffer, TArray<FVector2D> &uvBuffer, TArray<FColor> &colorBuffer, TArray<FProcMeshTangent> &tangentBuffer)
				: MeshGeometryOp(sourceGridPtr, DestGridType::create(UNVISITED_VERTEX_INDEX), vertexBuffer, polygonBuffer, normalBuffer, uvBuffer, colorBuffer, tangentBuffer)
			{
				DestGridPtr->setName(SourceGridPtr->getName() + ".indices");
				DestGridPtr->setTransform(SourceGridPtr->transformPtr());
				UE_LOG(LogOpenVDBModule, Display, TEXT("%s"), *FString::Printf(TEXT("%s (source pre mesh op) %d active voxels"), UTF8_TO_TCHAR(SourceGridPtr->getName().c_str()), SourceGridPtr->activeVoxelCount()));
				UE_LOG(LogOpenVDBModule, Display, TEXT("%s"), *FString::Printf(TEXT("%s (dest pre mesh op) %d active voxels"), UTF8_TO_TCHAR(DestGridPtr->getName().c_str()), DestGridPtr->activeVoxelCount()));
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

			inline void doMeshOp()
			{
				clearBuffers();
				BasicMesher<TreeType>::DoTransformValues(SourceGridPtr->cbeginValueOn(), *DestGridPtr, *this);
				UE_LOG(LogOpenVDBModule, Display, TEXT("%s"), *FString::Printf(TEXT("%s (source post mesh op) %d active voxels"), UTF8_TO_TCHAR(SourceGridPtr->getName().c_str()), SourceGridPtr->activeVoxelCount()));
				UE_LOG(LogOpenVDBModule, Display, TEXT("%s"), *FString::Printf(TEXT("%s (dest post mesh op) %d active voxels"), UTF8_TO_TCHAR(DestGridPtr->getName().c_str()), DestGridPtr->activeVoxelCount()));
				collectPolygons();
			}
		};
	}
}