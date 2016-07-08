#pragma once
#include "ProceduralMeshComponent.h"
#include "ProceduralMeshComponent_Async.generated.h"

/**
*	Component that allows you to specify custom triangle mesh geometry
*	Beware! This feature is experimental and may be substantially changed in future releases.
*/
UCLASS(hidecategories = (Object, LOD), meta = (BlueprintSpawnableComponent), Experimental, ClassGroup = Experimental)
class OPENVDBMODULE_API UProceduralMeshComponent_Async : public UMeshComponent, public IInterface_CollisionDataProvider
{
	GENERATED_UCLASS_BODY()

		/**
		*	Create/replace a section for this procedural mesh component.
		*	@param	SectionIndex		Index of the section to create or replace.
		*	@param	Vertices			Vertex buffer of all vertex positions to use for this mesh section.
		*	@param	Triangles			Index buffer indicating which vertices make up each triangle. Length must be a multiple of 3.
		*	@param	Normals				Optional array of normal vectors for each vertex. If supplied, must be same length as Vertices array.
		*	@param	UV0					Optional array of texture co-ordinates for each vertex. If supplied, must be same length as Vertices array.
		*	@param	VertexColors		Optional array of colors for each vertex. If supplied, must be same length as Vertices array.
		*	@param	Tangents			Optional array of tangent vector for each vertex. If supplied, must be same length as Vertices array.
		*	@param	bCreateCollision	Indicates whether collision should be created for this section. This adds significant cost.
		*/
		UFUNCTION(BlueprintCallable, Category = "Components|ProceduralMeshAsync", meta = (AutoCreateRefTerm = "Normals,UV0,VertexColors,Tangents"))
		void CreateMeshSection(int32 SectionIndex, const TArray<FVector>& Vertices, const TArray<int32>& Triangles, const TArray<FVector>& Normals, const TArray<FVector2D>& UV0, const TArray<FColor>& VertexColors, const TArray<FProcMeshTangent>& Tangents, bool bCreateCollision);

	/**
	*	Updates a section of this procedural mesh component. This is faster than CreateMeshSection, but does not let you change topology. Collision info is also updated.
	*	@param	Vertices			Vertex buffer of all vertex positions to use for this mesh section.
	*	@param	Normals				Optional array of normal vectors for each vertex. If supplied, must be same length as Vertices array.
	*	@param	UV0					Optional array of texture co-ordinates for each vertex. If supplied, must be same length as Vertices array.
	*	@param	VertexColors		Optional array of colors for each vertex. If supplied, must be same length as Vertices array.
	*	@param	Tangents			Optional array of tangent vector for each vertex. If supplied, must be same length as Vertices array.
	*/
	UFUNCTION(BlueprintCallable, Category = "Components|ProceduralMeshAsync", meta = (AutoCreateRefTerm = "Normals,UV0,VertexColors,Tangents"))
		void UpdateMeshSection(int32 SectionIndex, const TArray<FVector>& Vertices, const TArray<FVector>& Normals, const TArray<FVector2D>& UV0, const TArray<FColor>& VertexColors, const TArray<FProcMeshTangent>& Tangents);

	/** Clear a section of the procedural mesh. Other sections do not change index. */
	UFUNCTION(BlueprintCallable, Category = "Components|ProceduralMeshAsync")
		void ClearMeshSection(int32 SectionIndex);

	/** Clear all mesh sections and reset to empty state */
	UFUNCTION(BlueprintCallable, Category = "Components|ProceduralMeshAsync")
		void ClearAllMeshSections();

	/** Control visibility of a particular section */
	UFUNCTION(BlueprintCallable, Category = "Components|ProceduralMeshAsync")
		void SetMeshSectionVisible(int32 SectionIndex, bool bNewVisibility);

	/** Returns whether a particular section is currently visible */
	UFUNCTION(BlueprintCallable, Category = "Components|ProceduralMeshAsync")
		bool IsMeshSectionVisible(int32 SectionIndex) const;

	UFUNCTION(BlueprintCallable, Category = "Components|ProceduralMeshAsync")
		void CreateEmptyMeshSection(int32 SectionIndex, bool bCreateCollision, int32 NumVerts = 0, int32 NumTriIndices = 0);

	UFUNCTION(BlueprintCallable, Category = "Components|ProceduralMeshAsync")
		void FinishMesh();

	//~ Begin Interface_CollisionDataProvider Interface
	virtual bool GetPhysicsTriMeshData(struct FTriMeshCollisionData* CollisionData, bool InUseAllTriData) override;
	virtual bool ContainsPhysicsTriMeshData(bool InUseAllTriData) const override;
	virtual bool WantsNegXTriMesh() override { return false; }
	//~ End Interface_CollisionDataProvider Interface

	/** Collision data */
	UPROPERTY(transient, duplicatetransient)
	class UBodySetup* ProcMeshBodySetup;

protected:
	/** Array of sections of mesh */
	UPROPERTY()
		TArray<FProcMeshSection> ProcMeshSections;

private:
	//~ Begin USceneComponent Interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	//~ Begin USceneComponent Interface.

	//~ Begin UPrimitiveComponent Interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual class UBodySetup* GetBodySetup() override;
	//~ End UPrimitiveComponent Interface.

	//~ Begin UMeshComponent Interface.
	virtual int32 GetNumMaterials() const override;
	//~ End UMeshComponent Interface.



	/** Update LocalBounds member from the local box of each section */
	void UpdateLocalBounds();
	/** Ensure ProcMeshBodySetup is allocated and configured */
	void CreateProcMeshBodySetup();
	/** Mark collision data as dirty, and re-create on instance if necessary */
	void UpdateCollision();

	/** Local space bounds of mesh */
	UPROPERTY()
		FBoxSphereBounds LocalBounds;


	friend class FProceduralMeshSceneProxy;
};