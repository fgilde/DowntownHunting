#include "DHCameraMode_FirstPerson.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DHCameraMode_FirstPerson)

void UDHCameraMode_FirstPerson::OnActivation()
{
	Super::OnActivation();
	SetHeadHidden(true);
}

void UDHCameraMode_FirstPerson::OnDeactivation()
{
	SetHeadHidden(false);
	Super::OnDeactivation();
}

void UDHCameraMode_FirstPerson::SetHeadHidden(bool bHidden) const
{
	const ACharacter* Character = Cast<ACharacter>(GetTargetActor());
	USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	if (!Mesh)
	{
		return;
	}

	if (bHidden)
	{
		Mesh->HideBoneByName(HeadBoneName, PBO_None);
	}
	else
	{
		Mesh->UnHideBoneByName(HeadBoneName);
	}
}
