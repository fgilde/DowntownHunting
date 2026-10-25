#include "DHCameraMode_FirstPerson.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Animation/AnimSequenceBase.h"
#include "Camera/LyraCameraComponent.h"
#include "Equipment/LyraEquipmentManagerComponent.h"
#include "Weapons/LyraWeaponInstance.h"
#include "Components/ChildActorComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DHCameraMode_FirstPerson)

UDHCameraMode_FirstPerson::UDHCameraMode_FirstPerson()
{
	auto Pose = [](const TCHAR* Path)
	{
		return TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(FString::Printf(TEXT("/Game/Characters/Heroes/Mannequin/Animations/Locomotion/%s"), Path)));
	};
	// Grips sit far enough forward and down that the shoulders of the borrowed body stay out of view; long guns further.
	WeaponViews.Add(TEXT("Pistol"), { Pose(TEXT("Pistol/MM_Pistol_Idle_ADS.MM_Pistol_Idle_ADS")), FVector(58.0, 16.0, -24.0), FVector(58.0, 0.0, -15.0), /*bLeftArm=*/ false });
	WeaponViews.Add(TEXT("M1911"), { Pose(TEXT("Pistol/MM_Pistol_Idle_ADS.MM_Pistol_Idle_ADS")), FVector(58.0, 16.0, -24.0), FVector(58.0, 0.0, -15.0), /*bLeftArm=*/ false });
	WeaponViews.Add(TEXT("Shotgun"), { Pose(TEXT("Shotgun/MM_Shotgun_Idle_ADS.MM_Shotgun_Idle_ADS")), FVector(58.0, 18.0, -28.0), FVector(50.0, 0.0, -14.0) });
	FDHFirstPersonWeaponView Sniper{ Pose(TEXT("Rifle/MM_Rifle_Idle_ADS.MM_Rifle_Idle_ADS")), FVector(58.0, 18.0, -28.0), FVector(52.0, 0.0, -18.0) };
	Sniper.ADSFieldOfView = 14.f;
	Sniper.bScope = true;
	WeaponViews.Add(TEXT("Sniper"), Sniper);
	DefaultView = { Pose(TEXT("Rifle/MM_Rifle_Idle_ADS.MM_Rifle_Idle_ADS")), FVector(58.0, 18.0, -28.0), FVector(52.0, 0.0, -18.0) };
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
		// A new weapon in hand comes up from below the view (not on the first build after spawning or switching view).
		if (bWeaponsChanged && !CopiedWeapons.IsEmpty() && !Weapons.IsEmpty())
		{
			Raise = 0.f;
		}
		DestroyCopies();
		BuildCopies(*Character);
	}

	USkeletalMeshComponent* Body = BodyCopy.Get();
	if (!Body)
	{
		return;
	}

	UpdateWeaponState(*Character, DeltaTime);
	View.FieldOfView = FMath::Lerp(FieldOfView, CurrentView.ADSFieldOfView > 0.f ? CurrentView.ADSFieldOfView : ADSFieldOfView, ADSAlpha);
	SetFirstPersonRendering(true);

	UMeshComponent* Weapon = WeaponCopy.Get();
	Body->SetVisibility(Weapon != nullptr && !(CurrentView.bScope && ADSAlpha > 0.8f), /*bPropagateToChildren=*/ true);
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
	// Weapon switch: tilted down and to the side below the view, swinging up with a little overshoot.
	Raise = FMath::FInterpConstantTo(Raise, 1.f, DeltaTime, 1.f / RaiseTime);
	const float Lowered = 1.f - FMath::InterpEaseOut(0.f, 1.f, Raise, 3.f) - 0.12f * FMath::Sin(Raise * PI);
	const FQuat Tilt = FQuat(ViewQuat.GetRightVector(), FMath::DegreesToRadians(RaisePitch * Lowered))
		* FQuat(ViewQuat.GetForwardVector(), FMath::DegreesToRadians(-RaiseRoll * Lowered));
	const FQuat BodyQuat = Tilt * Kick * Correction * Facing;

	FVector Offset = FMath::Lerp(CurrentView.HipOffset, CurrentView.ADSOffset, ADSAlpha);
	Offset.X -= RecoilBack * Recoil;
	Offset.Z -= RaiseDrop * Lowered;
	const FVector Target = View.Location + ViewQuat.RotateVector(Offset);
	Body->SetWorldLocationAndRotation(Target - BodyQuat.RotateVector(Grip * Body->GetComponentScale()), BodyQuat);
	ShowOnlyArms();
}

void UDHCameraMode_FirstPerson::ShowOnlyArms() const
{
	// The arms hang off the torso, which sits right at the camera in the aiming poses. Copy the pose, shrink the
	// torso to (almost) nothing and put the upper arms back where they were: their children follow, arms and hands
	// stay as posed, chest and shoulders vanish.
	UPoseableMeshComponent* Arms = ArmsCopy.Get();
	USkeletalMeshComponent* Body = BodyCopy.Get();
	if (!Arms || !Body)
	{
		return;
	}
	Arms->CopyPoseFromSkeletalComponent(Body);
	TArray<FTransform, TInlineAllocator<2>> ArmTransforms;
	for (const FName& Bone : ArmBones)
	{
		ArmTransforms.Add(Arms->GetBoneTransformByName(Bone, EBoneSpaces::ComponentSpace));
	}
	FTransform Torso = Arms->GetBoneTransformByName(TorsoBone, EBoneSpaces::ComponentSpace);
	Torso.SetScale3D(FVector(0.01f));
	Arms->SetBoneTransformByName(TorsoBone, Torso, EBoneSpaces::ComponentSpace);
	for (int32 Index = 0; Index < ArmBones.Num(); ++Index)
	{
		if (CurrentView.bLeftArm || !ArmBones[Index].ToString().EndsWith(TEXT("_l")))
		{
			Arms->SetBoneTransformByName(ArmBones[Index], ArmTransforms[Index], EBoneSpaces::ComponentSpace);
		}
	}
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
				// Skeletal parts get a first-person copy, except heads (the camera sits inside them).
				// Everything else (team bands, groom hair, heads) is only hidden for the owner.
				TArray<UMeshComponent*> Meshes;
				Part->GetComponents<UMeshComponent>(Meshes, /*bIncludeFromChildActors=*/ false);
				for (UMeshComponent* PartMesh : Meshes)
				{
					USkeletalMeshComponent* Skeletal = Cast<USkeletalMeshComponent>(PartMesh);
					if (Skeletal && !PartMesh->GetName().Contains(TEXT("Head")))
					{
						BodyParts.Add(Skeletal);
					}
					else if (Skeletal || !PartMesh->IsA<UStaticMeshComponent>())
					{
						HiddenInFirstPerson.Add(PartMesh);  // head, groom hair
					}
					else
					{
						Originals.Add(PartMesh);  // team bands
					}
				}
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

	// The pose runs on an invisible copy of the pawn's anim mesh (Lyra's mannequin skeleton with the weapon sockets
	// and the skeleton the poses are made for); the visible parts follow it by leader pose. Crowd characters'
	// MetaHuman skeleton has no weapon socket, so driving the pose on their own body twisted the arms.
	// Bodies that carry the weapon sockets themselves (City Sample player characters) keep driving the pose, the
	// offsets are tuned for them.
	const TArray<AActor*> Weapons = GetAttachedWeapons(Character);
	const FName WeaponSocket = Weapons.IsEmpty() ? NAME_None : Weapons[0]->GetRootComponent()->GetAttachSocketName();
	USkeletalMeshComponent* OwnBody = BodyParts[0];
	const bool bOwnBodyDrives = OwnBody->DoesSocketExist(WeaponSocket);
	USkeletalMesh* FirstPersonBody = OwnBody->GetSkeletalMeshAsset();
	UPoseableMeshComponent* Arms = nullptr;
	if (!bOwnBodyDrives)
	{
		// Crowd characters' clothes do not fit the poses' skeleton (jacket shoulders end up in the view):
		// first-person arms come from the City Sample player body of the same gender instead.
		const bool bFemale = FirstPersonBody && FirstPersonBody->GetName().StartsWith(TEXT("f_"));
		FirstPersonBody = LoadObject<USkeletalMesh>(nullptr, bFemale
			? TEXT("/Game/Character/Player/Female/Meshes/SKM_PlayerFemale_Body.SKM_PlayerFemale_Body")
			: TEXT("/Game/Character/Player/Male/Meshes/SKM_PlayerMale.SKM_PlayerMale"));
		for (USkeletalMeshComponent* Part : BodyParts)
		{
			Originals.Add(Part);  // hidden for the owner, no copy
		}
		BodyParts.Reset();

		// Posed by hand every frame (UpdateView): only arms and hands, the torso shrunk away.
		Arms = NewObject<UPoseableMeshComponent>(&Character, NAME_None, RF_Transient);
		Arms->SetSkinnedAssetAndUpdate(FirstPersonBody);
		for (const FName& Bone : HiddenBones)
		{
			Arms->HideBoneByName(Bone, PBO_None);
		}
		Arms->SetOnlyOwnerSee(true);
		Arms->SetCastShadow(false);
		Arms->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Arms->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	}
	USkeletalMeshComponent* Root = NewObject<USkeletalMeshComponent>(&Character, NAME_None, RF_Transient);
	// The poses are made for Lyra's mannequin: on another skeleton they would not play (arms in the rest pose).
	Root->SetSkeletalMeshAsset(bOwnBodyDrives ? FirstPersonBody : Mesh->GetSkeletalMeshAsset());
	Root->SetWorldScale3D(Mesh->GetComponentScale());
	Root->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	CurrentView = FindView(GetAttachedWeapons(Character));
	Root->AnimationData.AnimToPlay = CurrentView.Pose.LoadSynchronous();
	Root->AnimationData.bSavedLooping = true;
	Root->AnimationData.bSavedPlaying = true;
	Root->SetVisibility(false);
	Root->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	Root->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Root->RegisterComponent();
	Copies.Add(Root);
	if (Arms)
	{
		Arms->SetupAttachment(Root);
		Arms->RegisterComponent();
		Copies.Add(Arms);
		ArmsCopy = Arms;
	}

	for (USkeletalMeshComponent* Part : BodyParts)
	{
		USkeletalMeshComponent* Copy = NewObject<USkeletalMeshComponent>(&Character, NAME_None, RF_Transient);
		Copy->SetSkeletalMeshAsset(Part->GetSkeletalMeshAsset());
		Copy->SetLeaderPoseComponent(Root);
		for (const FName& Bone : HiddenBones)
		{
			Copy->HideBoneByName(Bone, PBO_None);
		}
		Copy->SetupAttachment(Root);
		MakeCopy(Part, Copy);
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
	HiddenInFirstPerson.Reset();
	Copies.Reset();
	CopiedWeapons.Reset();
	BodyCopy.Reset();
	ArmsCopy.Reset();
	WeaponCopy.Reset();
}

void UDHCameraMode_FirstPerson::SetOriginalsHidden(bool bHidden)
{
	// WorldSpaceRepresentation hides the primitive from its owner's view but keeps its shadow and
	// leaves it untouched for every other viewer.
	const EFirstPersonPrimitiveType Type = bHidden ? EFirstPersonPrimitiveType::WorldSpaceRepresentation : EFirstPersonPrimitiveType::None;
	for (const TWeakObjectPtr<UMeshComponent>& Hidden : HiddenInFirstPerson)
	{
		if (Hidden.IsValid())
		{
			Hidden->SetHiddenInGame(bHidden);  // cosmetic parts are local to this machine, other players are not affected
		}
	}
	for (const TWeakObjectPtr<UMeshComponent>& Original : Originals)
	{
		if (Original.IsValid())
		{
			Original->SetFirstPersonPrimitiveType(Type);
		}
	}
}

const FDHFirstPersonWeaponView& UDHCameraMode_FirstPerson::FindView(const TArray<AActor*>& Weapons) const
{
	for (const AActor* Weapon : Weapons)
	{
		const FString ClassName = Weapon->GetClass()->GetName();
		for (const TPair<FString, FDHFirstPersonWeaponView>& Entry : WeaponViews)
		{
			if (ClassName.Contains(Entry.Key))
			{
				return Entry.Value;
			}
		}
	}
	return DefaultView;
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
