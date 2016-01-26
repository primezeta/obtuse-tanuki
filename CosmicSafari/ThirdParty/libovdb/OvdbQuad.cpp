#include "OvdbQuad.h"

QuadVertexType OvdbQuad::posUVW(QuadVertexIndex v) const
{
	const QuadVertexType &pos = vertices[indices[v]];
	if (quadFace() == XY_FACE) { return pos; }
	else if (quadFace() == XZ_FACE) { return QuadVertexType(pos.x(), pos.z(), pos.y()); }
	else if (quadFace() == YZ_FACE) { return QuadVertexType(pos.y(), pos.z(), pos.x()); }
	else { return QuadVertexType(); } //TODO: Throw an error
}

QuadVertexType OvdbQuad::vertexNormal(QuadVertexIndex v) const
{
	const QuadVertexType &pos = posUVW(v);
	if (quadFace() == XY_FACE) { return QuadVertexType(pos.x(), pos.y(), pos.z()*normal); }
	else if (quadFace() == XZ_FACE) { return QuadVertexType(pos.x(), pos.z(), pos.y()*normal); }
	else if (quadFace() == YZ_FACE) { return QuadVertexType(pos.y(), pos.z(), pos.x()*normal); }
	else { return QuadVertexType(); } //TODO: Throw an error
}

void OvdbQuad::setIndices(const QuadIndicesType &newIndices) //TODO: Error check index range
{
	//Quad vertices are in counterclockwise order
	indices = newIndices;
	QuadVertexType start = posUVW(V0);
	QuadVertexType end = posUVW(V1);
	double quadSizeU = QuadVertexType(start - end).length();
	end = posUVW(V3);
	double quadSizeV = QuadVertexType(start - end).length();
	double quadSizeW = start.z();
	quadSize = QuadVertexType(quadSizeU, quadSizeV, quadSizeW);
}

bool OvdbQuad::mergeQuadsByLength(OvdbQuad &rhs)
{
	//Can only merge adjacent faces
	if (!rhs.quadIsMerged() && isQuadAdjacentByLength(rhs))
	{
		rhs.setIsMerged();
		setIndices(QuadIndicesType(indices[V0], rhs(V1), rhs(V2), indices[V3]));
		return true; //Successfully merged quads
	}
	return false; //Couldn't merge. TODO: Throw an error if other quad is already merged
}

bool OvdbQuad::mergeQuadsByWidth(OvdbQuad &rhs)
{
	//Can only merge adjacent faces
	if (!rhs.quadIsMerged() && isQuadAdjacentByWidth(rhs))
	{
		rhs.setIsMerged();
		setIndices(QuadIndicesType(indices[V0], indices[V1], rhs(V2), rhs(V3)));
		return true; //Successfully merged quads
	}
	return false; //Couldn't merge. TODO: Throw an error if other quad is already merged
}

bool OvdbQuad::isQuadAdjacentByLength(const OvdbQuad &rhs)
{
	//Adjacent in the V direction and of equal height?
	if (quadFace() == rhs.quadFace() &&
		openvdb::math::isApproxEqual(posUVW(V0).y(), rhs.posUVW(V0).y()))
	{
		return openvdb::math::isApproxEqual(quadSize.z(), openvdb::math::Abs(posUVW(V0).y() - rhs.posUVW(V0).y()));
	}
	return false;
}

bool OvdbQuad::isQuadAdjacentByWidth(const OvdbQuad &rhs)
{
	//Adjacent in the U direction and of equal width?
	if (quadFace() == rhs.quadFace() &&
		openvdb::math::isApproxEqual(posUVW(V0).x(), rhs.posUVW(V0).x()))
	{
		return openvdb::math::isApproxEqual(quadSize.z(), openvdb::math::Abs(posUVW(V0).x() - rhs.posUVW(V0).x()));
	}
	return false;
}