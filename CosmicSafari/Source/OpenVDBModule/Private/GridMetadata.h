#pragma once
#include "EngineMinimal.h"
#pragma warning(push)
#pragma warning(1:4211 4800 4503 4146)
#include <openvdb/openvdb.h>

namespace Vdb
{
	namespace Metadata
	{
		class RegionMetadata
		{
		public:
			RegionMetadata();
			RegionMetadata(const FString &worldName, const FString &regionName, const openvdb::CoordBBox &bbox);
			FString GetWorldName() const;
			FString GetRegionName() const;
			openvdb::CoordBBox GetRegionBBox() const;
			RegionMetadata& operator=(const RegionMetadata &rhs);
			FString ID() const;
			static FString ConstructRecordStr(const TArray<FString> &strs);
		private:
			FString WorldName;
			FString RegionName;
			openvdb::CoordBBox RegionBBox;
		};
	}
}

template<> OPENVDBMODULE_API inline std::string openvdb::TypedMetadata<Vdb::Metadata::RegionMetadata>::str() const;
template<> OPENVDBMODULE_API inline Vdb::Metadata::RegionMetadata openvdb::zeroVal<Vdb::Metadata::RegionMetadata>();
template<> OPENVDBMODULE_API inline std::string openvdb::TypedMetadata<openvdb::math::ScaleMap>::str() const;
template<> OPENVDBMODULE_API inline openvdb::math::ScaleMap openvdb::zeroVal<openvdb::math::ScaleMap>();
template<> OPENVDBMODULE_API inline openvdb::CoordBBox openvdb::zeroVal<openvdb::math::CoordBBox>();

namespace openvdb
{
	OPENVDB_USE_VERSION_NAMESPACE
	namespace OPENVDB_VERSION_NAME
	{
		namespace math
		{
			OPENVDBMODULE_API bool operator==(const Vdb::Metadata::RegionMetadata &lhs, const Vdb::Metadata::RegionMetadata &rhs);
		}
	}
}