#include "DHPlayerRulesComponent.h"

#include "AbilitySystemComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "UIExtensionSystem.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "AbilitySystemGlobals.h"
#include "Camera/LyraCameraComponent.h"
#include "Camera/LyraCameraMode.h"
#include "DHAbilities.h"
#include "DHAbilityHud.h"
#include "DHStreakHud.h"
#include "DHControllerScreen.h"
#include "DHDevScreen.h"
#include "DHProgression.h"
#include "DHGameRulesComponent.h"
#include "DHFreeForAllScoring.h"
#include "DHGunGameHud.h"
#include "DHKillstreaks.h"
#include "DHMatchEndScreen.h"
#include "DHMinimap.h"
#include "CommonInputSubsystem.h"
#include "DHMovement.h"
#include "Equipment/LyraEquipmentManagerComponent.h"
#include "Weapons/LyraWeaponInstance.h"
#include "DHPlayerChoicesComponent.h"
#include "DHScopeOverlay.h"
#include "DHSoundCue.h"
#include "DHWeaponAccess.h"
#include "Components/SkinnedMeshComponent.h"
#include "GroomComponent.h"
#include "AnimationRuntime.h"
#include "Curves/CurveFloat.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "DHObjectiveHud.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Inventory/LyraInventoryItemDefinition.h"
#include "Inventory/LyraInventoryItemInstance.h"
#include "Inventory/LyraInventoryManagerComponent.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DHPlayerRulesComponent)


bool UDHPlayerRulesComponent::bPreferFirstPerson = false;

namespace
{
	constexpr float RetryInterval = 0.25f;
	constexpr int32 MaxRetries = 40;

	// Lyra's camera and quick bar classes are not exported from LyraGame, so they are found by name.
	UActorComponent* FindComponentByClassName(const AActor* Actor, FName ClassName)
	{
		if (!Actor)
		{
			return nullptr;
		}
		for (UActorComponent* Component : Actor->GetComponents())
		{
			for (const UClass* Class = Component ? Component->GetClass() : nullptr; Class; Class = Class->GetSuperClass())
			{
				if (Class->GetFName() == ClassName)
				{
					return Component;
				}
			}
		}
		return nullptr;
	}

	bool AddToFirstFreeQuickBarSlot(UActorComponent* QuickBar, ULyraInventoryItemInstance* Item)
	{
		FArrayProperty* SlotsProperty = FindFProperty<FArrayProperty>(QuickBar->GetClass(), TEXT("Slots"));
		UFunction* AddItemToSlot = QuickBar->FindFunction(TEXT("AddItemToSlot"));
		if (!SlotsProperty || !AddItemToSlot)
		{
			return false;
		}

		FScriptArrayHelper Slots(SlotsProperty, SlotsProperty->ContainerPtrToValuePtr<void>(QuickBar));
		const FObjectPropertyBase* SlotProperty = CastField<FObjectPropertyBase>(SlotsProperty->Inner);
		for (int32 Index = 0; SlotProperty && Index < Slots.Num(); ++Index)
		{
			if (!SlotProperty->GetObjectPropertyValue(Slots.GetRawPtr(Index)))
			{
				struct
				{
					int32 SlotIndex;
					ULyraInventoryItemInstance* Item;
				} Params{ Index, Item };
				QuickBar->ProcessEvent(AddItemToSlot, &Params);
				return true;
			}
		}
		return false;
	}

	/** Takes Lyra's starting weapons that are not part of the loadout out of the quick bar and the inventory. */
	void RemoveQuickBarItemsExcept(UActorComponent* QuickBar, ULyraInventoryManagerComponent* Inventory, const TArray<UClass*>& Keep)
	{
		FArrayProperty* SlotsProperty = FindFProperty<FArrayProperty>(QuickBar->GetClass(), TEXT("Slots"));
		UFunction* RemoveItemFromSlot = QuickBar->FindFunction(TEXT("RemoveItemFromSlot"));
		const FObjectPropertyBase* SlotProperty = SlotsProperty ? CastField<FObjectPropertyBase>(SlotsProperty->Inner) : nullptr;
		if (!SlotProperty || !RemoveItemFromSlot)
		{
			return;
		}

		FScriptArrayHelper Slots(SlotsProperty, SlotsProperty->ContainerPtrToValuePtr<void>(QuickBar));
		for (int32 Index = 0; Index < Slots.Num(); ++Index)
		{
			ULyraInventoryItemInstance* Item = Cast<ULyraInventoryItemInstance>(SlotProperty->GetObjectPropertyValue(Slots.GetRawPtr(Index)));
			if (Item && !Keep.Contains(Item->GetItemDef().Get()))
			{
				struct
				{
					int32 SlotIndex;
					ULyraInventoryItemInstance* ReturnValue;
				} Params{ Index, nullptr };
				QuickBar->ProcessEvent(RemoveItemFromSlot, &Params);
				Inventory->RemoveItemInstance(Item);
			}
		}
	}

	/** Quick bar slot holding an item of this definition, INDEX_NONE if none. */
	int32 FindQuickBarSlot(UActorComponent* QuickBar, const UClass* ItemDef)
	{
		FArrayProperty* SlotsProperty = FindFProperty<FArrayProperty>(QuickBar->GetClass(), TEXT("Slots"));
		const FObjectPropertyBase* SlotProperty = SlotsProperty ? CastField<FObjectPropertyBase>(SlotsProperty->Inner) : nullptr;
		if (!SlotProperty)
		{
			return INDEX_NONE;
		}
		FScriptArrayHelper Slots(SlotsProperty, SlotsProperty->ContainerPtrToValuePtr<void>(QuickBar));
		for (int32 Index = 0; Index < Slots.Num(); ++Index)
		{
			const ULyraInventoryItemInstance* Item = Cast<ULyraInventoryItemInstance>(SlotProperty->GetObjectPropertyValue(Slots.GetRawPtr(Index)));
			if (Item && Item->GetItemDef().Get() == ItemDef)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	void SetActiveQuickBarSlot(UActorComponent* QuickBar, int32 Index)
	{
		if (UFunction* SetActiveSlotIndex = QuickBar->FindFunction(TEXT("SetActiveSlotIndex")))
		{
			QuickBar->ProcessEvent(SetActiveSlotIndex, &Index);
		}
	}
}

UDHPlayerRulesComponent::UDHPlayerRulesComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ThirdPersonMode = TSoftClassPtr<ULyraCameraMode>(FSoftObjectPath(TEXT("/Game/Characters/Cameras/CM_ThirdPerson.CM_ThirdPerson_C")));
	ThirdPersonAimMode = TSoftClassPtr<ULyraCameraMode>(FSoftObjectPath(TEXT("/ShooterCore/Camera/CM_ThirdPersonADS.CM_ThirdPersonADS_C")));
	DeathMode = TSoftClassPtr<ULyraCameraMode>(FSoftObjectPath(TEXT("/Game/Characters/Cameras/CM_ThirdPerson_Death.CM_ThirdPerson_Death_C")));
	FirstPersonMode = TSoftClassPtr<ULyraCameraMode>(FSoftObjectPath(TEXT("/DowntownCore/Camera/CM_DH_FirstPerson.CM_DH_FirstPerson_C")));
	ToggleAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/DowntownCore/Input/IA_DH_TogglePerspective.IA_DH_TogglePerspective")));
	InputMapping = TSoftObjectPtr<UInputMappingContext>(FSoftObjectPath(TEXT("/DowntownCore/Input/IMC_DH_Perspective.IMC_DH_Perspective")));
	ScoreboardAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/ShooterCore/Input/Actions/IA_ShowScoreboard.IA_ShowScoreboard")));
	AbilityAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Input/Actions/IA_Ability_Dash.IA_Ability_Dash")));
	Ability2Action = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/DowntownCore/Input/IA_DH_Ability2.IA_DH_Ability2")));
	SprintAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/DowntownCore/Input/IA_DH_Sprint.IA_DH_Sprint")));
}

void UDHPlayerRulesComponent::BeginPlay()
{
	Super::BeginPlay();

	// Controller, Lyra's initial inventory, input component and camera binding all arrive after BeginPlay, and
	// Lyra may set up input and the camera delegate again later (e.g. when loading is slow). The loadout retries
	// for a while; the local player's camera and input are checked for as long as the pawn lives.
	GetWorld()->GetTimerManager().SetTimer(RetryTimer, [this]()
	{
		const APawn* Pawn = GetPawn<APawn>();
		const bool bLocalPlayer = Pawn && Pawn->IsLocallyControlled() && Pawn->IsPlayerControlled();
		if (Pawn && Pawn->HasAuthority() && !bLoadoutGiven && Retries < MaxRetries)
		{
			GiveLoadout();
		}
		if (bLocalPlayer)
		{
			SetUpLocalPlayer();
		}
		++Retries;
		if (!bLocalPlayer && (bLoadoutGiven || Retries >= MaxRetries))
		{
			GetWorld()->GetTimerManager().ClearTimer(RetryTimer);
		}
	}, RetryInterval, true);

	// Killstreak hunter mode, the infected and adrenaline: faster; attachments on the equipped weapon. On the server and
	// the owning client alike (movement corrections, client-side spread). Cloak everywhere.
	GetWorld()->GetTimerManager().SetTimer(HunterTimer, [this]()
	{
		ApplyAttachments();
		AttachHairToHead();
		DHAbilities::UpdateCloak(GetPawn<APawn>(), CloakState);
		ACharacter* Character = GetPawn<ACharacter>();
		if (Character && Character->HasAuthority())
		{
			PrepareLyraDash();
			ApplyDevCheats();
			UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Character);
			if (ASC && FireSource.Get() != ASC)
			{
				if (FireSource.IsValid())
				{
					FireSource->AbilityActivatedCallbacks.Remove(FireHandle);
				}
				FireSource = ASC;
				FireHandle = ASC->AbilityActivatedCallbacks.AddUObject(this, &ThisClass::HandleAbilityActivated);
			}
		}
		if (Character && !Character->HasAuthority() && Character->IsLocallyControlled())
		{
			PrepareLyraDash();  // its widget, on the owning client
		}
		UpdateMoveSpeed();
		const UObject* Weapon = DHWeaponAccess::ActiveWeapon(Character);
		if (Character && Character->IsLocallyControlled() && Character->IsPlayerControlled() && Weapon != LoggedWeapon.Get())
		{
			LoggedWeapon = Weapon;
			UE_LOG(LogDHRules, Log, TEXT("%s: weapon in hand %s"), *GetNameSafe(Character), *GetNameSafe(Weapon ? Weapon->GetClass() : nullptr));
		}
	}, 0.25f, true);
	GetWorld()->GetTimerManager().SetTimer(MoveAnimTimer, this, &ThisClass::UpdateMoveAnims, 0.05f, true);
}

void UDHPlayerRulesComponent::UpdateMoveAnims()
{
	// Every machine, every pawn: Lyra has no sprint or slide animations; ours (Tools/Python/make_moves.py) play as a
	// full-body montage while the pawn runs at sprint speed or slides crouched. Read from the movement, so nothing has to
	// be replicated for other players' pawns.
	ACharacter* Character = GetPawn<ACharacter>();
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	UAnimInstance* Anim = Movement && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
	if (!Anim)
	{
		return;
	}
	static const FGameplayTag DeathTag = FGameplayTag::RequestGameplayTag(TEXT("Status.Death"));
	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Character);
	const float Speed = Character->GetVelocity().Size2D();
	const float Walk = CastChecked<UCharacterMovementComponent>(Movement->GetArchetype())->MaxWalkSpeed;
	const bool bForward = FVector::DotProduct(Character->GetVelocity().GetSafeNormal2D(), Character->GetActorForwardVector()) > 0.7f;
	uint8 Wanted = 0;
	if (Movement->IsMovingOnGround() && !(ASC && ASC->HasMatchingGameplayTag(DeathTag)))
	{
		if (Movement->IsCrouching() && Speed > DHMovement::SlideMinSpeed * 0.8f)
		{
			Wanted = 2;
		}
		else if (!Movement->IsCrouching() && bForward && Speed > Walk * 1.18f)
		{
			Wanted = 1;
		}
	}
	const UObject* Weapon = DHWeaponAccess::ActiveWeapon(Character);
	const bool bPistol = Weapon && (Weapon->GetClass()->GetName().Contains(TEXT("Pistol")) || Weapon->GetClass()->GetName().Contains(TEXT("M1911")));
	if (Wanted != MoveAnim)
	{
		if (MoveMontage.IsValid())
		{
			Anim->Montage_Stop(Wanted == 2 ? 0.12f : 0.25f, MoveMontage.Get());
		}
		MoveMontage = nullptr;
		MoveAnim = Wanted;
		if (Wanted != 0)
		{
			const FString Path = FString::Printf(TEXT("/DowntownCore/Animations/A_DH_%s_%s.A_DH_%s_%s"),
				Wanted == 1 ? TEXT("Sprint") : TEXT("Slide"), bPistol ? TEXT("Pistol") : TEXT("Rifle"),
				Wanted == 1 ? TEXT("Sprint") : TEXT("Slide"), bPistol ? TEXT("Pistol") : TEXT("Rifle"));
			if (UAnimSequenceBase* Sequence = LoadObject<UAnimSequenceBase>(nullptr, *Path))
			{
				MoveMontage = Anim->PlaySlotAnimationAsDynamicMontage(Sequence, TEXT("FullBody"), Wanted == 2 ? 0.1f : 0.2f, 0.25f, 1.f, 1000);
			}
			UE_LOG(LogDHRules, Verbose, TEXT("%s: %s montage %s"), *GetNameSafe(Character), *Path, MoveMontage.IsValid() ? TEXT("playing") : TEXT("failed"));
		}
	}
	if (MoveAnim == 1 && MoveMontage.IsValid())
	{
		// Feet in step with the speed.
		Anim->Montage_SetPlayRate(MoveMontage.Get(), FMath::Clamp(Speed / Walk, 1.f, 1.7f));
	}

	// Other players' steps carry further than Lyra's own (which stay): you hear from where someone is coming, louder
	// when they sprint, muffled behind walls; crouching is silent.
	if (!Character->IsLocallyControlled() && GetNetMode() != NM_DedicatedServer && Movement->IsMovingOnGround() && !Movement->IsCrouching() && Speed > 150.f)
	{
		StepDistance += Speed * 0.05f;
		const float Stride = MoveAnim == 1 ? 200.f : 155.f;
		if (StepDistance >= Stride)
		{
			StepDistance = 0.f;
			static USoundAttenuation* Steps = []()
			{
				USoundAttenuation* Attenuation = NewObject<USoundAttenuation>(GetTransientPackage(), TEXT("DHFootstepAttenuation"));
				Attenuation->AddToRoot();
				Attenuation->Attenuation.bAttenuate = true;
				Attenuation->Attenuation.bSpatialize = true;
				Attenuation->Attenuation.AttenuationShapeExtents = FVector(300.f);
				Attenuation->Attenuation.FalloffDistance = 3200.f;
				Attenuation->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
				Attenuation->Attenuation.bAttenuateWithLPF = true;
				Attenuation->Attenuation.LPFRadiusMin = 800.f;
				Attenuation->Attenuation.LPFRadiusMax = 3200.f;
				Attenuation->Attenuation.bEnableOcclusion = true;
				Attenuation->Attenuation.OcclusionLowPassFilterFrequency = 900.f;
				Attenuation->Attenuation.OcclusionVolumeAttenuation = 0.5f;
				return Attenuation;
			}();
			if (USoundBase* Step = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/MetaSounds/sfx_Character_FS_Concrete_nl_meta.sfx_Character_FS_Concrete_nl_meta")))
			{
				UGameplayStatics::PlaySoundAtLocation(this, Step, Character->GetActorLocation() - FVector(0.f, 0.f, 85.f), FRotator::ZeroRotator,
					MoveAnim == 1 ? 1.4f : 1.f, FMath::FRandRange(0.95f, 1.05f), 0.f, Steps);
			}
		}
	}
}

void UDHPlayerRulesComponent::UpdateMoveSpeed()
{
	ACharacter* Character = GetPawn<ACharacter>();
	UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Movement)
	{
		return;
	}
	const float Base = CastChecked<UCharacterMovementComponent>(Movement->GetArchetype())->MaxWalkSpeed;
	const UDHInfectedScoring* Infected = Cast<UDHInfectedScoring>(UDHFreeForAllScoring::Find(this));
	const UDHPlayerChoicesComponent* Choices = Character->GetController() ? Character->GetController()->FindComponentByClass<UDHPlayerChoicesComponent>() : nullptr;
	const bool bAdrenaline = DHAbilities::HasAdrenaline(Character->GetPlayerState())
		|| (Choices && Character->IsLocallyControlled() && Choices->GetLocalAdrenalineUntil() > GetWorld()->GetTimeSeconds());
	Movement->MaxWalkSpeed = Base * (DHKillstreaks::IsHunting(Character->GetPlayerState()) ? DHKillstreaks::HunterSpeed : 1.f)
		* (Infected && Infected->IsInfected(Character->GetPlayerState()) ? UDHInfectedScoring::InfectedSpeed : 1.f)
		* (bAdrenaline ? DHAbilities::AdrenalineSpeed : 1.f)
		* (bSprinting ? DHMovement::SprintSpeed : 1.f);
}

void UDHPlayerRulesComponent::ApplyDevCheats()
{
	// Only the host's own pawn: the switches are this machine's (dev screen).
	const APawn* Pawn = GetPawn<APawn>();
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn);
	if (!ASC || !Pawn->IsLocallyControlled() || !Pawn->IsPlayerControlled())
	{
		return;
	}
	// Lyra's health set ignores damage while the owner has Cheat.GodMode.
	static const FGameplayTag GodTag = FGameplayTag::RequestGameplayTag(TEXT("Cheat.GodMode"));
	const bool bGod = FDHProgression::DevFlag(TEXT("God"));
	if (bGod != bDevGod)
	{
		bDevGod = bGod;
		if (bGod)
		{
			ASC->AddLooseGameplayTag(GodTag);
		}
		else
		{
			ASC->RemoveLooseGameplayTag(GodTag);
		}
	}
	if (FDHProgression::DevFlag(TEXT("InfiniteAmmo")))
	{
		static const FGameplayTag MagazineSize = FGameplayTag::RequestGameplayTag(TEXT("Lyra.ShooterGame.Weapon.MagazineSize"));
		static const FGameplayTag MagazineAmmo = FGameplayTag::RequestGameplayTag(TEXT("Lyra.ShooterGame.Weapon.MagazineAmmo"));
		static const FGameplayTag SpareAmmo = FGameplayTag::RequestGameplayTag(TEXT("Lyra.ShooterGame.Weapon.SpareAmmo"));
		if (ULyraInventoryItemInstance* Item = DHWeaponAccess::ItemOf(DHWeaponAccess::ActiveWeapon(Pawn)))
		{
			const int32 Size = DHWeaponAccess::ItemStat(Item, MagazineSize);
			DHWeaponAccess::AddItemStat(Item, MagazineAmmo, Size - DHWeaponAccess::ItemStat(Item, MagazineAmmo));
			DHWeaponAccess::AddItemStat(Item, SpareAmmo, FMath::Max(0, Size * 3 - DHWeaponAccess::ItemStat(Item, SpareAmmo)));
		}
	}
}

void UDHPlayerRulesComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorld()->GetTimerManager().ClearTimer(RetryTimer);
	GetWorld()->GetTimerManager().ClearTimer(HunterTimer);
	GetWorld()->GetTimerManager().ClearTimer(SprintTimer);
	GetWorld()->GetTimerManager().ClearTimer(MoveAnimTimer);
	if (FireSource.IsValid())
	{
		FireSource->AbilityActivatedCallbacks.Remove(FireHandle);
	}
	if (GunGameHud)
	{
		GunGameHud->RemoveFromParent();
		GunGameHud = nullptr;
	}
	HideScoreboard();
	if (Minimap)
	{
		Minimap->RemoveFromParent();
		Minimap = nullptr;
	}
	if (ScopeOverlay)
	{
		ScopeOverlay->RemoveFromParent();
		ScopeOverlay = nullptr;
	}
	if (AbilityHud)
	{
		AbilityHud->RemoveFromParent();
		AbilityHud = nullptr;
	}
	if (StreakHud)
	{
		StreakHud->RemoveFromParent();
		StreakHud = nullptr;
	}

	const APawn* Pawn = GetPawn<APawn>();
	const APlayerController* PC = Pawn ? Pawn->GetController<APlayerController>() : nullptr;
	UEnhancedInputLocalPlayerSubsystem* Input = PC && PC->GetLocalPlayer() ? PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (Input && bMappingAdded && InputMapping.Get())
	{
		Input->RemoveMappingContext(InputMapping.Get());
	}

	Super::EndPlay(EndPlayReason);
}

void UDHPlayerRulesComponent::GiveLoadout()
{
	const UDHGameRulesComponent* Rules = UDHGameRulesComponent::Find(this);
	const AController* Controller = GetPawn<APawn>()->GetController();
	ULyraInventoryManagerComponent* Inventory = Controller ? Controller->FindComponentByClass<ULyraInventoryManagerComponent>() : nullptr;
	UActorComponent* QuickBar = FindComponentByClassName(Controller, TEXT("LyraQuickBarComponent"));
	if (!Rules)
	{
		bLoadoutGiven = true;
		return;
	}
	// Wait for Lyra's own starting weapon: it is replaced by the loadout unless the loadout contains it.
	if (!Inventory || !QuickBar || Inventory->GetAllItems().IsEmpty())
	{
		return;
	}

	const TArray<UClass*> Items = Rules->GetLoadoutItems(Controller);
	if (!Items.IsEmpty())
	{
		RemoveQuickBarItemsExcept(QuickBar, Inventory, Items);
	}
	for (UClass* ItemClass : Items)
	{
		const TSubclassOf<ULyraInventoryItemDefinition> ItemDef(ItemClass);
		if (!Inventory->FindFirstItemStackByDefinition(ItemDef))
		{
			const bool bSlotted = AddToFirstFreeQuickBarSlot(QuickBar, Inventory->AddItemDefinition(ItemDef));
			UE_LOG(LogDHRules, Log, TEXT("%s: gave %s (quick bar: %s)"), *GetNameSafe(Controller), *ItemClass->GetName(), bSlotted ? TEXT("yes") : TEXT("full"));
		}
	}
	// Hardcore: little health (Lyra's health set is not exported: attributes by name).
	if (Rules->IsHardcore())
	{
		UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetPawn<APawn>());
		const UClass* HealthSet = FindObject<UClass>(nullptr, TEXT("/Script/LyraGame.LyraHealthSet"));
		if (ASC && HealthSet)
		{
			ASC->SetNumericAttributeBase(FGameplayAttribute(FindFProperty<FProperty>(HealthSet, TEXT("MaxHealth"))), UDHGameRulesComponent::HardcoreHealth);
			ASC->SetNumericAttributeBase(FGameplayAttribute(FindFProperty<FProperty>(HealthSet, TEXT("Health"))), UDHGameRulesComponent::HardcoreHealth);
		}
	}
	// Chosen attachments, once per item (items stay in the inventory across respawns).
	if (const UDHPlayerChoicesComponent* Choices = Controller->FindComponentByClass<UDHPlayerChoicesComponent>())
	{
		for (ULyraInventoryItemInstance* Item : Inventory->GetAllItems())
		{
			const FName Attachment = Choices->GetAttachment(Rules->GetWeaponName(Item->GetItemDef().Get()));
			const FGameplayTag Tag = Attachment.IsNone() ? FGameplayTag() : FGameplayTag::RequestGameplayTag(*(TEXT("DH.Attachment.") + Attachment.ToString()), false);
			if (!Tag.IsValid() || DHWeaponAccess::ItemStat(Item, Tag) > 0)
			{
				continue;
			}
			DHWeaponAccess::AddItemStat(Item, Tag, 1);
			UE_LOG(LogDHRules, Log, TEXT("%s: %s with %s"), *GetNameSafe(Controller), *GetNameSafe(Item->GetItemDef().Get()), *Attachment.ToString());
			if (Attachment == TEXT("ExtMag"))
			{
				// Half a magazine more, loaded.
				const int32 Extra = DHWeaponAccess::ItemStat(Item, FGameplayTag::RequestGameplayTag(TEXT("Lyra.ShooterGame.Weapon.MagazineSize"))) / 2;
				DHWeaponAccess::AddItemStat(Item, FGameplayTag::RequestGameplayTag(TEXT("Lyra.ShooterGame.Weapon.MagazineSize")), Extra);
				DHWeaponAccess::AddItemStat(Item, FGameplayTag::RequestGameplayTag(TEXT("Lyra.ShooterGame.Weapon.MagazineAmmo")), Extra);
				UE_LOG(LogDHRules, Log, TEXT("%s: magazine %d"), *GetNameSafe(Item->GetItemDef().Get()), DHWeaponAccess::ItemStat(Item, FGameplayTag::RequestGameplayTag(TEXT("Lyra.ShooterGame.Weapon.MagazineSize"))));
			}
		}
	}
	// Start with the primary weapon (Lyra's starting pistol may sit in slot 0 before it). The quick bar lives on the
	// controller and may still count that slot as active from the previous pawn (Lyra's auto respawn resets it, round
	// modes have none): switching away and back equips the weapon on this pawn.
	const int32 Primary = Items.IsEmpty() ? 0 : FMath::Max(0, FindQuickBarSlot(QuickBar, Items[0]));
	if (const FIntProperty* Active = FindFProperty<FIntProperty>(QuickBar->GetClass(), TEXT("ActiveSlotIndex")))
	{
		if (Active->GetPropertyValue_InContainer(QuickBar) == Primary)
		{
			SetActiveQuickBarSlot(QuickBar, Primary == 0 ? 1 : 0);
		}
	}
	SetActiveQuickBarSlot(QuickBar, Primary);
	bLoadoutGiven = true;
}

void UDHPlayerRulesComponent::SetUpLocalPlayer()
{
	APawn* Pawn = GetPawn<APawn>();

	// Lyra's hero component binds its own camera mode selection once the pawn is initialized; take it over.
	if (ULyraCameraComponent* Camera = static_cast<ULyraCameraComponent*>(FindComponentByClassName(Pawn, TEXT("LyraCameraComponent"))))
	{
		if (!Camera->DetermineCameraModeDelegate.IsBoundToObject(this))
		{
			Camera->DetermineCameraModeDelegate.BindUObject(this, &ThisClass::DetermineCameraMode);
		}
	}

	UEnhancedInputComponent* InputComponent = Cast<UEnhancedInputComponent>(Pawn->InputComponent);
	const APlayerController* PC = Pawn->GetController<APlayerController>();
	UEnhancedInputLocalPlayerSubsystem* Input = PC && PC->GetLocalPlayer() ? PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (InputComponent && BoundInputComponent.Get() != InputComponent)
	{
		if (UInputAction* Action = ToggleAction.LoadSynchronous())
		{
			InputComponent->BindAction(Action, ETriggerEvent::Started, this, &ThisClass::TogglePerspective);
		}
		if (UInputAction* Action = ScoreboardAction.LoadSynchronous())
		{
			InputComponent->BindAction(Action, ETriggerEvent::Started, this, &ThisClass::ShowScoreboard);
			InputComponent->BindAction(Action, ETriggerEvent::Completed, this, &ThisClass::HideScoreboard);
		}
		// Lyra's own crouch and jump stay bound; slide and wall jump only add to them.
		if (UInputAction* Action = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/Actions/IA_Crouch.IA_Crouch")))
		{
			InputComponent->BindAction(Action, ETriggerEvent::Started, this, &ThisClass::TrySlide);
		}
		if (UInputAction* Action = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/Actions/IA_Jump.IA_Jump")))
		{
			InputComponent->BindAction(Action, ETriggerEvent::Started, this, &ThisClass::TryWallJump);
		}
		if (UInputAction* Action = AbilityAction.LoadSynchronous())
		{
			InputComponent->BindAction(Action, ETriggerEvent::Started, this, &ThisClass::UseAbility1);
		}
		if (UInputAction* Action = Ability2Action.LoadSynchronous())
		{
			InputComponent->BindAction(Action, ETriggerEvent::Started, this, &ThisClass::UseAbility2);
		}
		// Killstreaks: 4 / 5 / 6 for each, D-pad left / down for the selected one (Tools/Python/setup_input.py).
		using FHandler = void (ThisClass::*)();
		const TPair<const TCHAR*, FHandler> StreakInputs[] = {
			{ TEXT("/DowntownCore/Input/IA_DH_Streak1.IA_DH_Streak1"), &ThisClass::UseStreak1 },
			{ TEXT("/DowntownCore/Input/IA_DH_Streak2.IA_DH_Streak2"), &ThisClass::UseStreak2 },
			{ TEXT("/DowntownCore/Input/IA_DH_Streak3.IA_DH_Streak3"), &ThisClass::UseStreak3 },
			{ TEXT("/DowntownCore/Input/IA_DH_StreakUse.IA_DH_StreakUse"), &ThisClass::UseSelectedStreak },
			{ TEXT("/DowntownCore/Input/IA_DH_StreakNext.IA_DH_StreakNext"), &ThisClass::SelectNextStreak },
		};
		for (const TPair<const TCHAR*, FHandler>& StreakInput : StreakInputs)
		{
			if (UInputAction* Action = LoadObject<UInputAction>(nullptr, StreakInput.Key))
			{
				InputComponent->BindAction(Action, ETriggerEvent::Started, this, StreakInput.Value);
			}
		}
		if (UInputAction* Action = SprintAction.LoadSynchronous())
		{
			InputComponent->BindAction(Action, ETriggerEvent::Started, this, &ThisClass::HandleSprintPressed);
			InputComponent->BindAction(Action, ETriggerEvent::Completed, this, &ThisClass::HandleSprintReleased);
		}
		BoundInputComponent = InputComponent;
	}
	const UDHGameRulesComponent* Rules = UDHGameRulesComponent::Find(this);
	if (!ScopeOverlay && PC)
	{
		ScopeOverlay = CreateWidget<UDHScopeOverlay>(const_cast<APlayerController*>(PC));
		ScopeOverlay->AddToViewport(4);
	}
	if (!AbilityHud && PC)
	{
		AbilityHud = CreateWidget<UDHAbilityHud>(const_cast<APlayerController*>(PC));
		AbilityHud->AddToViewport(5);
	}
	if (!StreakHud && PC)
	{
		StreakHud = CreateWidget<UDHStreakHud>(const_cast<APlayerController*>(PC));
		StreakHud->AddToViewport(5);
	}
	if (!Minimap && PC && !(Rules && Rules->IsHardcore()))
	{
		Minimap = CreateWidget<UDHMinimap>(const_cast<APlayerController*>(PC));
		Minimap->AddToViewport(5);
	}
	if (!GunGameHud && PC)
	{
		// Mode overlays: Gun Game progress, Search & Destroy objective.
		TSubclassOf<UUserWidget> Overlay = Rules && !Rules->GetGunGame().IsEmpty() ? UDHGunGameHud::StaticClass()
			: Cast<UDHSearchAndDestroyScoring>(UDHFreeForAllScoring::Find(this)) ? UDHObjectiveHud::StaticClass() : nullptr;
		if (Overlay)
		{
			GunGameHud = CreateWidget<UUserWidget>(const_cast<APlayerController*>(PC), Overlay);
			GunGameHud->AddToViewport();
		}
	}

	UInputMappingContext* Mapping = InputMapping.LoadSynchronous();
	if (Input && Mapping && !Input->HasMappingContext(Mapping))
	{
		// Above ShooterCore's contexts: our gamepad keys (melee on the right stick, RB for the second ability) win.
		Input->AddMappingContext(Mapping, 10);
		bMappingAdded = true;
		UDHControllerScreen::ApplyBindings(PC->GetLocalPlayer());
	}
	// Developer mode (fg.txt): Lyra's cheats (God, UnlimitedHealth, ...) for the local player; real on the host.
	// The dev screen hotkey also when a match was started without the main menu.
	UDHDevScreen::InstallHotkey();
	if (PC && !PC->CheatManager && FDHProgression::IsDevMode())
	{
		const_cast<APlayerController*>(PC)->AddCheats(true);
	}
}

bool UDHPlayerRulesComponent::HasScope() const
{
	const UObject* Weapon = DHWeaponAccess::ActiveWeapon(GetPawn<APawn>());
	return Weapon && Weapon->GetClass()->GetName().Contains(TEXT("Sniper"));
}

void UDHPlayerRulesComponent::UseStreak(int32 Slot)
{
	if (StreakHud)
	{
		StreakHud->Activate(Slot);
	}
}

void UDHPlayerRulesComponent::SelectNextStreak()
{
	if (StreakHud)
	{
		StreakHud->SelectNext();
	}
}

void UDHPlayerRulesComponent::AttachHairToHead()
{
	// City Sample's grooms are bound to the head mesh, but the binding does not deform them here (skin cache on or
	// not): the hair stays in the head's rest pose and floats away whenever an animation moves the head (running,
	// dying). Hang every groom rigidly on the head bone instead, where it sits in the rest pose, without the binding.
	const APawn* Pawn = GetPawn<APawn>();
	if (!Pawn || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	static const FName HeadBone(TEXT("head"));
	TArray<AActor*> Parts;
	Pawn->GetAttachedActors(Parts, /*bResetArray=*/ true, /*bRecursivelyIncludeAttachedActors=*/ true);
	for (const AActor* Part : Parts)
	{
		TInlineComponentArray<UGroomComponent*> Grooms(Part);
		for (UGroomComponent* Groom : Grooms)
		{
			USkinnedMeshComponent* Skin = Cast<USkinnedMeshComponent>(Groom->GetAttachParent());
			if (!Skin || !Skin->GetSkinnedAsset() || Groom->GetAttachSocketName() == HeadBone)
			{
				continue;
			}
			const FReferenceSkeleton& Skeleton = Skin->GetSkinnedAsset()->GetRefSkeleton();
			const int32 Bone = Skeleton.FindBoneIndex(HeadBone);
			if (Bone == INDEX_NONE)
			{
				continue;
			}
			const FTransform BoneRestPose = FAnimationRuntime::GetComponentSpaceTransformRefPose(Skeleton, Bone);
			const FTransform Relative = Groom->GetRelativeTransform() * BoneRestPose.Inverse();
			Groom->SetBindingAsset(nullptr);
			Groom->AttachToComponent(Skin, FAttachmentTransformRules::KeepRelativeTransform, HeadBone);
			Groom->SetRelativeTransform(Relative);
		}
	}
}

void UDHPlayerRulesComponent::ApplyAttachments()
{
	// Lyra makes a new weapon instance on every equip: modify each one once.
	UObject* Weapon = DHWeaponAccess::ActiveWeapon(GetPawn<APawn>());
	if (!Weapon || Weapon == ModifiedWeapon.Get())
	{
		return;
	}
	ULyraInventoryItemInstance* Item = DHWeaponAccess::ItemOf(Weapon);
	if (!Item)
	{
		return;
	}
	ModifiedWeapon = Weapon;
	auto Scale = [Weapon](const TCHAR* Name, float Factor)
	{
		if (FFloatProperty* Property = FindFProperty<FFloatProperty>(Weapon->GetClass(), Name))
		{
			Property->SetPropertyValue_InContainer(Weapon, Property->GetPropertyValue_InContainer(Weapon) * Factor);
		}
	};
	if (DHWeaponAccess::ItemStat(Item, FGameplayTag::RequestGameplayTag(TEXT("DH.Attachment.Laser"))) > 0)
	{
		UE_LOG(LogDHRules, Verbose, TEXT("%s: laser on %s"), *GetNameSafe(GetPawn<APawn>()), *GetNameSafe(Weapon));
		// Shots gather closer to the centre of the spread cone, and a tighter cone when aiming.
		Scale(TEXT("SpreadExponent"), 2.5f);
		Scale(TEXT("SpreadAngleMultiplier_Aiming"), 0.75f);
	}
	if (DHWeaponAccess::ItemStat(Item, FGameplayTag::RequestGameplayTag(TEXT("DH.Attachment.Barrel"))) > 0)
	{
		// Damage falls off 60 % further out.
		if (FStructProperty* Property = FindFProperty<FStructProperty>(Weapon->GetClass(), TEXT("DistanceDamageFalloff")))
		{
			for (FRichCurveKey& Key : Property->ContainerPtrToValuePtr<FRuntimeFloatCurve>(Weapon)->GetRichCurve()->Keys)
			{
				Key.Time *= 1.6f;
			}
		}
	}
}

void UDHPlayerRulesComponent::TrySlide()
{
	ACharacter* Character = GetPawn<ACharacter>();
	UDHPlayerChoicesComponent* Choices = Character && Character->GetController() ? Character->GetController()->FindComponentByClass<UDHPlayerChoicesComponent>() : nullptr;
	if (Choices && DHMovement::CanSlide(Character))
	{
		SetSprintingLocal(false);
		SlideStartedAt = GetWorld()->GetTimeSeconds();
		DHMovement::Slide(Character);
		Choices->ServerSlide();
	}
}

bool UDHPlayerRulesComponent::IsGamepadInput() const
{
	const APawn* Pawn = GetPawn<APawn>();
	const APlayerController* PC = Pawn ? Pawn->GetController<APlayerController>() : nullptr;
	const UCommonInputSubsystem* CommonInput = PC && PC->GetLocalPlayer() ? UCommonInputSubsystem::Get(PC->GetLocalPlayer()) : nullptr;
	return CommonInput && CommonInput->GetCurrentInputType() == ECommonInputType::Gamepad;
}

void UDHPlayerRulesComponent::HandleSprintPressed()
{
	// The gamepad button toggles (a stick press, nobody holds that while running); the key is held. Pressed before the
	// player moves forward, the sprint starts as soon as they do (on the gamepad within a moment).
	if (bSprinting && IsGamepadInput())
	{
		bSprintWanted = false;
		SetSprintingLocal(false);
		return;
	}
	bSprintWanted = true;
	SprintWantedAt = GetWorld()->GetTimeSeconds();
	CheckSprint();
	GetWorld()->GetTimerManager().SetTimer(SprintTimer, this, &ThisClass::CheckSprint, 0.1f, true);
}

void UDHPlayerRulesComponent::HandleSprintReleased()
{
	if (!IsGamepadInput())
	{
		bSprintWanted = false;
		SetSprintingLocal(false);
	}
}

void UDHPlayerRulesComponent::SetSprintingLocal(bool bOn)
{
	const APawn* Pawn = GetPawn<APawn>();
	if (bOn == bSprinting || !Pawn)
	{
		return;
	}
	SetSprinting(bOn);
	UDHPlayerChoicesComponent* Choices = Pawn->GetController() ? Pawn->GetController()->FindComponentByClass<UDHPlayerChoicesComponent>() : nullptr;
	if (Choices && !Pawn->HasAuthority())
	{
		Choices->ServerSetSprint(bOn);
	}
}

void UDHPlayerRulesComponent::SetSprinting(bool bOn)
{
	bSprinting = bOn;
	UpdateMoveSpeed();
	UE_LOG(LogDHRules, Log, TEXT("%s: sprint %s (%s)"), *GetNameSafe(GetPawn<APawn>()), bOn ? TEXT("on") : TEXT("off"), GetOwner()->HasAuthority() ? TEXT("server") : TEXT("client"));
}

void UDHPlayerRulesComponent::CheckSprint()
{
	// Owning client, while the sprint is wanted or on: it ends when the stick or key lets go of forward, on aiming or
	// crouching, and on a shot; a held key starts it again once possible, a toggle on the gamepad does not.
	const ACharacter* Character = GetPawn<ACharacter>();
	ULyraEquipmentManagerComponent* Equipment = Character ? Character->FindComponentByClass<ULyraEquipmentManagerComponent>() : nullptr;
	const ULyraWeaponInstance* Weapon = Equipment ? Equipment->GetFirstInstanceOfType<ULyraWeaponInstance>() : nullptr;
	const bool bPossible = DHMovement::CanSprint(Character) && !(Weapon && Weapon->GetTimeSinceLastInteractedWith() < 0.2f);
	const bool bGamepad = IsGamepadInput();
	if (bSprinting && !bPossible)
	{
		SetSprintingLocal(false);
		bSprintWanted = bSprintWanted && !bGamepad;
	}
	else if (!bSprinting && bSprintWanted)
	{
		if (bPossible)
		{
			SetSprintingLocal(true);
		}
		else if (bGamepad && GetWorld()->GetTimeSeconds() - SprintWantedAt > 0.6)
		{
			bSprintWanted = false;
		}
	}
	if (!bSprinting && !bSprintWanted)
	{
		GetWorld()->GetTimerManager().ClearTimer(SprintTimer);
	}
}

void UDHPlayerRulesComponent::TryWallJump()
{
	ACharacter* Character = GetPawn<ACharacter>();
	// Jump out of a slide (Lyra does not jump while crouched): stand up and jump with the slide's speed.
	if (Character && Character->bIsCrouched && GetWorld()->GetTimeSeconds() - SlideStartedAt < DHMovement::SlideSeconds)
	{
		SlideStartedAt = -100.0;
		Character->UnCrouch();
		Character->Jump();
		return;
	}
	UDHPlayerChoicesComponent* Choices = Character && Character->GetController() ? Character->GetController()->FindComponentByClass<UDHPlayerChoicesComponent>() : nullptr;
	const double Now = GetWorld()->GetTimeSeconds();
	const FVector WallNormal = Choices && Now - LastWallJump > DHMovement::WallJumpCooldown ? DHMovement::FindWall(Character) : FVector::ZeroVector;
	if (!WallNormal.IsZero())
	{
		LastWallJump = Now;
		DHMovement::WallJump(Character, WallNormal);
		Choices->ServerWallJump(WallNormal);
	}
}

void UDHPlayerRulesComponent::UseAbility(int32 Slot)
{
	const APawn* Pawn = GetPawn<APawn>();
	if (UDHPlayerChoicesComponent* Choices = Pawn && Pawn->GetController() ? Pawn->GetController()->FindComponentByClass<UDHPlayerChoicesComponent>() : nullptr)
	{
		Choices->UseAbility(Slot);
	}
}

void UDHPlayerRulesComponent::HandleAbilityActivated(UGameplayAbility* Ability)
{
	static const FGameplayTag FireTag = FGameplayTag::RequestGameplayTag(TEXT("Ability.Type.Action.WeaponFire"));
	if (Ability && Ability->GetAssetTags().HasTag(FireTag) && HasScope())
	{
		UDHSoundCue::Play(GetPawn<APawn>(), TEXT("Sniper"));
	}
}

void UDHPlayerRulesComponent::PrepareLyraDash()
{
	// Lyra's dash (directional root-motion montage, trail effect, sound; predicted and replicated) is the loadout's
	// Dash: UDHPlayerChoicesComponent::UseAbility activates it. Its own input tag goes, so Lyra's dash button (now the
	// first ability, whatever it is) does not fire it, and its old cooldown widget goes, our HUD shows the abilities.
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetPawn<APawn>());
	if (!ASC)
	{
		return;
	}
	static const FGameplayTag InputTag = FGameplayTag::RequestGameplayTag(TEXT("InputTag.Ability.Dash"));
	for (FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (!Spec.Ability || !Spec.Ability->GetClass()->GetName().Contains(TEXT("Dash")))
		{
			continue;
		}
		if (ASC->IsOwnerActorAuthoritative() && Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			Spec.GetDynamicSpecSourceTags().RemoveTag(InputTag);
			ASC->MarkAbilitySpecDirty(Spec);
		}
		UGameplayAbility* Instance = Spec.GetPrimaryInstance();
		FArrayProperty* Handles = Instance && GetPawn<APawn>()->IsLocallyControlled() ? FindFProperty<FArrayProperty>(Instance->GetClass(), TEXT("WidgetExtensionHandles")) : nullptr;
		if (Handles && Handles->Inner->IsA<FStructProperty>() && CastField<FStructProperty>(Handles->Inner)->Struct == FUIExtensionHandle::StaticStruct())
		{
			FScriptArrayHelper Array(Handles, Handles->ContainerPtrToValuePtr<void>(Instance));
			for (int32 Index = 0; Index < Array.Num(); ++Index)
			{
				reinterpret_cast<FUIExtensionHandle*>(Array.GetRawPtr(Index))->Unregister();
			}
		}
	}
}

void UDHPlayerRulesComponent::ShowScoreboard()
{
	APlayerController* PC = GetPawn<APawn>() ? GetPawn<APawn>()->GetController<APlayerController>() : nullptr;
	if (!Scoreboard && PC)
	{
		UDHMatchEndScreen* Board = CreateWidget<UDHMatchEndScreen>(PC);
		Board->bLiveScoreboard = true;
		Board->AddToViewport(40);
		Scoreboard = Board;
	}
}

void UDHPlayerRulesComponent::HideScoreboard()
{
	if (Scoreboard)
	{
		Scoreboard->RemoveFromParent();
		Scoreboard = nullptr;
	}
}

void UDHPlayerRulesComponent::TogglePerspective()
{
	const UDHGameRulesComponent* Rules = UDHGameRulesComponent::Find(this);
	if (Rules && Rules->GetPerspectiveRule() == EDHPerspectiveRule::Choice)
	{
		bPreferFirstPerson = !bPreferFirstPerson;
	}
}

bool UDHPlayerRulesComponent::IsFirstPerson() const
{
	const UDHGameRulesComponent* Rules = UDHGameRulesComponent::Find(this);
	switch (Rules ? Rules->GetPerspectiveRule() : EDHPerspectiveRule::ThirdPerson)
	{
	case EDHPerspectiveRule::FirstPerson: return true;
	case EDHPerspectiveRule::Choice: return bPreferFirstPerson;
	default: return false;
	}
}

TSubclassOf<ULyraCameraMode> UDHPlayerRulesComponent::DetermineCameraMode() const
{
	static const FGameplayTag DeathTag = FGameplayTag::RequestGameplayTag(TEXT("Status.Death"));
	static const FGameplayTag AimTag = FGameplayTag::RequestGameplayTag(TEXT("Event.Movement.ADS"));
	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetPawn<APawn>());
	if (ASC && ASC->HasMatchingGameplayTag(DeathTag))
	{
		return DeathMode.LoadSynchronous();
	}

	if (IsFirstPerson() || (ASC && ASC->HasMatchingGameplayTag(AimTag) && HasScope()))
	{
		// Aiming in first person is handled inside the first-person mode (zoom, weapon to the center).
		return FirstPersonMode.LoadSynchronous();
	}

	return (ASC && ASC->HasMatchingGameplayTag(AimTag)) ? ThirdPersonAimMode.LoadSynchronous() : ThirdPersonMode.LoadSynchronous();
}
