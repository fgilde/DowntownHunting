#include "DHCameraMode_FirstPerson.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Animation/AnimSequenceBase.h"
#include "Camera/LyraCameraComponent.h"
#include "Equipment/LyraEquipmentManagerComponent.h"
#include "Weapons/LyraWeaponInstance.h"
#include "Components/ChildActorComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DHCameraMode_FirstPerson)

UDHCameraMode_FirstPerson::UDHCameraMode_FirstPerson()
{
	const TCHAR* Poses = TEXT("/Game/Characters/Heroes/Mannequin/Animations/Locomotion");
	WeaponPoses.Add(TEXT("Pistol"), TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(FString::Printf(TEXT("%s/Pistol/MM_Pistol_Idle_ADS.MM_Pistol_Idle_ADS"), Poses))));
	WeaponPoses.Add(TEXT("Shotgun"), TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(FString::Printf(TEXT("%s/Shotgun/MM_Shotgun_Idle_ADS.MM_Shotgun_Idle_ADS"), Poses))));
	DefaultPose = TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(FString::Printf(TEXT("%s/Rifle/MM_Rifle_Idle_ADS.MM_Rifle_Idle_ADS"), Poses)));
}

void UDHCameraMode_FirstPerson::OnDeactivation()
{
	DestroyCopies();
	SetFirstPersonRendering(false);
	Super::OnDeactivation();
}

void UDHCameraMode_FirstPerson::UpdateView(float DeltaTime)
{
	Super::UpdateView(DeltaTime);

	ACharacter* Character = Cast<ACharacter>(GetTargetActor());
	USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	if (!Mesh || !Character->IsLocallyControlled())
	{
		DestroyCopies();
		return;
	}

	// Rebuild when the equipped weapon actors change (equip, swap, drop) or the copies got lost.
	TArray<AActor*> Weapons = GetAttachedWeapons(*Character);
	bool bWeaponsChanged = Weapons.Num() != CopiedWeapons.Num();
	for (int32 i = 0; !bWeaponsChanged && i < Weapons.Num(); ++i)
	{
		bWeaponsChanged = CopiedWeapons[i].Get() != Weapons[i];
	}
	if (bWeaponsChanged || !BodyCopy.IsValid())
	{
		DestroyCopies();
		BuildCopies(*Character);
	}

	USkeletalMeshComponent* Body = BodyCopy.Get();
	if (!Body)
	{
		return;
	}

	UpdateWeaponState(*Character, DeltaTime);
	View.FieldOfView = FMath::Lerp(FieldOfView, ADSFieldOfView, ADSAlpha);
	SetFirstPersonRendering(true);

	UMeshComponent* Weapon = WeaponCopy.Get();
	Body->SetVisibility(Weapon != nullptr, /*bPropagateToChildren=*/ true);
	if (!Weapon)
	{
		return;
	}

	// Grip and muzzle of the weapon copy in the body copy's component space.
	const FTransform WeaponInBody = Weapon->GetRelativeTransform() * Body->GetSocketTransform(Weapon->GetAttachSocketName(), RTS_Component);
	const FVector Grip = WeaponInBody.GetLocation();
	const FVector Muzzle = Weapon->GetSocketTransform(MuzzleSocket, RTS_Component).GetLocation();
	const FVector Barrel = WeaponInBody.TransformVector(Muzzle);

	// Face the view like the real mesh does, then correct so the barrel points straight along the view
	// (the ready poses hold the weapon at an angle to the body), then add recoil pitch.
	const FQuat ViewQuat = View.Rotation.Quaternion();
	const FQuat Facing = ViewQuat * Mesh->GetRelativeRotation().Quaternion();
	const FQuat Correction = FQuat::FindBetweenNormals(Facing.RotateVector(Barrel).GetSafeNormal(), ViewQuat.GetForwardVector());
	const FQuat Kick(ViewQuat.GetRightVector(), FMath::DegreesToRadians(-RecoilPitch * Recoil));
	const FQuat BodyQuat = Kick * Correction * Facing;

	FVector Offset = FMath::Lerp(HipWeaponOffset, ADSWeaponOffset, ADSAlpha);
	Offset.X -= RecoilBack * Recoil;
	const FVector Target = View.Location + ViewQuat.RotateVector(Offset);
	Body->SetWorldLocationAndRotation(Target - BodyQuat.RotateVector(Grip * Body->GetComponentScale()), BodyQuat);
}

void UDHCameraMode_FirstPerson::UpdateWeaponState(const ACharacter& Character, float DeltaTime)
{
	static const FGameplayTag ADSTag = FGameplayTag::RequestGameplayTag(TEXT("Event.Movement.ADS"));
	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(&Character);
	const bool bADS = ASC && ASC->HasMatchingGameplayTag(ADSTag);
	ADSAlpha = FMath::FInterpConstantTo(ADSAlpha, bADS ? 1.0f : 0.0f, DeltaTime, ADSBlendSpeed / 2.0f);

	// Lyra stamps the weapon on each shot; a drop in "time since interacted" means it just fired (or was
	// equipped, which also rebuilds the copies, so the kick is barely visible then).
	Recoil = FMath::FInterpTo(Recoil, 0.0f, DeltaTime, RecoilRecoverySpeed);
	ULyraEquipmentManagerComponent* Equipment = Character.FindComponentByClass<ULyraEquipmentManagerComponent>();
	const ULyraWeaponInstance* Weapon = Equipment ? Equipment->GetFirstInstanceOfType<ULyraWeaponInstance>() : nullptr;
	if (Weapon)
	{
		const float TimeSince = Weapon->GetTimeSinceLastInteractedWith();
		if (TimeSince < LastTimeSinceInteracted)
		{
			Recoil = 1.0f;
		}
		LastTimeSinceInteracted = TimeSince;
	}
}

void UDHCameraMode_FirstPerson::SetFirstPersonRendering(bool bEnabled) const
{
	if (ULyraCameraComponent* Camera = GetLyraCameraComponent())
	{
		// The renderer uses the first-person FOV for these primitives as soon as either option is on, so keep
		// it equal to the world FOV to avoid distortion.
		Camera->SetEnableFirstPersonScale(bEnabled);
		Camera->SetFirstPersonScale(FirstPersonScale);
		Camera->SetEnableFirstPersonFieldOfView(bEnabled);
		Camera->SetFirstPersonFieldOfView(View.FieldOfView);
	}
}

void UDHCameraMode_FirstPerson::BuildCopies(ACharacter& Character)
{
	USkeletalMeshComponent* Mesh = Character.GetMesh();

	// Visible body parts are Lyra cosmetic child actors, leader-posed to the (invisible) anim mesh.
	TArray<USkeletalMeshComponent*> BodyParts;
	for (USceneComponent* Child : Mesh->GetAttachChildren())
	{
		if (const UChildActorComponent* ChildActor = Cast<UChildActorComponent>(Child))
		{
			if (AActor* Part = ChildActor->GetChildActor())
			{
				Part->GetComponents<USkeletalMeshComponent>(BodyParts, /*bIncludeFromChildActors=*/ false);
			}
		}
	}
	if (BodyParts.IsEmpty())
	{
		return;
	}

	auto MakeCopy = [&Character, this](UMeshComponent* Original, UMeshComponent* Copy)
	{
		for (int32 i = 0; i < Original->GetNumMaterials(); ++i)
		{
			Copy->SetMaterial(i, Original->GetMaterial(i));
		}
		Copy->SetOnlyOwnerSee(true);
		Copy->SetCastShadow(false);
		Copy->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Copy->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
		Copy->RegisterComponent();
		Originals.Add(Original);
		Copies.Add(Copy);
	};

	USkeletalMeshComponent* Root = nullptr;
	for (USkeletalMeshComponent* Part : BodyParts)
	{
		USkeletalMeshComponent* Copy = NewObject<USkeletalMeshComponent>(&Character, NAME_None, RF_Transient);
		Copy->SetSkeletalMeshAsset(Part->GetSkeletalMeshAsset());
		if (Root)
		{
			Copy->SetLeaderPoseComponent(Root);
		}
		else
		{
			Copy->SetWorldScale3D(Mesh->GetComponentScale());
			Copy->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			Copy->AnimationData.AnimToPlay = FindPose(GetAttachedWeapons(Character));
			Copy->AnimationData.bSavedLooping = true;
			Copy->AnimationData.bSavedPlaying = true;
		}
		for (const FName& Bone : HiddenBones)
		{
			Copy->HideBoneByName(Bone, PBO_None);
		}
		if (Root)
		{
			Copy->SetupAttachment(Root);
		}
		MakeCopy(Part, Copy);
		Root = Root ? Root : Copy;
	}
	BodyCopy = Root;

	// Weapons are attached to sockets of the anim mesh, so the same sockets exist on the body copy.
	for (AActor* Weapon : GetAttachedWeapons(Character))
	{
		const FName Socket = Weapon->GetRootComponent()->GetAttachSocketName();
		const FTransform SocketTransform = Mesh->GetSocketTransform(Socket);

		TArray<UMeshComponent*> WeaponMeshes;
		Weapon->GetComponents<UMeshComponent>(WeaponMeshes);
		for (UMeshComponent* WeaponMesh : WeaponMeshes)
		{
			UMeshComponent* Copy = nullptr;
			if (const UStaticMeshComponent* Static = Cast<UStaticMeshComponent>(WeaponMesh))
			{
				UStaticMeshComponent* StaticCopy = NewObject<UStaticMeshComponent>(&Character, NAME_None, RF_Transient);
				StaticCopy->SetStaticMesh(Static->GetStaticMesh());
				Copy = StaticCopy;
			}
			else if (USkeletalMeshComponent* Skeletal = Cast<USkeletalMeshComponent>(WeaponMesh))
			{
				USkeletalMeshComponent* SkeletalCopy = NewObject<USkeletalMeshComponent>(&Character, NAME_None, RF_Transient);
				SkeletalCopy->SetSkeletalMeshAsset(Skeletal->GetSkeletalMeshAsset());
				SkeletalCopy->SetLeaderPoseComponent(Skeletal);
				Copy = SkeletalCopy;
			}
			if (!Copy || !WeaponMesh->IsVisible())
			{
				continue;
			}

			Copy->SetupAttachment(Root, Socket);
			Copy->SetRelativeTransform(WeaponMesh->GetComponentTransform().GetRelativeTransform(SocketTransform));
			MakeCopy(WeaponMesh, Copy);
			if (!WeaponCopy.IsValid() && Copy->DoesSocketExist(MuzzleSocket))
			{
				WeaponCopy = Copy;
			}
		}
		CopiedWeapons.Add(Weapon);
	}

	SetOriginalsHidden(true);
}

void UDHCameraMode_FirstPerson::DestroyCopies()
{
	SetOriginalsHidden(false);
	for (const TWeakObjectPtr<UMeshComponent>& Copy : Copies)
	{
		if (Copy.IsValid())
		{
			Copy->DestroyComponent();
		}
	}
	Originals.Reset();
	Copies.Reset();
	CopiedWeapons.Reset();
	BodyCopy.Reset();
	WeaponCopy.Reset();
}

void UDHCameraMode_FirstPerson::SetOriginalsHidden(bool bHidden)
{
	// WorldSpaceRepresentation hides the primitive from its owner's view but keeps its shadow and
	// leaves it untouched for every other viewer.
	const EFirstPersonPrimitiveType Type = bHidden ? EFirstPersonPrimitiveType::WorldSpaceRepresentation : EFirstPersonPrimitiveType::None;
	for (const TWeakObjectPtr<UMeshComponent>& Original : Originals)
	{
		if (Original.IsValid())
		{
			Original->SetFirstPersonPrimitiveType(Type);
		}
	}
}

UAnimSequenceBase* UDHCameraMode_FirstPerson::FindPose(const TArray<AActor*>& Weapons) const
{
	for (const AActor* Weapon : Weapons)
	{
		const FString ClassName = Weapon->GetClass()->GetName();
		for (const TPair<FString, TSoftObjectPtr<UAnimSequenceBase>>& Pose : WeaponPoses)
		{
			if (ClassName.Contains(Pose.Key))
			{
				return Pose.Value.LoadSynchronous();
			}
		}
	}
	return DefaultPose.LoadSynchronous();
}

TArray<AActor*> UDHCameraMode_FirstPerson::GetAttachedWeapons(const ACharacter& Character) const
{
	// Lyra equipment actors are spawned separately and attached to the anim mesh.
	TArray<AActor*> Weapons;
	for (USceneComponent* Child : Character.GetMesh()->GetAttachChildren())
	{
		AActor* Owner = Child ? Child->GetOwner() : nullptr;
		if (Owner && Owner != &Character && Owner->GetRootComponent() == Child)
		{
			Weapons.Add(Owner);
		}
	}
	return Weapons;
}
