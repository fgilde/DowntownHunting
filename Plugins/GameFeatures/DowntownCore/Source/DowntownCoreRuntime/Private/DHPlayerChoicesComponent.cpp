#include "DHPlayerChoicesComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/GameplayAbility.h"
#include "DHAbilities.h"
#include "DHCharacterSettings.h"
#include "DHLoadoutSettings.h"
#include "DHGameRulesComponent.h"
#include "DHHitMarker.h"
#include "DHKillcam.h"
#include "DHKillstreaks.h"
#include "DHStreakBanner.h"
#include "DHMovement.h"
#include "DHProgression.h"
#include "DHPlayerRulesComponent.h"
#include "DHSoundCue.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/PlatformProcess.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DHPlayerChoicesComponent)

namespace
{
	UActorComponent* FindCharacterParts(AController* Controller)
	{
		for (UActorComponent* Component : Controller->GetComponents())
		{
			for (UClass* Class = Component->GetClass(); Class; Class = Class->GetSuperClass())
			{
				if (Class->GetFName() == TEXT("LyraControllerComponent_CharacterParts"))
				{
					return Component;
				}
			}
		}
		return nullptr;
	}

	/** Calls a UFunction whose only parameter (if any) is a FLyraCharacterPart with PartClass set. */
	void CallPartFunction(UObject* Target, FName FunctionName, UClass* PartClass)
	{
		UFunction* Function = Target->FindFunction(FunctionName);
		if (!Function)
		{
			return;
		}
		uint8* Params = static_cast<uint8*>(FMemory_Alloca_Aligned(Function->ParmsSize, Function->GetMinAlignment()));
		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->InitializeValue_InContainer(Params);
			if (const FStructProperty* PartProperty = CastField<FStructProperty>(*It))
			{
				if (const FClassProperty* ClassProperty = FindFProperty<FClassProperty>(PartProperty->Struct, TEXT("PartClass")))
				{
					ClassProperty->SetObjectPropertyValue_InContainer(PartProperty->ContainerPtrToValuePtr<void>(Params), PartClass);
				}
			}
		}
		Target->ProcessEvent(Function, Params);
		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->DestroyValue_InContainer(Params);
		}
	}
}

UDHPlayerChoicesComponent::UDHPlayerChoicesComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
}

void UDHPlayerChoicesComponent::BeginPlay()
{
	Super::BeginPlay();

	AController* Controller = GetController<AController>();
	if (Controller && Controller->IsLocalPlayerController())
	{
		ServerSetCharacter(UDHCharacterSettings::GetPreferredCharacter());
		ChosenHandle = UDHCharacterSettings::OnCharacterChosen.AddUObject(this, &ThisClass::HandleCharacterChosen);
		ServerSetLoadout(UDHLoadoutSettings::GetLoadout());
		SetAbilities(UDHLoadoutSettings::GetLoadout());
		ServerSetPlayerName(UDHCharacterSettings::GetPlayerName(), FPlatformProcess::ComputerName());
		LoadoutHandle = UDHLoadoutSettings::OnLoadoutChosen.AddUObject(this, &ThisClass::HandleLoadoutChosen);
	}
	else if (Controller && Controller->HasAuthority() && !Controller->IsPlayerController())
	{
		ApplyCharacter(FString());  // bot
	}
}

void UDHPlayerChoicesComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UDHCharacterSettings::OnCharacterChosen.Remove(ChosenHandle);
	UDHLoadoutSettings::OnLoadoutChosen.Remove(LoadoutHandle);
	if (AttackSource.IsValid())
	{
		AttackSource->AbilityActivatedCallbacks.Remove(AttackHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void UDHPlayerChoicesComponent::HandleCharacterChosen(APlayerController* Player, const FString& Id)
{
	if (Player == GetOwner())
	{
		ServerSetCharacter(Id);
	}
}

void UDHPlayerChoicesComponent::HandleLoadoutChosen(APlayerController* Player, const TArray<FString>& NewLoadout)
{
	if (Player == GetOwner())
	{
		ServerSetLoadout(NewLoadout);
		SetAbilities(NewLoadout);
	}
}

void UDHPlayerChoicesComponent::ServerSetLoadout_Implementation(const TArray<FString>& NewLoadout)
{
	Loadout.Reset();
	Attachments.Reset();
	for (int32 Index = UDHLoadoutSettings::Primary; Index <= UDHLoadoutSettings::Secondary && Index < NewLoadout.Num(); ++Index)
	{
		// "Weapon" or "Weapon:Attachment"
		const FString& Entry = NewLoadout[Index];
		FString Weapon = Entry, Attachment;
		Entry.Split(TEXT(":"), &Weapon, &Attachment);
		if (!Weapon.IsEmpty())
		{
			Loadout.Add(FName(*Weapon));
			if (Attachment == TEXT("ExtMag") || Attachment == TEXT("Laser") || Attachment == TEXT("Barrel"))
			{
				Attachments.Add(FName(*Weapon), FName(*Attachment));
			}
		}
	}
	SetAbilities(NewLoadout);
	Streaks = UDHLoadoutSettings::StreaksOf(NewLoadout);
	UE_LOG(LogDHRules, Log, TEXT("%s: loadout %s"), *GetNameSafe(GetOwner()), *FString::Join(NewLoadout, TEXT(",")));
}

void UDHPlayerChoicesComponent::SetAbilities(const TArray<FString>& NewLoadout)
{
	auto Entry = [&NewLoadout](int32 Index) { return NewLoadout.IsValidIndex(Index) ? NewLoadout[Index] : FString(); };
	Abilities[0] = UDHLoadoutSettings::FindAbility(Entry(UDHLoadoutSettings::Ability)) ? Entry(UDHLoadoutSettings::Ability) : FString();
	Abilities[1] = UDHLoadoutSettings::FindAbility(Entry(UDHLoadoutSettings::Ability2)) && Entry(UDHLoadoutSettings::Ability2) != Abilities[0]
		? Entry(UDHLoadoutSettings::Ability2) : FString();
	PerkId = Entry(UDHLoadoutSettings::Perk);
}

float UDHPlayerChoicesComponent::GetCooldown(int32 Slot) const
{
	const FDHAbilityEntry* Ability = UDHLoadoutSettings::FindAbility(Abilities[Slot]);
	if (Ability && FDHProgression::DevFlag(TEXT("NoCooldowns")))
	{
		return 0.5f;  // dev screen: just enough to not fire twice from one press
	}
	return Ability ? Ability->Cooldown * (PerkId == TEXT("QuickRecharge") ? UDHLoadoutSettings::QuickRechargeFactor : 1.f) : 0.f;
}

float UDHPlayerChoicesComponent::GetDuration(int32 Slot) const
{
	const FDHAbilityEntry* Ability = UDHLoadoutSettings::FindAbility(Abilities[Slot]);
	return Ability ? Ability->Duration * (PerkId == TEXT("LongLasting") ? UDHLoadoutSettings::LongLastingFactor : 1.f) : 0.f;
}

float UDHPlayerChoicesComponent::GetRemaining(int32 Slot) const
{
	return FMath::Max(0.f, float(ReadyAt[Slot] - GetWorld()->GetTimeSeconds()));
}

void UDHPlayerChoicesComponent::UseAbility(int32 Slot)
{
	static const FGameplayTag DeathTag = FGameplayTag::RequestGameplayTag(TEXT("Status.Death"));
	ACharacter* Character = Cast<ACharacter>(GetPawn<APawn>());
	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Character);
	if (!Character || Abilities[Slot].IsEmpty() || GetRemaining(Slot) > 0.f || (ASC && ASC->HasMatchingGameplayTag(DeathTag)))
	{
		return;
	}
	ReadyAt[Slot] = GetWorld()->GetTimeSeconds() + GetCooldown(Slot);
	FVector Direction = Character->GetLastMovementInputVector();
	if (Direction.IsNearlyZero())
	{
		Direction = Character->GetVelocity();
	}
	if (Abilities[Slot] == TEXT("Dash"))
	{
		// Lyra's dash ability (montage, trail, sound), predicted here and run on the server (UDHPlayerRulesComponent::PrepareLyraDash).
		UAbilitySystemComponent* OwnASC = const_cast<UAbilitySystemComponent*>(ASC);
		for (const FGameplayAbilitySpec& Spec : OwnASC ? OwnASC->GetActivatableAbilities() : TArray<FGameplayAbilitySpec>())
		{
			if (Spec.Ability && Spec.Ability->GetClass()->GetName().Contains(TEXT("Dash")))
			{
				OwnASC->TryActivateAbility(Spec.Handle);
				break;
			}
		}
	}
	else
	{
		DHAbilities::Push(Character, Abilities[Slot], Direction);
	}
	if (Abilities[Slot] == TEXT("Adrenaline"))
	{
		LocalAdrenalineUntil = GetWorld()->GetTimeSeconds() + GetDuration(Slot);
	}
	ServerUseAbility(Slot, Direction.GetSafeNormal2D());
}

void UDHPlayerChoicesComponent::ServerUseAbility_Implementation(uint8 Slot, FVector_NetQuantizeNormal Direction)
{
	static const FGameplayTag DeathTag = FGameplayTag::RequestGameplayTag(TEXT("Status.Death"));
	ACharacter* Character = Cast<ACharacter>(GetPawn<APawn>());
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Character);
	const double Now = GetWorld()->GetTimeSeconds();
	// A little slack for the client's clock and the time the call travels.
	if (Slot > 1 || !Character || Abilities[Slot].IsEmpty() || Now < ServerReadyAt[Slot] - 0.3 || (ASC && ASC->HasMatchingGameplayTag(DeathTag)))
	{
		return;
	}
	ServerReadyAt[Slot] = Now + GetCooldown(Slot);
	if (!Character->IsLocallyControlled() && Abilities[Slot] != TEXT("Dash"))
	{
		DHAbilities::Push(Character, Abilities[Slot], Direction);
	}
	DHAbilities::Start(Character, Abilities[Slot], GetDuration(Slot));
	if (Abilities[Slot] != TEXT("Dash"))  // Lyra's dash brings its own cue
	{
		UDHSoundCue::Play(Character, *(TEXT("Ability.") + Abilities[Slot]));
	}
	if (Abilities[Slot] == TEXT("Cloak") && ASC && AttackSource.Get() != ASC)
	{
		if (AttackSource.IsValid())
		{
			AttackSource->AbilityActivatedCallbacks.Remove(AttackHandle);
		}
		AttackSource = ASC;
		AttackHandle = ASC->AbilityActivatedCallbacks.AddUObject(this, &ThisClass::HandleAbilityActivated);
	}
	UE_LOG(LogDHRules, Log, TEXT("%s: ability %s"), *GetNameSafe(GetOwner()), *Abilities[Slot]);
}

void UDHPlayerChoicesComponent::HandleAbilityActivated(UGameplayAbility* Ability)
{
	// Firing, melee or a grenade ends the cloak.
	static const FGameplayTagContainer Attacks = FGameplayTagContainer::CreateFromArray(TArray<FGameplayTag>{
		FGameplayTag::RequestGameplayTag(TEXT("Ability.Type.Action.WeaponFire")),
		FGameplayTag::RequestGameplayTag(TEXT("Ability.Type.Action.Melee")),
		FGameplayTag::RequestGameplayTag(TEXT("Ability.Type.Action.Grenade")) });
	AController* Controller = GetController<AController>();
	if (Ability && Controller && Ability->GetAssetTags().HasAny(Attacks))
	{
		DHAbilities::EndCloak(Controller->PlayerState);
	}
}

void UDHPlayerChoicesComponent::ClientShowKiller_Implementation(APlayerState* Killer, float KillerHealth, float Distance)
{
	if (APlayerController* Player = GetController<APlayerController>())
	{
		UE_LOG(LogDHRules, Log, TEXT("Killcam: killed by %s"), *GetNameSafe(Killer));
		UDHKillcam* Killcam = CreateWidget<UDHKillcam>(Player);
		Killcam->Start(Killer, KillerHealth, Distance);
		Killcam->AddToViewport(30);
	}
}

void UDHPlayerChoicesComponent::ClientHitFeedback_Implementation(bool bKill, bool bHeadshot)
{
	APlayerController* Player = GetController<APlayerController>();
	if (!Player)
	{
		return;
	}
	if (!HitMarker)
	{
		HitMarker = CreateWidget<UDHHitMarker>(Player);
		HitMarker->AddToViewport(20);
	}
	HitMarker->Show(bKill, bHeadshot);
}

void UDHPlayerChoicesComponent::ClientStreak_Implementation(int32 Streak, const FString& EarnedId)
{
	if (APlayerController* Player = GetController<APlayerController>())
	{
		UDHStreakBanner* Banner = CreateWidget<UDHStreakBanner>(Player);
		const FDHStreakEntry* Earned = UDHLoadoutSettings::FindStreak(EarnedId);
		Banner->Show(Earned ? FText::Format(NSLOCTEXT("DowntownHunting", "StreakEarned", "{0} READY  ·  CALL IT IN"), Earned->Name)
			: FText::Format(NSLOCTEXT("DowntownHunting", "StreakCount", "{0} KILL STREAK"), Streak), Earned != nullptr);
		Banner->AddToViewport(25);
	}
}

void UDHPlayerChoicesComponent::ServerUseStreak_Implementation(const FString& Id, FVector_NetQuantize Target)
{
	const AController* Controller = GetController<AController>();
	DHKillstreaks::Use(Controller ? Controller->PlayerState : nullptr, Id, Target, /*bAutoTarget=*/ false);
}

TArray<FString> UDHPlayerChoicesComponent::GetStreaks() const
{
	if (!Streaks.IsEmpty())
	{
		return Streaks;
	}
	const AController* Controller = GetController<AController>();
	return UDHLoadoutSettings::StreaksOf(Controller && Controller->IsLocalPlayerController() ? UDHLoadoutSettings::GetLoadout() : TArray<FString>());
}

void UDHPlayerChoicesComponent::ServerSlide_Implementation()
{
	ACharacter* Character = Cast<ACharacter>(GetPawn<APawn>());
	if (DHMovement::CanSlide(Character))
	{
		DHMovement::Slide(Character);
	}
}

void UDHPlayerChoicesComponent::ServerSetSprint_Implementation(bool bOn)
{
	if (UDHPlayerRulesComponent* Rules = GetPawn<APawn>() ? GetPawn<APawn>()->FindComponentByClass<UDHPlayerRulesComponent>() : nullptr)
	{
		Rules->SetSprinting(bOn);
	}
}

void UDHPlayerChoicesComponent::ServerWallJump_Implementation(FVector_NetQuantizeNormal WallNormal)
{
	// The client found the wall; the server only checks the jump is possible at all (falling, not too often).
	ACharacter* Character = Cast<ACharacter>(GetPawn<APawn>());
	const double Now = GetWorld()->GetTimeSeconds();
	const FVector Normal = FVector(WallNormal).GetSafeNormal2D();
	if (Character && Character->GetCharacterMovement()->IsFalling() && !Normal.IsZero() && Now - LastWallJump > DHMovement::WallJumpCooldown)
	{
		LastWallJump = Now;
		DHMovement::WallJump(Character, Normal);
	}
}

void UDHPlayerChoicesComponent::ServerSetPlayerName_Implementation(const FString& Name, const FString& OfflineName)
{
	AController* Controller = GetController<AController>();
	APlayerState* PlayerState = Controller ? Controller->PlayerState : nullptr;
	if (!PlayerState)
	{
		return;
	}
	FString NewName = Name.TrimStartAndEnd().Left(20);
	if (NewName.IsEmpty())
	{
		if (OfflineName.IsEmpty() || !PlayerState->GetPlayerName().StartsWith(OfflineName))
		{
			return;
		}
		NewName = FString::Printf(TEXT("Hunter %d"), 100 + GetTypeHash(OfflineName) % 900);
	}
	Controller->GetWorld()->GetAuthGameMode()->ChangeName(Controller, NewName, false);
	UE_LOG(LogDHRules, Log, TEXT("%s: name %s"), *GetNameSafe(Controller), *NewName);
}

void UDHPlayerChoicesComponent::ServerSetCharacter_Implementation(const FString& Id)
{
	ApplyCharacter(Id);
}

void UDHPlayerChoicesComponent::ApplyCharacter(const FString& Id)
{
	AController* Controller = GetController<AController>();
	UActorComponent* Parts = Controller ? FindCharacterParts(Controller) : nullptr;
	const TArray<FDHCharacterEntry> All = UDHCharacterSettings::GetCharacters();
	if (!Parts || All.IsEmpty())
	{
		UE_LOG(LogDHRules, Warning, TEXT("%s: no character parts component or no characters configured"), *GetNameSafe(Controller));
		return;
	}

	const FDHCharacterEntry* Entry = All.FindByPredicate([&Id](const FDHCharacterEntry& E) { return E.Id == Id; });
	if (!Entry)
	{
		// FMath::Rand gave every bot of a match the same pick; a stream per call keeps them apart.
		static FRandomStream Stream(static_cast<int32>(FPlatformTime::Cycles()));
		Entry = &All[Stream.RandRange(0, All.Num() - 1)];
	}
	if (Entry->Id == AppliedId)
	{
		return;
	}

	UClass* PartClass = Entry->PartClass.TryLoadClass<AActor>();
	if (!PartClass)
	{
		UE_LOG(LogDHRules, Warning, TEXT("%s: character %s has no part class %s"), *GetNameSafe(Controller), *Entry->Id, *Entry->PartClass.ToString());
		return;
	}
	CallPartFunction(Parts, TEXT("RemoveAllCharacterParts"), nullptr);
	CallPartFunction(Parts, TEXT("AddCharacterPart"), PartClass);
	AppliedId = Entry->Id;
	UE_LOG(LogDHRules, Log, TEXT("%s: character %s"), *GetNameSafe(Controller), *AppliedId);
}
