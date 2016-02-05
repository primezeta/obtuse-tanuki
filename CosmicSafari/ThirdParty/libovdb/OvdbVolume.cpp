//#include "libovdb.h"

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