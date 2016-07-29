#pragma once
#include "VDBHandlePrivate.h"

//class FVoxelGrid;
//class FVoxelMetaData;
//class FVoxelTreeData;
//class FVoxelTransform;

/**
* 
*/
template<typename IOStream, typename Serializer>
class OPENVDBMODULE_API FVoxelDatabaseBase : public FArchive, public IOStream, public Serializer
{
public:
	typedef typename IOStream IOStreamType;
	typedef typename Serializer SerializerType;
	
	const std::string Filename;
	const int Mode;
	const bool DelayLoadEnabled;

	FVoxelDatabaseBase()
		: Filename(""), Mode(0), DelayLoadEnabled(false), SerializerType(this)
	{
	}
	
	FVoxelDatabaseBase(const std::FString &filename, int mode)
		: Filename(TCHAR_TO_UTF8(*filename)), Mode(mode), DelayLoadEnabled(false), IOStreamType(filename, mode), SerializerType(this)
	{
		check(!fail());
		check(is_open());
	}

	FVoxelDatabaseBase(std::streambuf * buffer)
		: IOStreamType(buffer), Filename(""), Mode(0), DelayLoadEnabled(false), SerializerType(this)
	{
	}

	FVoxelDatabaseBase(std::istream &stream, bool delayLoad) //Delay load is only for use with an input stream
		: IOStreamType(stream), Filename(""), Mode(0), DelayLoadEnabled(delayLoad), SerializerType(this, delayLoad)
	{
	}

	FVoxelDatabaseBase(std::istream &stream, const std::FString &filename, bool delayLoad) //Delay load is only for use with an input stream
		: IOStreamType(stream), Filename(TCHAR_TO_UTF8(*filename)), Mode(0), DelayLoadEnabled(delayLoad), SerializerType(this, delayLoad)
	{
	}

	//TODO: Might need to do something with void Serialize(void* Data, int64 Num)
};

class OPENVDBMODULE_API FVoxelDatabaseWriter : public FVoxelDatabaseBase<std::ofstream, openvdb::io::Stream>
{
public:
	FVoxelDatabaseWriter(const FString &filename)
		: FVoxelDatabaseBase(filename, std::ios_base::out | std::ios_base::binary | std::ios_base::trunc)
	{
	}
};

class OPENVDBMODULE_API FVoxelDatabaseReader : public FVoxelDatabaseBase<std::ifstream, openvdb::io::Stream>
{
public:
	FVoxelDatabaseReader(const FString &filename)
		: FVoxelDatabaseBase(filename, std::ios_base::in | std::ios_base::binary)
	{
	}
};

class OPENVDBMODULE_API FVoxelDatabaseMemoryMappedReader : public FVoxelDatabaseBase<std::istream, openvdb::io::Stream>
{
public:
	FVoxelDatabaseMemoryMappedReader(const FString &filename)
		: FileMap(TCHAR_TO_UTF8(*filename), boost::interprocess::read_only),
		RegionMap(FileMap, boost::interprocess::read_only),
		MappedBuffer(static_cast<const char*>(RegionMap.get_address()), RegionMap.get_size()),
		FVoxelDatabaseBase(MappedBuffer, filename, /*delayLoad=*/true) //Delay load is required for mapped file reading
	{
	}

private:
	boost::interprocess::file_mapping FileMap;
	boost::interprocess::mapped_region RegionMap;
	boost::iostreams::stream_buffer<boost::iostreams::array_source> MappedBuffer;
};