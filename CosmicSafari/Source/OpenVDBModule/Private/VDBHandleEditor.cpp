#pragma once
#include "OpenVDBModule.h"
#include "VDBHandlePrivate.h"

extern VDBRegistryType VDBRegistry;

void UVDBHandle::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	TSharedPtr<VDBHandlePrivateType> VDBPrivatePtr = VDBRegistry.FindChecked(FilePath);
	VDBPrivatePtr->WriteChangesAsync();
}