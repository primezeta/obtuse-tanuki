// Fill out your copyright notice in the Description page of Project Settings.

#include "OpenVDBModule.h"
#include "ProceduralTerrainMeshComponent.h"

DECLARE_CYCLE_STAT(TEXT("FDelegateGraphTask.AddGrid"), STAT_FDelegateGraphTask_AddGrid, STATGROUP_TaskGraphTasks);
DECLARE_CYCLE_STAT(TEXT("FDelegateGraphTask.ReadGridTree"), STAT_FDelegateGraphTask_ReadGridTree, STATGROUP_TaskGraphTasks);
DECLARE_CYCLE_STAT(TEXT("FDelegateGraphTask.FillTreeValues"), STAT_FDelegateGraphTask_FillTreeValues, STATGROUP_TaskGraphTasks);
DECLARE_CYCLE_STAT(TEXT("FDelegateGraphTask.ExtractIsoSurface"), STAT_FDelegateGraphTask_ExtractIsoSurface, STATGROUP_TaskGraphTasks);
DECLARE_CYCLE_STAT(TEXT("FDelegateGraphTask.RemoveGrid"), STAT_FDelegateGraphTask_RemoveGrid, STATGROUP_TaskGraphTasks);
DECLARE_CYCLE_STAT(TEXT("FDelegateGraphTask.MeshGrid"), STAT_FDelegateGraphTask_MeshGrid, STATGROUP_TaskGraphTasks);
DECLARE_CYCLE_STAT(TEXT("FDelegateGraphTask.FinishAllSections"), STAT_FDelegateGraphTask_FinishAllSections, STATGROUP_TaskGraphTasks);

UProceduralTerrainMeshComponent::UProceduralTerrainMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RegionState = EGridState::GRID_STATE_INIT;
	IsTreeReady = false;
	IsGridDirty = true;
	IsQueued = false;
	SectionCount = 0;
	NumReadySections = 0;
	//One state per actual grid state except the final one, and a grid state per voxel type
	NumStatesRemaining = NUM_TOTAL_GRID_STATES;
	SectionBounds = FBox(EForceInit::ForceInit);
	BodyInstance.SetUseAsyncScene(true); //TODO: Need async scene?
	bWantsInitializeComponent = true;
	SetComponentTickEnabled(IsComponentTickEnabled());
	RegionStart = CreateDefaultSubobject<APlayerStart>(*FString::Printf(TEXT("%s_%s"), TEXT("PlayerStart"), *GetName()));
	check(RegionStart);
	const float test = PrimaryComponentTick.TickInterval;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.bCanEverTick = true;
	for (auto i = 0; i < NUM_GRID_STATES; ++i)
	{
		IsStateStarted[i] = (int32)false;
	}
}

void UProceduralTerrainMeshComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	check(ThisTickFunction == &PrimaryComponentTick);
	if (RegionState == EGridState::GRID_STATE_INIT && IsStateStarted[(int32)EGridState::GRID_STATE_INIT] == 0)
	{
		//Initialize the grid on the render thread
		IsStateStarted[(int32)EGridState::GRID_STATE_INIT] = 1;
		FDelegateGraphTask::CreateAndDispatchWhenReady(
			FDelegateGraphTask::FDelegate::CreateUObject(this, &UProceduralTerrainMeshComponent::AddGrid),
			GET_STATID(STAT_FDelegateGraphTask_AddGrid), nullptr, ENamedThreads::GameThread, ENamedThreads::RenderThread);
	}
	else if (RegionState == EGridState::GRID_STATE_READ_TREE && IsStateStarted[(int32)EGridState::GRID_STATE_READ_TREE] == 0)
	{
		//Read the grid tree data from file on the render thread
		IsStateStarted[(int32)EGridState::GRID_STATE_READ_TREE] = 1;
		FDelegateGraphTask::CreateAndDispatchWhenReady(
			FDelegateGraphTask::FDelegate::CreateUObject(this, &UProceduralTerrainMeshComponent::ReadGridTree),
			GET_STATID(STAT_FDelegateGraphTask_ReadGridTree), nullptr, ENamedThreads::GameThread, ENamedThreads::RenderThread);
	}
	else if (RegionState == EGridState::GRID_STATE_FILL_VALUES && IsStateStarted[(int32)EGridState::GRID_STATE_FILL_VALUES] == 0)
	{
		//Fill the grid with values on the render thread
		IsStateStarted[(int32)EGridState::GRID_STATE_FILL_VALUES] = 1;
		FDelegateGraphTask::CreateAndDispatchWhenReady(
			FDelegateGraphTask::FDelegate::CreateUObject(this, &UProceduralTerrainMeshComponent::FillTreeValues),
			GET_STATID(STAT_FDelegateGraphTask_FillTreeValues), nullptr, ENamedThreads::GameThread, ENamedThreads::RenderThread);
	}
	else if (RegionState == EGridState::GRID_STATE_EXTRACT_SURFACE && IsStateStarted[(int32)EGridState::GRID_STATE_EXTRACT_SURFACE] == 0)
	{
		//Extract the isosurface on the render thread
		IsStateStarted[(int32)EGridState::GRID_STATE_EXTRACT_SURFACE] = 1;
		FDelegateGraphTask::CreateAndDispatchWhenReady(
			FDelegateGraphTask::FDelegate::CreateUObject(this, &UProceduralTerrainMeshComponent::ExtractIsoSurface),
			GET_STATID(STAT_FDelegateGraphTask_ExtractIsoSurface), nullptr, ENamedThreads::GameThread, ENamedThreads::RenderThread);
	}
	else if (RegionState == EGridState::GRID_STATE_MESH && IsStateStarted[(int32)EGridState::GRID_STATE_MESH] == 0)
	{
		//Mesh the grid on the render thread
		IsStateStarted[(int32)EGridState::GRID_STATE_MESH] = 1;
		FDelegateGraphTask::CreateAndDispatchWhenReady(
			FDelegateGraphTask::FDelegate::CreateUObject(this, &UProceduralTerrainMeshComponent::MeshGrid),
			GET_STATID(STAT_FDelegateGraphTask_MeshGrid), nullptr, ENamedThreads::GameThread, ENamedThreads::RenderThread);
	}
	else if (RegionState == EGridState::GRID_STATE_READY && IsStateStarted[(int32)EGridState::GRID_STATE_READY] == 0)
	{
		//Finish all sections on the game thread
		//TODO: Get current thread ID from UE4?
		IsStateStarted[(int32)EGridState::GRID_STATE_READY] = 1;
		FDelegateGraphTask::CreateAndDispatchWhenReady(
			FDelegateGraphTask::FDelegate::CreateUObject(this, &UProceduralTerrainMeshComponent::FinishAllSections),
			GET_STATID(STAT_FDelegateGraphTask_FinishAllSections), nullptr, ENamedThreads::GameThread, ENamedThreads::GameThread);
	}
	SetComponentTickEnabled(false);
}

void UProceduralTerrainMeshComponent::AddGrid(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	check(CurrentThread != ENamedThreads::Type::GameThread && CurrentThread != ENamedThreads::Type::GameThread_Local);
	check(VdbHandle != nullptr);
	check(RegionState == EGridState::GRID_STATE_INIT);
	MeshID = VdbHandle->AddGrid(MeshName, RegionIndex, VoxelSize, ProcMeshSections);
	NumStatesRemaining--;
	RegionState = EGridState::GRID_STATE_READ_TREE;
	SetComponentTickEnabled(true);
}

void UProceduralTerrainMeshComponent::ReadGridTree(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	check(CurrentThread != ENamedThreads::Type::GameThread && CurrentThread != ENamedThreads::Type::GameThread_Local);
	check(VdbHandle != nullptr);
	check(RegionState == EGridState::GRID_STATE_READ_TREE);
	VdbHandle->ReadGridTree(MeshID, StartIndex, EndIndex);
	//Calling GetGridDimensions at this point may result in a SectionBounds that contains the entire grid volume because no voxels may yet be active.
	//The actual bounds of active voxels are valid after calling ExtractIsoSurface in which voxels spanning the isosurface are set to active.
	VdbHandle->GetGridDimensions(MeshID, SectionBounds);
	NumStatesRemaining--;
	RegionState = EGridState::GRID_STATE_FILL_VALUES;
	SetComponentTickEnabled(true);
}

void UProceduralTerrainMeshComponent::FillTreeValues(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	check(CurrentThread != ENamedThreads::Type::GameThread && CurrentThread != ENamedThreads::Type::GameThread_Local);
	check(VdbHandle != nullptr);
	check(RegionState == EGridState::GRID_STATE_FILL_VALUES);
	VdbHandle->FillTreePerlin(MeshID, StartIndex, EndIndex);
	NumStatesRemaining--;
	RegionState = EGridState::GRID_STATE_EXTRACT_SURFACE;
	SetComponentTickEnabled(true);
}

void UProceduralTerrainMeshComponent::ExtractIsoSurface(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	check(CurrentThread != ENamedThreads::Type::GameThread && CurrentThread != ENamedThreads::Type::GameThread_Local);
	check(VdbHandle != nullptr);
	check(RegionState == EGridState::GRID_STATE_EXTRACT_SURFACE);
	VdbHandle->ExtractIsoSurface(MeshID, SectionMaterialIDs, SectionBounds, StartLocation);
	NumStatesRemaining--;
	RegionState = EGridState::GRID_STATE_MESH;
	SetComponentTickEnabled(true);
}

void UProceduralTerrainMeshComponent::MeshGrid(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	check(CurrentThread != ENamedThreads::Type::GameThread && CurrentThread != ENamedThreads::Type::GameThread_Local);
	check(VdbHandle != nullptr);
	check(RegionState == EGridState::GRID_STATE_MESH);
	VdbHandle->MeshGrid(MeshID);
	FinishRender(); //TODO: Where to call FinishRender()?
	NumStatesRemaining--;
	RegionState = EGridState::GRID_STATE_READY;
	SetComponentTickEnabled(true);
}

void UProceduralTerrainMeshComponent::FinishAllSections(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	check(CurrentThread == ENamedThreads::Type::GameThread);
	check(VdbHandle != nullptr);
	check(RegionState == EGridState::GRID_STATE_READY);
	
	//Finish each individual section
	for (auto i = 0; i < NUM_GRID_STATES; ++i)
	{
		FinishSection(i, true);
	}
	check(NumStatesRemaining == 1);

	//All sections are done so calculate collision
	FinishCollision();
	check(RegionStart);
	RegionStart->SetActorLocation(StartLocation);
	NumStatesRemaining--;
	check(NumStatesRemaining == 0);
	RegionState = EGridState::GRID_STATE_FINISHED;
	SetComponentTickEnabled(false);
}

void UProceduralTerrainMeshComponent::FinishSection(int32 SectionIndex, bool isVisible)
{
	NumReadySections++;
	check(NumReadySections >= 0 && NumReadySections <= FVoxelData::VOXEL_TYPE_COUNT);
	NumStatesRemaining--;
	check(NumStatesRemaining >= 0);
	SetMeshSectionVisible(SectionIndex, isVisible);
}

void UProceduralTerrainMeshComponent::RemoveGrid(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	//TODO
	check(VdbHandle != nullptr);
}