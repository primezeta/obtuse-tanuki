#pragma once

/**
*
*/
class FComponentTask : public FRunnable
{
public:
	FComponentTask(TFunction<void(void)> &task) : Task(task), bIsRunning(false), bIsFinished(false)
	{
	}

	void CreateThread(const FString &ThreadName)
	{
		if (!bIsRunning)
		{
			bIsRunning = true; //Must always call CreateThread from the same thread for this to work
			bIsFinished = false;
			FRunnableThread::Create(this, *ThreadName);
		}
	}

	virtual uint32 Run() override
	{
		Task();
		bIsRunning = false;
		bIsFinished = true;
		return 0;
	}

	bool IsTaskRunning()
	{
		return bIsRunning;
	}

	bool IsTaskFinished()
	{
		return bIsFinished;
	}

private:
	TFunction<void(void)> &Task;
	bool bIsRunning;
	bool bIsFinished;
};
