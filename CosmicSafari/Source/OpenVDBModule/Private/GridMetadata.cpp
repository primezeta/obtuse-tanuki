#include "OpenVDBModule.h"
#include "GridMetadata.h"
#pragma warning(push)
#pragma warning(1:4211 4800 4503 4146)

Vdb::Metadata::RegionMetadata::RegionMetadata()
{
	ParentGridName = TEXT("");
	RegionName = TEXT("");
	RegionBBox = openvdb::CoordBBox();
}

Vdb::Metadata::RegionMetadata::RegionMetadata(const FString &parentName, const FString &name, const openvdb::CoordBBox &bbox)
	: ParentGridName(parentName), RegionName(name), RegionBBox(bbox)
{
}

FString Vdb::Metadata::RegionMetadata::GetParentGridName() const
{
	return ParentGridName;
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
	ParentGridName = rhs.ParentGridName;
	RegionName = rhs.RegionName;
	RegionBBox = rhs.RegionBBox;
	return *this;
}

FString Vdb::Metadata::RegionMetadata::ID() const
{
	TArray<FString> metaNameStrs;
	metaNameStrs.Add(ParentGridName);
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
	strs.Add(mValue.GetParentGridName());
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
	return lhs.GetParentGridName() == rhs.GetParentGridName() && lhs.GetRegionName() == rhs.GetRegionName() && lhs.GetRegionBBox() == rhs.GetRegionBBox();
}