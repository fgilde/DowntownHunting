#include "DHCharacterSettings.h"

#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DHCharacterSettings)

FDHCharacterChosen UDHCharacterSettings::OnCharacterChosen;

namespace
{
	const TCHAR* SettingsSection = TEXT("DowntownHunting");
	const TCHAR* SettingsKey = TEXT("Character");
}

TArray<FDHCharacterEntry> UDHCharacterSettings::GetCharacters()
{
	return GetDefault<UDHCharacterSettings>()->Characters;
}

FString UDHCharacterSettings::GetPreferredCharacter()
{
	FString Id;
	if (!FParse::Value(FCommandLine::Get(), TEXT("DHCharacter="), Id))
	{
		GConfig->GetString(SettingsSection, SettingsKey, Id, GGameUserSettingsIni);
	}
	return Id;
}

void UDHCharacterSettings::ChooseCharacter(APlayerController* Player, const FString& Id)
{
	GConfig->SetString(SettingsSection, SettingsKey, *Id, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
	OnCharacterChosen.Broadcast(Player, Id);
}
