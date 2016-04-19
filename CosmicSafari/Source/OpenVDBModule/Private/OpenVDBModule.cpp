#include "OpenVDBModule.h"

DEFINE_LOG_CATEGORY(LogOpenVDBModule)

void FOpenVDBModule::RegisterVdb(const FString &FilePath, bool EnableGridStats, bool EnableDelayLoad, TArray<TArray<FVector>> &VertexBuffers, TArray<TArray<int32>> &PolygonBuffers, TArray<TArray<FVector>> &NormalBuffers)
{
	if (!VdbRegistry.Contains(FilePath))
	{
		VdbRegistry.Add(FilePath, TSharedPtr<VdbHandlePrivateType>(new VdbHandlePrivateType(FilePath, EnableGridStats, EnableDelayLoad, &VertexBuffers, &PolygonBuffers, &NormalBuffers)));
	}
	else
	{
		TSharedPtr<VdbHandlePrivateType> &vdb = VdbRegistry.FindChecked(FilePath);
		if (vdb->EnableGridStats != EnableGridStats ||
			vdb->EnableDelayLoad != EnableDelayLoad)
		{
			VdbRegistry.Add(FilePath, TSharedPtr<VdbHandlePrivateType>(new VdbHandlePrivateType(FilePath, EnableGridStats, EnableDelayLoad, &VertexBuffers, &PolygonBuffers, &NormalBuffers)));
		}
		else
		{
			vdb->InitBuffers(&VertexBuffers, &PolygonBuffers, &NormalBuffers);
		}
	}
	VdbRegistry.FindChecked(FilePath)->Init();
}

IMPLEMENT_GAME_MODULE(FOpenVDBModule, OpenVDBModule);