#include "OpenVDBModule.h"
#include "GridMetadata.h"

Vdb::Metadata::RegionMetadata::RegionMetadata()
{
	WorldName = TEXT("");
	RegionName = TEXT("");
	RegionBBox = openvdb::CoordBBox();
}

Vdb::Metadata::RegionMetadata::RegionMetadata(const FString &worldName, const FString &regionName, const openvdb::CoordBBox &bbox)
	: WorldName(worldName), RegionName(regionName), RegionBBox(bbox)
{
}

FString Vdb::Metadata::RegionMetadata::GetWorldName() const
{
	return WorldName;
}

FString Vdb::Metadata::RegionMetadata::GetRegionName() const
{
	return RegionName;
}

openvdb::CoordBBox Vdb::Metadata::RegionMetadata::GetRegionBBox() const
{
	return RegionBBox;
}

Vdb::Metadata::RegionMetadata& Vdb::Metadata::RegionMetadata::operator=(const Vdb::Metadata::RegionMetadata &rhs)
{
	WorldName = rhs.WorldName;
	RegionName = rhs.RegionName;
	RegionBBox = rhs.RegionBBox;
	return *this;
}

FString Vdb::Metadata::RegionMetadata::ID() const
{
	TArray<FString> metaNameStrs;
	metaNameStrs.Add(WorldName);
	metaNameStrs.Add(TEXT("region"));
	metaNameStrs.Add(RegionName);
	return ConstructRecordStr(metaNameStrs);
}

FString Vdb::Metadata::RegionMetadata::ConstructRecordStr(const TArray<FString> &strs)
{
	//\x1B is the escape sequence char and \x1e is the record seperator char
	FString recordStr = TEXT("");
	for (auto i = strs.CreateConstIterator(); i; ++i)
	{
		i->Replace(TEXT("\x1B"), TEXT("\x1B\x1B")); //Escape all escape chars
		i->Replace(TEXT("\x1e"), TEXT("\x1B\x1e")); //Escape all record seperator chars
		recordStr += *i + TEXT("\x1e"); //Insert a record seperator after the record
	}
	return recordStr;
}

template<> inline std::string openvdb::TypedMetadata<Vdb::Metadata::RegionMetadata>::str() const
{
	TArray<FString> strs;
	strs.Add(mValue.GetWorldName());
	strs.Add(mValue.GetRegionName());
	openvdb::CoordBBox regionBBox = mValue.GetRegionBBox();
	strs.Add(FString::Printf(TEXT("%s"), UTF8_TO_TCHAR(std::string(regionBBox.min().asVec3d().str() + regionBBox.max().asVec3d().str()).c_str())));
	FString metaStr = Vdb::Metadata::RegionMetadata::ConstructRecordStr(strs);
	return std::string(TCHAR_TO_UTF8(*metaStr));
}

template<> inline Vdb::Metadata::RegionMetadata openvdb::zeroVal<Vdb::Metadata::RegionMetadata>()
{
	return Vdb::Metadata::RegionMetadata();
}

bool openvdb::math::operator==(const Vdb::Metadata::RegionMetadata &lhs, const Vdb::Metadata::RegionMetadata &rhs)
{
	return lhs.GetWorldName() == rhs.GetWorldName() && lhs.GetRegionName() == rhs.GetRegionName() && lhs.GetRegionBBox() == rhs.GetRegionBBox();
}

template<> inline std::string openvdb::TypedMetadata<openvdb::math::ScaleMap>::str() const
{
	return mValue.str();
}

template<> inline openvdb::math::ScaleMap openvdb::zeroVal<openvdb::math::ScaleMap>()
{
	return openvdb::math::ScaleMap(openvdb::Vec3d(0.0));
}

template<> inline openvdb::CoordBBox openvdb::zeroVal<openvdb::math::CoordBBox>()
{
	return openvdb::CoordBBox(openvdb::Coord(), openvdb::Coord());
}