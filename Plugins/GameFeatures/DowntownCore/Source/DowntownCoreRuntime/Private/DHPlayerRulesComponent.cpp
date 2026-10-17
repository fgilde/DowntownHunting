#include "DHPlayerRulesComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Camera/LyraCameraComponent.h"
#include "Camera/LyraCameraMode.h"
#include "DHGameRulesComponent.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
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
}

void UDHPlayerRulesComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorld()->GetTimerManager().ClearTimer(RetryTimer);

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
	// Wait for Lyra's own starting weapon so the extra ones land in the following slots.
	if (!Inventory || !QuickBar || Inventory->GetAllItems().IsEmpty())
	{
		return;
	}

	for (UClass* ItemClass : Rules->GetLoadoutItems(Controller))
	{
		const TSubclassOf<ULyraInventoryItemDefinition> ItemDef(ItemClass);
		if (!Inventory->FindFirstItemStackByDefinition(ItemDef))
		{
			const bool bSlotted = AddToFirstFreeQuickBarSlot(QuickBar, Inventory->AddItemDefinition(ItemDef));
			UE_LOG(LogDHRules, Log, TEXT("%s: gave %s (quick bar: %s)"), *GetNameSafe(Controller), *ItemClass->GetName(), bSlotted ? TEXT("yes") : TEXT("full"));
		}
	}
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
		BoundInputComponent = InputComponent;
	}
	UInputMappingContext* Mapping = InputMapping.LoadSynchronous();
	if (Input && Mapping && !Input->HasMappingContext(Mapping))
	{
		Input->AddMappingContext(Mapping, 1);
		bMappingAdded = true;
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

	if (IsFirstPerson())
	{
		// Aiming in first person is handled inside the first-person mode (zoom, weapon to the center).
		return FirstPersonMode.LoadSynchronous();
	}

	return (ASC && ASC->HasMatchingGameplayTag(AimTag)) ? ThirdPersonAimMode.LoadSynchronous() : ThirdPersonMode.LoadSynchronous();
}
