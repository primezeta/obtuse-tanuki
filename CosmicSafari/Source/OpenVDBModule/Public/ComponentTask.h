#pragma once
#include "EngineMinimal.h"
#include "ComponentTask.generated.h"

/**
*
*/
UCLASS()
class OPENVDBMODULE_API FComponentTask : public FRunnable
{
	GENERATED_BODY()

public:
	ComponentTask(TFunction<void(void)> &task) : Task(task), bIsRunning(false)
	{
	}

	void CreateThread(const FString &ThreadName)
	{
		if (!bIsRunning)
		{
			bIsRunning = true; //Must always call CreateThread from the same thread for this to work
			FRunnableThread::Create(this, *ThreadName);
		}
	}

	virtual uint32 Run() override
	{
		Task();
		bIsRunning = false;
		return 0;
	}

private:
	TFunction<void(void)> &Task;
	bool bIsRunning;
};
