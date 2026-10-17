#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"

#include "DHEditorHelpers.generated.h"

class USkeletalMesh;

/** Small editor-time helpers for the asset scripts in Tools/Python (things Python cannot set itself). */
UCLASS()
class DOWNTOWNUI_API UDHEditorHelpers : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Adds or updates a mesh socket; reuses an unnamed socket if there is one. Editor only, marks the mesh dirty. */
	UFUNCTION(BlueprintCallable, Category = "Downtown Hunting|Editor")
	static bool SetMeshSocket(USkeletalMesh* Mesh, FName SocketName, FName BoneName, FVector Location, FRotator Rotation);
};
