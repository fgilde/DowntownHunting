#include "DHLoadoutSettings.h"

#include "Misc/ConfigCacheIni.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DHLoadoutSettings)

FDHLoadoutChosen UDHLoadoutSettings::OnLoadoutChosen;

namespace
{
	const TCHAR* SettingsSection = TEXT("DowntownHunting");
	const TCHAR* SettingsKey = TEXT("Loadout");
}

TArray<FDHWeaponEntry> UDHLoadoutSettings::GetWeapons()
{
	return GetDefault<UDHLoadoutSettings>()->Weapons;
}

TArray<FString> UDHLoadoutSettings::GetLoadout()
{
	FString Stored;
	TArray<FString> Loadout;
	if (GConfig->GetString(SettingsSection, SettingsKey, Stored, GGameUserSettingsIni))
	{
		Stored.ParseIntoArray(Loadout, TEXT(","));
	}
	return Loadout.IsEmpty() ? GetDefault<UDHLoadoutSettings>()->DefaultLoadout : Loadout;
}

void UDHLoadoutSettings::ChooseLoadout(APlayerController* Player, const TArray<FString>& Loadout)
{
	GConfig->SetString(SettingsSection, SettingsKey, *FString::Join(Loadout, TEXT(",")), GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
	OnLoadoutChosen.Broadcast(Player, Loadout);
}
