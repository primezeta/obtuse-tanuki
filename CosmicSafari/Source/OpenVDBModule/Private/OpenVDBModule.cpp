#include "OpenVDBModule.h"

DEFINE_LOG_CATEGORY(LogOpenVDBModule)

void FOpenVDBModule::RegisterVdb(const FString &FilePath, bool EnableGridStats, bool EnableDelayLoad)
{
	if (!VdbRegistry.Contains(FilePath))
	{
		VdbRegistry.Add(FilePath, TSharedPtr<VdbHandlePrivateType>(new VdbHandlePrivateType(FilePath, EnableGridStats, EnableDelayLoad)));
	}
	//else
	//{
	//	TSharedPtr<VdbHandlePrivateType> &vdb = VdbRegistry.FindChecked(FilePath);
	//	if (vdb->EnableGridStats != EnableGridStats ||
	//		vdb->EnableDelayLoad != EnableDelayLoad)
	//	{
	//		VdbRegistry.Add(FilePath, TSharedPtr<VdbHandlePrivateType>(new VdbHandlePrivateType(FilePath, EnableGridStats, EnableDelayLoad)));
	//	}
	//}
	VdbRegistry.FindChecked(FilePath)->Init();
}

IMPLEMENT_GAME_MODULE(FOpenVDBModule, OpenVDBModule);