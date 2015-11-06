#include "OpenVDBModule.h"
#include "libovdb.h"
#include "../Plugins/Runtime/ProceduralMeshComponent/Source/Public/ProceduralMeshComponent.h"
#include <string>
#include <vector>

void FOpenVDBModule::StartupModule()
{
	//std::string filename = "C:/Users/zach/Documents/Unreal Projects/obtuse-tanuki/CosmicSafari/ThirdParty/Build/x64/Release/vdbs/noise_w3008_h3008_l3008_t16_s1_t1.vdb";
	//std::string filename = "C:/Users/zach/Documents/Unreal Projects/obtuse-tanuki/CosmicSafari/ThirdParty/Build/x64/Release/vdbs/noise_w288_h288_l288_t16_s1_t1.vdb";
	std::string filename = "C:/Users/zach/Documents/Unreal Projects/obtuse-tanuki/CosmicSafari/ThirdParty/Build/x64/Release/vdbs/noise_w288_h288_l288_t16_s1_t0.vdb";
	//std::string filename = "C:/Users/zach/Documents/Unreal Projects/obtuse-tanuki/CosmicSafari/ThirdParty/Build/x64/Debug/vdbs/noise_w288_h288_l288_t16_s1_t0.vdb";

	double isovalue = 0.0;
	double adaptivity = 0.0;

	if (OvdbInitialize())
	{
		//TODO: Handle Ovdb errors
	}
	if(OvdbLoadVdb(filename))
	{
		//TODO: Handle Ovdb errors
	}
	if(OvdbVolumeToMesh(isovalue, adaptivity))
	{
		//TODO: Handle Ovdb errors
	}

	UProceduralMeshComponent * mesh = nullptr;
	//mesh.CreateMeshSection(int32 SectionIndex, const TArray<FVector>& Vertices, const TArray<int32>& Triangles, const TArray<FVector>& Normals, const TArray<FVector2D>& UV0, const TArray<FColor>& VertexColors, const TArray<FProcMeshTangent>& Tangents, bool bCreateCollision)

	FVector vertex;
	TArray<FVector> vertices;
	while (OvdbGetNextMeshPoint(vertex.X, vertex.Y, vertex.Z))
	{
		vertices.Add(vertex);
	}

	TArray<int32> triangles;
	uint32_t tris[3];
	while (OvdbGetNextMeshTriangle(tris[0], tris[1], tris[2]))
	{
		if (tris[0] > INT_MAX || tris[1] > INT_MAX || tris[2] > INT_MAX)
		{
			int x = 0; //for breakpoint until I decide what to do
			return;
		}
		triangles.Add((int32)tris[0]);
		triangles.Add((int32)tris[1]);
		triangles.Add((int32)tris[2]);
	}
	return;
}

void FOpenVDBModule::ShutdownModule()
{
	OvdbUninitialize();
	return;
}

IMPLEMENT_GAME_MODULE(FOpenVDBModule, OpenVDBModule);