// Fill out your copyright notice in the Description page of Project Settings.

#include "OpenVDBModule.h"
#include "ComponentTask.h"

TArray<FComponentTask> Tasks;

UVdbHandle::UVdbHandle(const FObjectInitializer& ObjectInitializer)
	: VdbName(GetFName().ToString())
{
	//TODO: VDB configuration options in the editor (via plugin?)
	MeshMethod = EMeshType::MESH_TYPE_CUBES;
	FilePath = TEXT("C:\\Users\\zach\\Documents\\Unreal Projects\\obtuse-tanuki\\CosmicSafari\\Binaries\\Win64\\vdbs\\perlin.vdb"); //TODO: Remove this path
	EnableDelayLoad = true;
	EnableGridStats = true;
	WorldName = TEXT("perlin_test");
	PerlinSeed = 0;
	PerlinFrequency = 4.0f;
	PerlinLacunarity = 0.49f;
	PerlinPersistence = 2.01f;
	PerlinOctaveCount = 9;
	ThreadedGridOps = true;
	IsOpen = false;
	bWantsInitializeComponent = true;
	SetComponentTickEnabled(true);
	PrimaryComponentTick.TickInterval = 5.0f;
}

void UVdbHandle::InitializeComponent()
{
	Super::InitializeComponent();
	RegisterComponent();
}

void UVdbHandle::BeginPlay()
{
	OpenVoxelDatabaseGuard();
}

void UVdbHandle::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	//Cleanup finished tasks (starting from the back of the array and removing each finished task)
	for (int32 i = Tasks.Num() - 1; i >= 0; i--)
	{
		if (Tasks[i].IsTaskFinished())
		{
			check(!Tasks[i].IsTaskRunning());
			Tasks.RemoveAt(i);
		}
	}
}

void UVdbHandle::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	//if(EndPlayReason != EEndPlayReason::LevelTransition)
	//CloseVoxelDatabaseGuard(); TODO: Ever need to close and update the database here?
}

void UVdbHandle::BeginDestroy()
{
	CloseVoxelDatabaseGuard();
	Super::BeginDestroy();
}

void UVdbHandle::RunVoxelDatabaseTask(const FString &ThreadName, TFunction<void(void)> &&Task)
{
	//Launch the thread
	const int32 idx = Tasks.Add(FComponentTask(Task));
	Tasks[idx].CreateThread(ThreadName);
}

FString UVdbHandle::AddGrid(const FString &gridName, const FIntVector &regionIndex, const FVector &voxelSize, TArray<FProcMeshSection> &sectionBuffers)
{
	FOpenVDBModule::CheckVoxelDatabaseIn(VdbName);
	const FString gridID = FOpenVDBModule::AddGrid(VdbName, gridName, regionIndex, voxelSize, sectionBuffers);
	return gridID;
}

void UVdbHandle::ReadGridTree(const FString &gridID, FIntVector &startIndex, FIntVector &endIndex)
{
	FOpenVDBModule::CheckVoxelDatabaseIn(VdbName);
	FOpenVDBModule::ReadGridTree(VdbName, gridID, startIndex, endIndex);
}

bool UVdbHandle::FillTreePerlin(const FString &gridID, FIntVector &startFill, FIntVector &endFill)
{
	FOpenVDBModule::CheckVoxelDatabaseIn(VdbName);
	const bool isChanged = FOpenVDBModule::FillTreePerlin(VdbName, gridID, startFill, endFill, PerlinSeed, PerlinFrequency, PerlinLacunarity, PerlinPersistence, PerlinOctaveCount, ThreadedGridOps);
	return isChanged;
}

bool UVdbHandle::ExtractIsoSurface(const FString &gridID, TArray<TEnumAsByte<EVoxelType>> &sectionMaterialIDs, FBox &gridDimensions, FVector &initialLocation)
{
	FOpenVDBModule::CheckVoxelDatabaseIn(VdbName);
	const bool hasActiveVoxels = FOpenVDBModule::ExtractIsoSurface(VdbName, gridID, MeshMethod, sectionMaterialIDs, gridDimensions, initialLocation, ThreadedGridOps);
	return hasActiveVoxels;
}

void UVdbHandle::MeshGrid(const FString &gridID)
{
	FOpenVDBModule::CheckVoxelDatabaseIn(VdbName);
	FOpenVDBModule::MeshGrid(VdbName, gridID, MeshMethod);
}

TArray<FString> UVdbHandle::GetAllGridIDs()
{
	FOpenVDBModule::CheckVoxelDatabaseIn(VdbName);
	const TArray<FString> GridIDs = FOpenVDBModule::GetAllGridIDs(VdbName); //TODO: Return as const &
	return GridIDs;
}

void UVdbHandle::RemoveGrid(const FString &gridID)
{
	FOpenVDBModule::CheckVoxelDatabaseIn(VdbName);
	FOpenVDBModule::RemoveGrid(VdbName, gridID);
}

bool UVdbHandle::SetRegionScale(const FIntVector &regionScale)
{
	FOpenVDBModule::CheckVoxelDatabaseIn(VdbName);
	bool isScaleValid = false;
	if (regionScale.X > 0 && regionScale.Y > 0 && regionScale.Z > 0)
	{
		FOpenVDBModule::SetRegionScale(VdbName, regionScale);
		isScaleValid = true;
	}
	return isScaleValid;
}

void UVdbHandle::GetVoxelCoord(const FString &gridID, const FVector &worldLocation, FIntVector &outVoxelCoord)
{
	FOpenVDBModule::CheckVoxelDatabaseIn(VdbName);
	FOpenVDBModule::GetVoxelCoord(VdbName, gridID, worldLocation, outVoxelCoord);
}

bool UVdbHandle::GetGridDimensions(const FString &gridID, FVector &startLocation)
{
	FOpenVDBModule::CheckVoxelDatabaseIn(VdbName);
	const bool hasActiveVoxels = FOpenVDBModule::GetGridDimensions(VdbName, gridID, startLocation);
	return hasActiveVoxels;
}

bool UVdbHandle::GetGridDimensions(const FString &gridID, FBox &worldBounds)
{
	FOpenVDBModule::CheckVoxelDatabaseIn(VdbName);
	const bool hasActiveVoxels = FOpenVDBModule::GetGridDimensions(VdbName, gridID, worldBounds);
	return hasActiveVoxels;
}

bool UVdbHandle::GetGridDimensions(const FString &gridID, FBox &worldBounds, FVector &startLocation)
{
	FOpenVDBModule::CheckVoxelDatabaseIn(VdbName);
	const bool hasActiveVoxels = FOpenVDBModule::GetGridDimensions(VdbName, gridID, worldBounds, startLocation);
	return hasActiveVoxels;
}

FIntVector UVdbHandle::GetRegionIndex(const FVector &worldLocation)
{
	FOpenVDBModule::CheckVoxelDatabaseIn(VdbName);
	const FIntVector regionIndex = FOpenVDBModule::GetRegionIndex(VdbName, worldLocation);
	return regionIndex;
}

void UVdbHandle::WriteAllGrids()
{
	FOpenVDBModule::CheckVoxelDatabaseIn(VdbName);
	FOpenVDBModule::WriteChanges(VdbName);
}

void UVdbHandle::OpenVoxelDatabaseGuard()
{
	if (!IsOpen)
	{
		check(!FilePath.IsEmpty());
		IsOpen = FOpenVDBModule::IsAvailable() && !FilePath.IsEmpty() && FOpenVDBModule::OpenVoxelDatabase(VdbName, FilePath, EnableGridStats, EnableDelayLoad);
		check(IsOpen);
	}
}

void UVdbHandle::CloseVoxelDatabaseGuard()
{
	if (IsOpen)
	{
		const bool saveChanges = false;
		FOpenVDBModule::CloseVoxelDatabase(VdbName, saveChanges);
		IsOpen = false;
	}
}

//virtual void OnRegister();
//
///**
//* Called when a component is unregistered. Called after DestroyRenderState_Concurrent and DestroyPhysicsState are called.
//*/
//virtual void OnUnregister();
//
//// Always called immediately before properties are received from the remote.
//virtual void PreNetReceive() override { }
//
//// Always called immediately after properties are received from the remote.
//virtual void PostNetReceive() override { }