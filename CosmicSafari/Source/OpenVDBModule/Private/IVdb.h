#pragma once
#include "EngineMinimal.h"
#include <openvdb/openvdb.h>

template<typename TreeType, typename... MetadataTypes>
class IVdbInterfaceDefinition
{
public:
	typedef openvdb::Grid<TreeType> IVdbGridType;
		
	const FString FilePath;
	const bool DelayLoadIsEnabled;
	const bool GridStatsIsEnabled;

	IVdbInterfaceDefinition(const FString &path, bool enableDelayLoad, bool enableGridStats)
		: FilePath(path), DelayLoadIsEnabled(enableDelayLoad), GridStatsIsEnabled(enableGridStats) {}

	virtual ~IVdbInterfaceDefinition() = 0;

	template<typename... Args>
	inline void pass(Args&&...) {}

	template<typename MetadataType>
	virtual void InitializeMetadata() = 0;

	template<typename... MetadataTypes>
	inline void InitializeMetadataTypes(MetadataTypes&&...)
	{
		pass(InitializeMetadata()...);
	}

	virtual void OpenFileGuard() = 0;
	virtual void CloseFileGuard() = 0;
	virtual TSharedPtr<IVdbGridType> ReadGridMeta(const FString &gridName) = 0;
	virtual TSharedPtr<IVdbGridType> ReadGridTree(const FString &gridName) = 0;
	template<typename FileMetaType> virtual TSharedPtr<FileMetaType> GetFileMetaValue(const FString &metaName, const FileMetaType &metaValue) = 0;
	template<typename FileMetaType> virtual void InsertFileMeta(const FString &metaName, const FileMetaType &metaValue) = 0;
	virtual void RemoveFileMeta(const FString &metaName) = 0;
	template<typename MetadataType> virtual TSharedPtr<openvdb::TypedMetadata<MetadataType>> GetGridMetaValue(const FString &gridName, const FString &metaName) = 0;
	template<typename MetadataType> virtual void InsertGridMeta(const FString &gridName, const FString &metaName, const MetadataType &metaValue) = 0;
	template<typename MetadataType> virtual void RemoveGridMeta(const FString &gridName, const FString &metaName) = 0;
	virtual void WriteChanges() = 0;
};