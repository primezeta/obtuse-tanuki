#include "OpenVDBModule.h"
#include "VDBHandlePrivate.h"

template<> inline FVoxelData openvdb::zeroVal<FVoxelData>()
{
	return FVoxelData();
}

std::ostream& operator<<(std::ostream& os, const FVoxelData& voxelData)
{
	os << voxelData.Data << "." << (int32)voxelData.VoxelType;
	return os;
}

FVoxelData operator+(const FVoxelData &lhs, const float &rhs)
{
	return FVoxelData(lhs.Data + rhs, lhs.VoxelType);
}

FVoxelData operator+(const FVoxelData &lhs, const FVoxelData &rhs)
{
	return FVoxelData(lhs.Data + rhs.Data, lhs.VoxelType);
}

FVoxelData operator-(const FVoxelData &lhs, const FVoxelData &rhs)
{
	return FVoxelData(lhs.Data - rhs.Data, lhs.VoxelType);
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
	return FVoxelData(fabs(voxelData.Data), voxelData.VoxelType);
}

FVoxelData operator-(const FVoxelData &voxelData)
{
	return FVoxelData(-voxelData.Data, voxelData.VoxelType);
}