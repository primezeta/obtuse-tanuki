// Fill out your copyright notice in the Description page of Project Settings.

#include "OpenVDBModule.h"

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
	OnCloseWriteChangesAsync = true;
}

void UVdbHandle::InitializeComponent()
{
	Super::InitializeComponent();
	RegisterComponent();
}

void UVdbHandle::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	//if(EndPlayReason != EEndPlayReason::LevelTransition)
	CloseVoxelDatabaseGuard(OnCloseWriteChangesAsync);
}

void UVdbHandle::BeginDestroy()
{
	CloseVoxelDatabaseGuard(OnCloseWriteChangesAsync);
	Super::BeginDestroy();
}

FString UVdbHandle::AddGrid(const FString &gridName, const FIntVector &regionIndex, const FVector &voxelSize, TArray<FProcMeshSection> &sectionBuffers)
{
	OpenVoxelDatabaseGuard();
	const FString gridID = FOpenVDBModule::AddGrid(VdbName, gridName, regionIndex, voxelSize, sectionBuffers);
	return gridID;
}

void UVdbHandle::ReadGridTree(const FString &gridID, FIntVector &startIndex, FIntVector &endIndex)
{
	OpenVoxelDatabaseGuard();
	FOpenVDBModule::ReadGridTree(VdbName, gridID, startIndex, endIndex);
}

bool UVdbHandle::FillTreePerlin(const FString &gridID, FIntVector &startFill, FIntVector &endFill)
{
	OpenVoxelDatabaseGuard();
	const bool isChanged = FOpenVDBModule::FillTreePerlin(VdbName, gridID, startFill, endFill, PerlinSeed, PerlinFrequency, PerlinLacunarity, PerlinPersistence, PerlinOctaveCount, ThreadedGridOps);
	return isChanged;
}

bool UVdbHandle::ExtractIsoSurface(const FString &gridID, TArray<TEnumAsByte<EVoxelType>> &sectionMaterialIDs, FBox &gridDimensions, FVector &initialLocation)
{
	OpenVoxelDatabaseGuard();
	const bool hasActiveVoxels = FOpenVDBModule::ExtractIsoSurface(VdbName, gridID, MeshMethod, sectionMaterialIDs, gridDimensions, initialLocation, ThreadedGridOps);
	return hasActiveVoxels;
}

void UVdbHandle::MeshGrid(const FString &gridID)
{
	OpenVoxelDatabaseGuard();
	FOpenVDBModule::MeshGrid(VdbName, gridID, MeshMethod);
}

TArray<FString> UVdbHandle::GetAllGridIDs()
{
	OpenVoxelDatabaseGuard();
	const TArray<FString> GridIDs = FOpenVDBModule::GetAllGridIDs(VdbName); //TODO: Return as const &
	return GridIDs;
}

void UVdbHandle::RemoveGrid(const FString &gridID)
{
	OpenVoxelDatabaseGuard();
	FOpenVDBModule::RemoveGrid(VdbName, gridID);
}

bool UVdbHandle::SetRegionScale(const FIntVector &regionScale)
{
	bool isScaleValid = false;
	OpenVoxelDatabaseGuard();
	if (regionScale.X > 0 && regionScale.Y > 0 && regionScale.Z > 0)
	{
		FOpenVDBModule::SetRegionScale(VdbName, regionScale);
		isScaleValid = true;
	}
	return isScaleValid;
}

void UVdbHandle::GetVoxelCoord(const FString &gridID, const FVector &worldLocation, FIntVector &outVoxelCoord)
{
	OpenVoxelDatabaseGuard();
	FOpenVDBModule::GetVoxelCoord(VdbName, gridID, worldLocation, outVoxelCoord);
}

bool UVdbHandle::GetGridDimensions(const FString &gridID, FVector &startLocation)
{
	OpenVoxelDatabaseGuard();
	const bool hasActiveVoxels = FOpenVDBModule::GetGridDimensions(VdbName, gridID, startLocation);
	return hasActiveVoxels;
}

bool UVdbHandle::GetGridDimensions(const FString &gridID, FBox &worldBounds)
{
	OpenVoxelDatabaseGuard();
	const bool hasActiveVoxels = FOpenVDBModule::GetGridDimensions(VdbName, gridID, worldBounds);
	return hasActiveVoxels;
}

bool UVdbHandle::GetGridDimensions(const FString &gridID, FBox &worldBounds, FVector &startLocation)
{
	OpenVoxelDatabaseGuard();
	const bool hasActiveVoxels = FOpenVDBModule::GetGridDimensions(VdbName, gridID, worldBounds, startLocation);
	return hasActiveVoxels;
}

FIntVector UVdbHandle::GetRegionIndex(const FVector &worldLocation)
{
	OpenVoxelDatabaseGuard();
	const FIntVector regionIndex = FOpenVDBModule::GetRegionIndex(VdbName, worldLocation);
	return regionIndex;
}

void UVdbHandle::WriteAllGrids(bool isAsync)
{
	OpenVoxelDatabaseGuard();
	const bool isFinal = false; //Never final from this level
	FOpenVDBModule::WriteChanges(VdbName, isFinal, isAsync);
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

void UVdbHandle::CloseVoxelDatabaseGuard(bool isAsync)
{
	if (IsOpen)
	{
		const bool isFinal = false;
		FOpenVDBModule::CloseVoxelDatabase(VdbName, isFinal, isAsync);
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