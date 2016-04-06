#pragma once
#include "OpenVDBModule.h"
#include "VdbHandlePrivate.h"

extern VdbRegistryType VdbRegistry;

void UVdbHandle::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UVdbHandle, FilePath) && !FilePath.IsEmpty())
	{
		UVdbHandle::RegisterVdb(this);
	}
}