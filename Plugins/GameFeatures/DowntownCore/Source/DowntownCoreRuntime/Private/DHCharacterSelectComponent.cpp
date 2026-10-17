#include "DHCharacterSelectComponent.h"

#include "DHGameRulesComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DHCharacterSelectComponent)

namespace
{
	const TCHAR* SettingsSection = TEXT("DowntownHunting");
	const TCHAR* SettingsKey = TEXT("Character");

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

UDHCharacterSelectComponent::UDHCharacterSelectComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
}

TArray<FDHCharacterEntry> UDHCharacterSelectComponent::GetCharacters()
{
	return GetDefault<UDHCharacterSelectComponent>()->Characters;
}

FString UDHCharacterSelectComponent::GetPreferredCharacter()
{
	FString Id;
	if (!FParse::Value(FCommandLine::Get(), TEXT("DHCharacter="), Id))
	{
		GConfig->GetString(SettingsSection, SettingsKey, Id, GGameUserSettingsIni);
	}
	return Id;
}

void UDHCharacterSelectComponent::SetPreferredCharacter(const FString& Id)
{
	GConfig->SetString(SettingsSection, SettingsKey, *Id, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

void UDHCharacterSelectComponent::BeginPlay()
{
	Super::BeginPlay();

	AController* Controller = GetController<AController>();
	if (Controller && Controller->IsLocalPlayerController())
	{
		ServerSetCharacter(GetPreferredCharacter());
	}
	else if (Controller && Controller->HasAuthority() && !Controller->IsPlayerController())
	{
		ApplyCharacter(FString());  // bot
	}
}

void UDHCharacterSelectComponent::ServerSetCharacter_Implementation(const FString& Id)
{
	ApplyCharacter(Id);
}

void UDHCharacterSelectComponent::ApplyCharacter(const FString& Id)
{
	AController* Controller = GetController<AController>();
	UActorComponent* Parts = Controller ? FindCharacterParts(Controller) : nullptr;
	const TArray<FDHCharacterEntry>& All = GetDefault<UDHCharacterSelectComponent>()->Characters;
	if (!Parts || All.IsEmpty())
	{
		UE_LOG(LogDHRules, Warning, TEXT("%s: no character parts component or no characters configured"), *GetNameSafe(Controller));
		return;
	}

	const FDHCharacterEntry* Entry = All.FindByPredicate([&Id](const FDHCharacterEntry& E) { return E.Id == Id; });
	if (!Entry)
	{
		Entry = &All[FMath::RandRange(0, All.Num() - 1)];
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
