#include "OpenVDBModule.h"
#include "VDBHandlePrivate.h"

template<> inline FVoxelData openvdb::zeroVal<FVoxelData>()
{
	return FVoxelData();
}

std::ostream& operator<<(std::ostream& os, const FVoxelData& voxelData)
{
	os << voxelData.Data << "." << voxelData.MaterialID;
	return os;
}

FVoxelData operator+(const FVoxelData &lhs, const float &rhs)
{
	return FVoxelData(lhs.Data + rhs, lhs.MaterialID);
}

FVoxelData operator+(const FVoxelData &lhs, const FVoxelData &rhs)
{
	return FVoxelData(lhs.Data + rhs.Data, lhs.MaterialID);
}

FVoxelData operator-(const FVoxelData &lhs, const FVoxelData &rhs)
{
	return FVoxelData(lhs.Data - rhs.Data, lhs.MaterialID);
}

bool operator<(const FVoxelData &lhs, const FVoxelData &rhs)
{
	return lhs.Data < rhs.Data;
}

bool operator>(const FVoxelData &lhs, const FVoxelData &rhs)
{
	return lhs.Data > rhs.Data;
}

bool operator==(const FVoxelData &lhs, const FVoxelData &rhs)
{
	return openvdb::math::isApproxEqual(lhs.Data, rhs.Data);
}

inline FVoxelData Abs(const FVoxelData &voxelData)
{
	return FVoxelData(fabs(voxelData.Data), voxelData.MaterialID);
}

FVoxelData operator-(const FVoxelData &voxelData)
{
	return FVoxelData(-voxelData.Data, voxelData.MaterialID);
}