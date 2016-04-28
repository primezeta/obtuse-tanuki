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
		struct PrimitiveCube
		{
			PrimitiveCube(const openvdb::Coord &cubeStart)
			{
				const openvdb::CoordBBox bbox = openvdb::CoordBBox::createCube(cubeStart, 1);
				primitiveVertices.push_back(bbox.getStart());
				primitiveVertices.push_back(bbox.getStart().offsetBy(1, 0, 0));
				primitiveVertices.push_back(bbox.getStart().offsetBy(0, 1, 0));
				primitiveVertices.push_back(bbox.getStart().offsetBy(0, 0, 1));
				primitiveVertices.push_back(bbox.getEnd().offsetBy(-1, 0, 0));
				primitiveVertices.push_back(bbox.getEnd().offsetBy(0, -1, 0));
				primitiveVertices.push_back(bbox.getEnd().offsetBy(0, 0, -1));
				primitiveVertices.push_back(bbox.getEnd());
			}

			IndexType& operator[](const int32 &v) { return primitiveIndices[v]; }
			const openvdb::Coord& operator()(const int32 &v) const { return primitiveVertices[v]; }
			//Add the vertex indices in counterclockwise order on each quad face
			openvdb::Vec4i getQuadXY0() const { return openvdb::Vec4i(primitiveIndices[3], primitiveIndices[4], primitiveIndices[7], primitiveIndices[5]); }
			openvdb::Vec4i getQuadXY1() const { return openvdb::Vec4i(primitiveIndices[6], primitiveIndices[2], primitiveIndices[0], primitiveIndices[1]); }
			openvdb::Vec4i getQuadXZ0() const { return openvdb::Vec4i(primitiveIndices[7], primitiveIndices[4], primitiveIndices[2], primitiveIndices[6]); }
			openvdb::Vec4i getQuadXZ1() const { return openvdb::Vec4i(primitiveIndices[5], primitiveIndices[1], primitiveIndices[0], primitiveIndices[3]); }
			openvdb::Vec4i getQuadYZ0() const { return openvdb::Vec4i(primitiveIndices[7], primitiveIndices[6], primitiveIndices[1], primitiveIndices[5]); }
			openvdb::Vec4i getQuadYZ1() const { return openvdb::Vec4i(primitiveIndices[0], primitiveIndices[2], primitiveIndices[4], primitiveIndices[3]); }
			
			std::vector<openvdb::Coord> primitiveVertices;
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
				 typename InTreeType,
			     typename OutTreeType,
			     typename SelfOpType,
				 bool ModifyTiles = true,
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
			typedef typename openvdb::tree::ValueAccessor<SourceTreeType, OutIsSafe, OutCacheLevels, InMutexType> SourceAccessorType;
			typedef typename openvdb::tree::ValueAccessor<const SourceTreeType, OutIsSafe, OutCacheLevels, InMutexType> SourceCAccessorType;
			typedef typename openvdb::tree::ValueAccessor<DestTreeType, OutIsSafe, OutCacheLevels, OutMutexType> DestAccessorType;
			typedef typename openvdb::tree::ValueAccessor<const DestTreeType, OutIsSafe, OutCacheLevels, OutMutexType> DestCAccessorType;
			typedef typename SourceTreeType::ValueType SourceValueType;
			typedef typename DestTreeType::ValueType DestValueType;
			typedef typename TSharedPtr<SelfOpType> Ptr;

			ITransformOp(const SourceGridTypePtr sourceGridPtr)
				: SourceGridPtr(sourceGridPtr)
			{
				check(SourceGridPtr != nullptr);
			}

			static void doTransformValues(const SourceIterType &beginIter, DestGridType &outGrid, SelfOpType &op, bool threaded = true)
			{
				//openvdb transformValues requires that ops are copyable even if shared = true, so instead call only the shared op applier here (code adapted from openvdb transformValues() in ValueTransformer.h)
				typedef openvdb::TreeAdapter<DestGridType> Adapter;
				typedef typename Adapter::TreeType OutTreeType;
				typedef typename openvdb::tools::valxform::SharedOpTransformer<SourceIterType, OutTreeType, SelfOpType, DestAccessorType> Processor;
				Processor proc(beginIter, Adapter::tree(outGrid), op, openvdb::MERGE_ACTIVE_STATES);
				proc.process(threaded);
			}

			inline void operator()(const SourceIterType& iter, DestAccessorType& acc)
			{
				if (iter.isVoxelValue())
				{
					//Do the transformation operation for a single voxel according to the source iter
					DoVoxelTransform(iter, acc);
				}
				else if(ModifyTiles == true)
				{
					//Do the transformation operation for the destination according to the source tile bounding box
					openvdb::CoordBBox bbox;
					iter.getBoundingBox(bbox);
					DoTileTransform(iter, acc, bbox);
				}
				else
				{
					//Not modifying tiles so set the destination tile off according to the source coord
					acc.setValueOff(iter.getCoord());
				}
			}

		protected:
			FCriticalSection CriticalSection;
			const SourceGridTypePtr SourceGridPtr;

			//In override call ModifyValue[AndActiveState]
			virtual inline void DoVoxelTransform(const SourceIterType& iter, DestAccessorType& acc) = 0;
			//In override call ModifyValue[AndActiveState]
			virtual inline void DoTileTransform(const SourceIterType& iter, DestAccessorType& acc, const openvdb::CoordBBox &tileBBox) = 0;			
			//Override the active/inactive logic used by ModifyValue[AndActiveState]
			virtual inline bool GetIsActive(const SourceIterType& iter, DestAccessorType& acc, const openvdb::Coord &coord, bool &outIsActive) const = 0;
			//Override the value production used by ModifyValue[AndActiveState]
			virtual inline bool GetValue(const SourceIterType& iter, DestAccessorType& acc, const openvdb::Coord &coord, DestValueType &outValue) const = 0;

			//Modify the value and active state and return both value and active state
			inline void ModifyValueAndActiveState(const SourceIterType& iter, DestAccessorType& acc, const openvdb::Coord &coord, DestValueType &outValue, bool &isActive)
			{
				//If the value or active state changed then modify both otherwise just return the value and active state without calling the modify op
				FScopeLock lock(&CriticalSection);
				bool isValueChanged = GetValue(iter, acc, coord, outValue);
				bool isStateChanged = GetIsActive(iter, acc, coord, isActive);
				if (isValueChanged || isStateChanged)
				{
					//According to openvdb ValueTransformer.h modify-in-place operations:
					//"are typically significantly faster than calling getValue() followed by setValue()."
					acc.modifyValueAndActiveState<BasicModifyActiveOp<DestValueType>>(coord, BasicModifyActiveOp<DestValueType>(outValue, isActive));
				}
			}

			inline bool ModifyValuesRange(const SourceIterType& iter, DestAccessorType& acc, std::vector<openvdb::Coord>::iterator begin, std::vector<openvdb::Coord>::iterator end, std::vector<DestValueType> &outValues)
			{
				FScopeLock lock(&CriticalSection);
				bool isAnyValueChanged = false;
				for (std::vector<openvdb::Coord>::iterator i = begin; i != end; ++i)
				{
					const openvdb::Coord &coord = *i;
					DestValueType outValue;
					bool isValueChanged = GetValue(iter, acc, coord, outValue);
					outValues.push_back(outValue);
					if (isValueChanged)
					{
						//According to openvdb ValueTransformer.h modify-in-place operations:
						//"are typically significantly faster than calling getValue() followed by setValue()."
						acc.modifyValue<BasicModifyOp<DestValueType>>(coord, BasicModifyOp<DestValueType>(outValue));
						isAnyValueChanged = true;
					}
				}
				return isAnyValueChanged;
			}

			inline bool ModifyValuesRange(const SourceIterType& iter, DestAccessorType& acc, std::vector<openvdb::Coord>::iterator begin, std::vector<openvdb::Coord>::iterator end)
			{
				FScopeLock lock(&CriticalSection);
				bool isAnyValueChanged = false;
				for (std::vector<openvdb::Coord>::iterator i = begin; i != end; ++i)
				{
					const openvdb::Coord &coord = *i;
					DestValueType outValue;
					bool isValueChanged = GetValue(iter, acc, coord, outValue);
					if (isValueChanged)
					{
						//According to openvdb ValueTransformer.h modify-in-place operations:
						//"are typically significantly faster than calling getValue() followed by setValue()."
						acc.modifyValue<BasicModifyOp<DestValueType>>(coord, BasicModifyOp<DestValueType>(outValue));
						isAnyValueChanged = true;
					}
				}
				return isAnyValueChanged;
			}

			inline bool ModifyValuesAndActiveStateRange(const SourceIterType& iter, DestAccessorType& acc, std::vector<openvdb::Coord>::iterator begin, std::vector<openvdb::Coord>::iterator end, std::vector<DestValueType> &outValues, std::vector<bool> &outActiveStates)
			{
				FScopeLock lock(&CriticalSection);
				bool isAnyValueChanged = false;
				for (std::vector<openvdb::Coord>::iterator i = begin; i != end; ++i)
				{
					const openvdb::Coord &coord = *i;
					DestValueType outValue;
					bool isActive;
					bool isValueChanged = GetValue(iter, acc, coord, outValue);
					bool isStateChanged = GetIsActive(iter, acc, coord, isActive);
					outValues.push_back(outValue);
					outActiveStates.push_back(isActive);
					if (isValueChanged || isStateChanged)
					{
						//According to openvdb ValueTransformer.h modify-in-place operations:
						//"are typically significantly faster than calling getValue() followed by setValue()."
						acc.modifyValueAndActiveState<BasicModifyActiveOp<DestValueType>>(coord, BasicModifyActiveOp<DestValueType>(outValue, isActive));
						isAnyValueChanged = true;
					}
				}
				return isAnyValueChanged;
			}

			inline bool ModifyValuesAndActiveStateRange(const SourceIterType& iter, DestAccessorType& acc, std::vector<openvdb::Coord>::iterator begin, std::vector<openvdb::Coord>::iterator end)
			{
				FScopeLock lock(&CriticalSection);
				bool isAnyValueChanged = false;
				for (std::vector<openvdb::Coord>::iterator i = begin; i != end; ++i)
				{
					const openvdb::Coord &coord = *i;
					DestValueType outValue;
					bool isActive;
					bool isValueChanged = GetValue(iter, acc, coord, outValue);
					bool isStateChanged = GetIsActive(iter, acc, coord, isActive);
					if (isValueChanged || isStateChanged)
					{
						//According to openvdb ValueTransformer.h modify-in-place operations:
						//"are typically significantly faster than calling getValue() followed by setValue()."
						acc.modifyValueAndActiveState<BasicModifyActiveOp<DestValueType>>(coord, BasicModifyActiveOp<DestValueType>(outValue, isActive));
						isAnyValueChanged = true;
					}
				}
				return isAnyValueChanged;
			}

			//Modify the value and return the value
			inline void ModifyValue(const SourceIterType& iter, DestAccessorType& acc, const openvdb::Coord &coord, DestValueType &outValue)
			{
				//If the value changed then modify the value otherwise just return the existing value without calling the modify op
				FScopeLock lock(&CriticalSection);
				bool isValueChanged = GetValue(iter, acc, coord, outValue);
				if (isValueChanged)
				{
					//According to openvdb ValueTransformer.h modify-in-place operations:
					//"are typically significantly faster than calling getValue() followed by setValue()."
					acc.modifyValue<BasicModifyOp<DestValueType>>(coord, BasicModifyOp<DestValueType>(outValue));
				}
			}

			//Modify active state and return the active state value
			inline void ModifyActiveState(const SourceIterType& iter, DestAccessorType& acc, const openvdb::Coord &coord, bool &isActive)
			{
				//If state changed then modify the active state otherwise just return the existing value without calling the modify op
				FScopeLock lock(&CriticalSection);
				bool isStateChanged = GetIsActive(iter, acc, coord, isActive);
				if (isStateChanged)
				{
					//There is no in-place modify function for just active state so use the iter
					acc.setActiveState(coord, isActive);
				}
			}

			//Modify value and active state without returning value and active state
			inline void ModifyValueAndActiveState(const SourceIterType& iter, DestAccessorType& acc, const openvdb::Coord &coord)
			{
				DestValueType outValue;
				bool isActive;
				ModifyValueAndActiveState(iter, acc, coord, outValue, isActive);
			}
			
			//Modify value and active state and return only the value
			inline void ModifyValueAndActiveState(const SourceIterType& iter, DestAccessorType& acc, const openvdb::Coord &coord, DestValueType &outValue)
			{
				bool isActive;
				ModifyValueAndActiveState(iter, acc, coord, outValue, isActive);
			}

			//Modify value and active state and return only the active state
			inline void ModifyValueAndActiveState(const SourceIterType& iter, DestAccessorType& acc, const openvdb::Coord &coord, bool &isActive)
			{
				DestValueType outValue;
				ModifyValueAndActiveState(iter, acc, coord, outValue, isActive);
			}

			//Modify value only
			inline void ModifyValue(const SourceIterType& iter, DestAccessorType& acc, const openvdb::Coord &coord)
			{
				DestValueType outValue;
				ModifyValue(iter, acc, coord, outValue);
			}

			//Modify active state only
			inline void ModifyActiveState(const SourceIterType& iter, DestAccessorType& acc, const openvdb::Coord &coord)
			{
				bool isActive;
				ModifyActiveState(iter, acc, coord, isActive);
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
			typedef typename DestValueType::DataType DataType;

			PerlinNoiseFillOp(const SourceGridTypePtr sourceGridPtr, const DataType &isovalue, int32 seed, float frequency, float lacunarity, float persistence, int32 octaveCount)
				: ITransformOp(sourceGridPtr), surfaceValue(isovalue)
			{
				valueSource.SetSeed(seed);
				valueSource.SetFrequency((double)frequency);
				valueSource.SetLacunarity((double)lacunarity);
				valueSource.SetPersistence((double)persistence);
				valueSource.SetOctaveCount(octaveCount);
			}

			virtual inline void DoVoxelTransform(const SourceIterType& iter, DestAccessorType& acc) override
			{
				//First set the density value of this voxel and all surrounding voxels since active state depends on surrounding values
				const openvdb::Coord coord = iter.getCoord();
				PrimitiveCube surroundingCoords(coord);

				//Change the value of all vertices atomically
				ModifyValuesRange(iter, acc, surroundingCoords.primitiveVertices.begin(), surroundingCoords.primitiveVertices.end());

				//Change active state of only this voxel
				ModifyActiveState(iter, acc, coord);
			}

			virtual inline void DoTileTransform(const SourceIterType& iter, DestAccessorType& acc, const openvdb::CoordBBox &tileBBox) override
			{
				//Do nothing for tiles
			}

			virtual inline bool GetIsActive(const SourceIterType& iter, DestAccessorType& acc, const openvdb::Coord &coord, bool &outIsActive) const override
			{
				//A voxel will only ever be visited once here, (i.e. coord will always be the same as iter.getCoord()) therefore always assume the active state was changed
				const static bool isChanged = true;

				//A voxel is active if it is on the surface:
				//i.e. use the values of each vertex of the cube to check if the voxel is partially inside and partially outside the surface
				PrimitiveCube surroundingCoords(coord);
				uint8 bitValue = 1;
				uint8 insideBits = 0;
				for (int32 i = 0; i < CUBE_VERTEX_COUNT; ++i)
				{
					if (acc.getValue(surroundingCoords(i)).Data < surfaceValue) //Convention of positive => above surface
					{
						insideBits |= bitValue;
						bitValue = bitValue << 1;
					}
				}
				//If all vertices are inside the surface or all are outside the surface then set off in order to not mesh this voxel
				outIsActive = insideBits > 0 && insideBits < 255;
				return isChanged;
			}

			virtual inline bool GetValue(const SourceIterType& iter, DestAccessorType& acc, const openvdb::Coord &coord, DestValueType &outValue) const override
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
				if (iter.getTree()->getValue(coord) == true)
				{
					outValue.Data = (DataType)acc.getValue(coord).Data;
				}
				else
				{
					isChanged = true;
					iter.getTree()->setValueOnly(coord, true);
					const openvdb::Vec3d vec = SourceGridPtr->transform().indexToWorld(coord);
					outValue.Data = (DataType)(valueSource.GetValue(vec.x(), vec.y(), vec.z()) + vec.z());
				}
				return isChanged;
			}
		private:
			noise::module::Perlin valueSource;
			const DataType surfaceValue;
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
			MeshGeometryOp(const SourceGridTypePtr sourceGridPtr, TArray<FVector> &vertexBuffer, TArray<int32> &polygonBuffer, TArray<FVector> &normalBuffer)
				: ITransformOp(sourceGridPtr), vertices(vertexBuffer), polygons(polygonBuffer), normals(normalBuffer)
			{
			}

			virtual inline void DoVoxelTransform(const SourceIterType& iter, DestAccessorType& acc) override
			{
				//Mesh the voxel as a simple cube with 6 equal sized quads
				PrimitiveCube primitiveIndices(iter.getCoord());
				
				//Change the value of all vertex indices atomically
				std::vector<DestValueType> outValues;
				std::vector<bool> outActiveStates;
				ModifyValuesAndActiveStateRange(iter, acc, primitiveIndices.primitiveVertices.begin(), primitiveIndices.primitiveVertices.end(), outValues, outActiveStates);

				//Set the active state of each and build the quad from vertex indices
				int32 v = 0;
				for (std::vector<DestValueType>::iterator i = outValues.begin(); i != outValues.end(); ++i)
				{
					primitiveIndices[v] = *i;
				}
				quads.Enqueue(primitiveIndices.getQuadXY0());
				quads.Enqueue(primitiveIndices.getQuadXY1());
				quads.Enqueue(primitiveIndices.getQuadXZ0());
				quads.Enqueue(primitiveIndices.getQuadXZ1());
				quads.Enqueue(primitiveIndices.getQuadYZ0());
				quads.Enqueue(primitiveIndices.getQuadYZ1());
			}
			
			virtual inline void DoTileTransform(const SourceIterType& iter, DestAccessorType& acc, const openvdb::CoordBBox &tileBBox) override
			{
				//Do nothing for tiles
			}

			virtual inline bool GetIsActive(const SourceIterType& iter, DestAccessorType& acc, const openvdb::Coord &coord, bool &outIsActive) const override
			{
				//A voxel will only ever be visited once here, (i.e. coord will always be the same as iter.getCoord()) therefore always assume the active state was changed
				const static DestValueType backgroundValue = acc.tree().background(); //The background value does not change during the course of this operation
				bool isChanged = acc.isValueOn(coord) == true;
				outIsActive = acc.getValue(coord) != backgroundValue;
				return isChanged;
			}

			virtual inline bool GetValue(const SourceIterType& iter, DestAccessorType& acc, const openvdb::Coord &coord, DestValueType &outValue) const override
			{
				bool isChanged = false;
				//The vertex value may have already been calculated by someone else since voxels share vertices so first check if the vertex exists
				if (acc.isValueOn(coord) == true)
				{
					//Get the saved vertex index
					outValue = acc.getValue(coord);
				}
				else
				{
					//Add to the vertex array and save the index of the new vertex
					isChanged = true;
					const openvdb::Vec3d vtx = SourceGridPtr->transform().indexToWorld(coord);
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
		};

		//Helper struct to hold the associated grid meshing info
		template <typename TreeType, typename VertexIndexTreeType>
		class BasicMesher :
			public MeshGeometryOp<TreeType, VertexIndexTreeType>
		{
		public:
			BasicMesher(const SourceGridTypePtr sourceGridPtr, TArray<FVector> &vertexBuffer, TArray<int32> &polygonBuffer, TArray<FVector> &normalBuffer)
				: MeshGeometryOp(sourceGridPtr, vertexBuffer, polygonBuffer, normalBuffer),
				  isDirty(true)
			{
			}

			inline void doMeshOp()
			{
				if (isDirty)
				{
					VisitedVertexIndices = DestGridType::create(UNVISITED_VERTEX_INDEX);
					vertices.Empty();
					polygons.Empty();
					normals.Empty();
					isDirty = false;
					BasicMesher<TreeType, VertexIndexTreeType>::doTransformValues(SourceGridPtr->cbeginValueOn(), *VisitedVertexIndices, *this);
					collectPolygons();
				}
			}

		protected:
			DestGridTypePtr VisitedVertexIndices;
			bool isDirty;
		};
	}
}