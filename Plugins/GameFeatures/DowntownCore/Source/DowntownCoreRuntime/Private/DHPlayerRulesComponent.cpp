#include "DHPlayerRulesComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Camera/LyraCameraComponent.h"
#include "Camera/LyraCameraMode.h"
#include "DHAbilities.h"
#include "DHAbilityHud.h"
#include "DHGameRulesComponent.h"
#include "DHFreeForAllScoring.h"
#include "DHGunGameHud.h"
#include "DHKillstreaks.h"
#include "DHMatchEndScreen.h"
#include "DHMinimap.h"
#include "DHMovement.h"
#include "DHPlayerChoicesComponent.h"
#include "DHScopeOverlay.h"
#include "DHWeaponAccess.h"
#include "Components/SkinnedMeshComponent.h"
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
		EnableHairSkinning();
		DHAbilities::UpdateCloak(GetPawn<APawn>(), CloakHidden, bCloakHiddenFromBots);
		ACharacter* Character = GetPawn<ACharacter>();
		UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
		if (Movement)
		{
			if (Character->HasAuthority())
			{
				RemoveLyraDash();
			}
			const float Base = CastChecked<UCharacterMovementComponent>(Movement->GetArchetype())->MaxWalkSpeed;
			const UDHInfectedScoring* Infected = Cast<UDHInfectedScoring>(UDHFreeForAllScoring::Find(this));
			const UDHPlayerChoicesComponent* Choices = Character->GetController() ? Character->GetController()->FindComponentByClass<UDHPlayerChoicesComponent>() : nullptr;
			const bool bAdrenaline = DHAbilities::HasAdrenaline(Character->GetPlayerState())
				|| (Choices && Character->IsLocallyControlled() && Choices->GetLocalAdrenalineUntil() > GetWorld()->GetTimeSeconds());
			Movement->MaxWalkSpeed = Base * (DHKillstreaks::IsHunting(Character->GetPlayerState()) ? DHKillstreaks::HunterSpeed : 1.f)
				* (Infected && Infected->IsInfected(Character->GetPlayerState()) ? UDHInfectedScoring::InfectedSpeed : 1.f)
				* (bAdrenaline ? DHAbilities::AdrenalineSpeed : 1.f);
		}
	}, 0.25f, true);
}

void UDHPlayerRulesComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorld()->GetTimerManager().ClearTimer(RetryTimer);
	GetWorld()->GetTimerManager().ClearTimer(HunterTimer);
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
	}
}

bool UDHPlayerRulesComponent::HasScope() const
{
	const UObject* Weapon = DHWeaponAccess::ActiveWeapon(GetPawn<APawn>());
	return Weapon && Weapon->GetClass()->GetName().Contains(TEXT("Sniper"));
}

void UDHPlayerRulesComponent::EnableHairSkinning()
{
	// Groom hair follows the head through its binding only if the head mesh runs through the GPU skin cache, which
	// Lyra leaves off by default (r.SkinCache.DefaultBehavior=0): without it the hair stays in the rest pose and floats
	// when the head moves. Turn the cache on for every mesh a groom hangs on (character parts can change any time).
	const APawn* Pawn = GetPawn<APawn>();
	if (!Pawn || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	TArray<AActor*> Parts;
	Pawn->GetAttachedActors(Parts, /*bResetArray=*/ true, /*bRecursivelyIncludeAttachedActors=*/ true);
	for (const AActor* Part : Parts)
	{
		for (UActorComponent* Component : Part->GetComponents())
		{
			USceneComponent* Groom = Cast<USceneComponent>(Component);
			USkinnedMeshComponent* Skin = Groom && Groom->GetClass()->GetFName() == TEXT("GroomComponent") ? Cast<USkinnedMeshComponent>(Groom->GetAttachParent()) : nullptr;
			if (!Skin || HairSkins.Contains(Skin))
			{
				continue;
			}
			HairSkins.Add(Skin);
			Skin->SkinCacheUsage.Init(ESkinCacheUsage::Enabled, FMath::Max(1, Skin->GetNumLODs()));
			Skin->MarkRenderStateDirty();
			Groom->MarkRenderStateDirty();
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
		DHMovement::Slide(Character);
		Choices->ServerSlide();
	}
}

void UDHPlayerRulesComponent::TryWallJump()
{
	ACharacter* Character = GetPawn<ACharacter>();
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

void UDHPlayerRulesComponent::RemoveLyraDash()
{
	// Lyra's hero ability set brings its dash with every pawn; the dash is a loadout ability here (DHAbilities).
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetPawn<APawn>());
	if (!ASC)
	{
		return;
	}
	TArray<FGameplayAbilitySpecHandle> Dashes;
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.Ability && Spec.Ability->GetClass()->GetName().Contains(TEXT("Dash")))
		{
			Dashes.Add(Spec.Handle);
		}
	}
	for (const FGameplayAbilitySpecHandle& Handle : Dashes)
	{
		ASC->ClearAbility(Handle);
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
