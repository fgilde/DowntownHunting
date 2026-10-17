#include "DHCharacterSettings.h"

#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DHCharacterSettings)

FDHCharacterChosen UDHCharacterSettings::OnCharacterChosen;

namespace
{
	const TCHAR* CharacterSection = TEXT("DowntownHunting");
	const TCHAR* CharacterKey = TEXT("Character");
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
		GConfig->GetString(CharacterSection, CharacterKey, Id, GGameUserSettingsIni);
	}
	return Id;
}

FString UDHCharacterSettings::GetPlayerName()
{
	FString Name;
	if (!FParse::Value(FCommandLine::Get(), TEXT("DHName="), Name))
	{
		GConfig->GetString(CharacterSection, TEXT("PlayerName"), Name, GGameUserSettingsIni);
	}
	return Name.TrimStartAndEnd().Left(20);
}

void UDHCharacterSettings::ChooseCharacter(APlayerController* Player, const FString& Id)
{
	GConfig->SetString(CharacterSection, CharacterKey, *Id, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
	OnCharacterChosen.Broadcast(Player, Id);
}
