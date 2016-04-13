#include "OpenVDBModule.h"

DEFINE_LOG_CATEGORY(LogOpenVDBModule)

void FOpenVDBModule::RegisterVdb(const UVdbHandle &vdbObject)
{
	if (!VdbRegistry.Contains(vdbObject.FilePath))
	{
		VdbRegistry.Add(vdbObject.FilePath, TSharedPtr<VdbHandlePrivateType>(new VdbHandlePrivateType(vdbObject.FilePath, vdbObject.EnableGridStats, vdbObject.EnableDelayLoad)));
	}
	else
	{
		TSharedPtr<VdbHandlePrivateType> &vdb = VdbRegistry.FindChecked(vdbObject.FilePath);
		if (vdb->EnableGridStats != vdbObject.EnableGridStats ||
			vdb->EnableDelayLoad != vdbObject.EnableDelayLoad)
		{
			VdbRegistry.Add(vdbObject.FilePath, TSharedPtr<VdbHandlePrivateType>(new VdbHandlePrivateType(vdbObject.FilePath, vdbObject.EnableGridStats, vdbObject.EnableDelayLoad)));
		}
	}
}

IMPLEMENT_GAME_MODULE(FOpenVDBModule, OpenVDBModule);