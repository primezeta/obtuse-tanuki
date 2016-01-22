#include "OvdbVolume.h"

template<typename TreeT>
void OvdbVoxelVolume<TreeT>::buildVolume(const openvdb::CoordBBox &bbox, float surfaceValue)
{
	//Step through only voxels that are on
	for (GridTypeValueOnCIter i = volumeGrid->cbeginValueOn(); i; ++i)
	{
		//Skip tile values and values that are not on the surface
		if (!i.isVoxelValue() ||
			!openvdb::math::isApproxEqual(getGridValue(i.getCoord()), surfaceValue))
		{
			continue;
		}

		//Set up the 6 quads
		std::vector<openvdb::Vec4I> quadPrimitives;
		buildQuads(bbox, quadPrimitives);

		for (int j = 0; j < CUBE_FACE_COUNT; ++j)
		{
			//Insert into the quad set to set up the total ordering when we later retrieve the quads with iterators;
			uniqueQuads[j].insert(Quad(volumeVertices, quadPrimitives[j], j));
		}
	}
}

template<typename TreeT>
void OvdbVoxelVolume<TreeT>::buildQuads(const openvdb::CoordBBox &bbox, std::vector<openvdb::Vec4I> &quadPrimitives)
{
	//Make 6 quads, each of width / height 1
	std::vector<openvdb::Coord> primitiveVertices;
	openvdb::CoordBBox prim = bbox.createCube(bbox.min(), 1);
	primitiveVertices.push_back(cube.getStart());
	primitiveVertices.push_back(cube.getStart().offsetBy(1, 0, 0));
	primitiveVertices.push_back(cube.getStart().offsetBy(0, 1, 0));
	primitiveVertices.push_back(cube.getStart().offsetBy(0, 0, 1));
	primitiveVertices.push_back(cube.getEnd().offsetBy(-1, 0, 0));
	primitiveVertices.push_back(cube.getEnd().offsetBy(0, -1, 0));
	primitiveVertices.push_back(cube.getEnd().offsetBy(0, 0, -1));
	primitiveVertices.push_back(cube.getEnd());

	std::vector<openvdb::Int32> primitiveIndices;
	for (std::vector<openvdb::Coord>::const_iterator i = primitiveVertices.cbegin(); i != primitiveVertices.end(); ++i)
	{
		openvdb::Int32 vertexIndex = getVisitedVertexValue(*i);
		if (vertexIndex == UNVISITED_VERTEX_INDEX)
		{
			//This is a new vertex. Save it to the visited vertex grid for use by any other voxels that share it
			volumeVertices.push_back(volumeGrid->indexToWorld(*i));
			vertexIndex = openvdb::Int32(volumeVertices.size() - 1); //TODO: Error check index ranges
			setVisitedVertexValue(*i, vertexIndex);
		}
		primitiveIndices.push_back(vertexIndex);
	}

	//Add the vertex indices in counterclockwise order on each quad face
	quadPrimitives.push_back(openvdb::Vec4I(primitiveIndices[3], primitiveIndices[4], primitiveIndices[7], primitiveIndices[5]));
	quadPrimitives.push_back(openvdb::Vec4I(primitiveIndices[6], primitiveIndices[2], primitiveIndices[0], primitiveIndices[1]));
	quadPrimitives.push_back(openvdb::Vec4I(primitiveIndices[7], primitiveIndices[4], primitiveIndices[2], primitiveIndices[6]));
	quadPrimitives.push_back(openvdb::Vec4I(primitiveIndices[5], primitiveIndices[1], primitiveIndices[0], primitiveIndices[3]));
	quadPrimitives.push_back(openvdb::Vec4I(primitiveIndices[7], primitiveIndices[6], primitiveIndices[1], primitiveIndices[5]));
	quadPrimitives.push_back(openvdb::Vec4I(primitiveIndices[0], primitiveIndices[2], primitiveIndices[4], primitiveIndices[3]));
}

template<typename TreeT>
void OvdbVoxelVolume<TreeT>::doMesh(OvdbMeshMethod method)
{
	//Collect the quads in a linear list and mesh them. TODO: Better way to do this than copying them all?
	std::vector<OvdbQuad> quads;
	for (int i = 0; i < CUBE_FACE_COUNT; ++i)
	{
		for (std::set<OvdbQuad, cmpByQuad>::iterator j = uniqueQuads[i].cbegin(); j != uniqueQuads[i].end(); ++j)
		{
			quads.push_back(*j);
		}
	}

	//If method is naive do nothing special
	if (method != MESHING_NAIVE)
	{
		//Merge adjacent quads in a greedy manner
		//if (method == MESHING_GREEDY)
		//{

		//}
	}

	uint32_t mergedCount = 0;
	uint32_t vertexIndex = 0;
	for (std::vector<OvdbQuad>::iterator i = quads.begin(); i != quads.end(); ++i)
	{
		OvdbQuad &q = *i;
		if (q.quadIsMerged())
		{
			mergedCount++; //For debugging
			continue;
		}
		//Collect triangle indices of the two triangles comprising this quad
		polygonIndices.push_back(q(V0)); //Triangle 1
		polygonIndices.push_back(q(V1));
		polygonIndices.push_back(q(V3));
		polygonIndices.push_back(q(V0)); //Triangle 2
		polygonIndices.push_back(q(V2));
		polygonIndices.push_back(q(V3));
	}
}

//void greedyMeshQuads(QuadVec &sortedQuads)
//{
//	for (auto i = sortedQuads.begin(); i != sortedQuads.end(); ++i)
//	{
//		//Skip a quad that's been merged
//		if (i->isMerged)
//		{
//			continue;
//		}
//
//		//We start with the lowest quad (since it's known they are sorted)
//		//and check each successive quad until no more can be merged.
//		//Then start again with the next un-merged quad.
//		auto j = i;
//		j++;
//		for (; j != sortedQuads.end(); ++j)
//		{
//			//Only attempt to merge an unmerged quad that is on the same vertical level
//			if (j->isMerged || //TODO: Figure out if 'isMerged' check here is necessary. Since we're greedy meshing, it may not ever matter to check here
//				openvdb::math::isApproxEqual(j->localPos(0).z(), i->localPos(0).z()))
//			{
//				continue;
//			}
//
//			//Check if we can merge by width
//			if (openvdb::math::isApproxEqual(j->localPos(0).y(), i->localPos(0).y()) && //Same location y-axis...
//				openvdb::math::isApproxEqual(j->localPos(0).x() - i->localPos(0).x(), i->width) && //Adjacent...(note: due to the sorted ordering, don't have to use abs)
//				openvdb::math::isApproxEqual(j->height, i->height)) //Same height
//			{
//				i->width += j->width;
//				(*i)[2] = (*j)[2];
//				(*i)[3] = (*j)[3];
//			}
//			//Check if we can merge by height
//			else if (openvdb::math::isApproxEqual(j->localPos(0).x(), i->localPos(0).x()) && //Same location x-axis...
//				openvdb::math::isApproxEqual(j->localPos(0).y() - i->localPos(0).y(), i->height) && //Adjacent...(note: due to the sorted ordering, don't have to use abs)
//				openvdb::math::isApproxEqual(j->width, i->width)) //Same width
//			{
//				i->height += j->height;
//				(*i)[1] = (*j)[1];
//				(*i)[3] = (*j)[3];
//			}
//			else
//			{
//				//Done with merging since we could no longer merge in either direction
//				break;
//			}
//			j->isMerged = true; //Mark this quad as merged so that it won't be meshed
//		}
//	}
//}