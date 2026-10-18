#include "DHPlayerChoicesComponent.h"

#include "DHCharacterSettings.h"
#include "DHLoadoutSettings.h"
#include "DHGameRulesComponent.h"
#include "DHKillcam.h"
#include "DHMovement.h"
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
	}
}

void UDHPlayerChoicesComponent::ServerSetLoadout_Implementation(const TArray<FString>& NewLoadout)
{
	Loadout.Reset();
	for (const FString& Name : NewLoadout)
	{
		if (Loadout.Num() < 2)  // primary and secondary
		{
			Loadout.Add(FName(*Name));
		}
	}
	UE_LOG(LogDHRules, Log, TEXT("%s: loadout %s"), *GetNameSafe(GetOwner()), *FString::Join(NewLoadout, TEXT(",")));
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

void UDHPlayerChoicesComponent::ServerSlide_Implementation()
{
	ACharacter* Character = Cast<ACharacter>(GetPawn<APawn>());
	if (DHMovement::CanSlide(Character))
	{
		DHMovement::Slide(Character);
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
