#include "DHEditorHelpers.h"

#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DHEditorHelpers)

bool UDHEditorHelpers::SetMeshSocket(USkeletalMesh* Mesh, FName SocketName, FName BoneName, FVector Location, FRotator Rotation)
{
#if WITH_EDITOR
	if (!Mesh || Mesh->GetRefSkeleton().FindBoneIndex(BoneName) == INDEX_NONE)
	{
		return false;
	}
	USkeletalMeshSocket* Socket = Mesh->FindSocket(SocketName);
	if (!Socket)
	{
		Socket = Mesh->FindSocket(NAME_None);
	}
	if (!Socket)
	{
		Socket = NewObject<USkeletalMeshSocket>(Mesh);
		Mesh->AddSocket(Socket);
	}
	Mesh->Modify();
	Socket->SocketName = SocketName;
	Socket->BoneName = BoneName;
	Socket->RelativeLocation = Location;
	Socket->RelativeRotation = Rotation;
	Mesh->RebuildSocketMap();
	Mesh->MarkPackageDirty();
	return true;
#else
	return false;
#endif
}
