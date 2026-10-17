#include "DHLoadoutSettings.h"

#include "Misc/ConfigCacheIni.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DHLoadoutSettings)

FDHLoadoutChosen UDHLoadoutSettings::OnLoadoutChosen;

namespace
{
	const TCHAR* LoadoutSection = TEXT("DowntownHunting");
	const TCHAR* LoadoutKey = TEXT("Loadout");
}

TArray<FDHWeaponEntry> UDHLoadoutSettings::GetWeapons()
{
	return GetDefault<UDHLoadoutSettings>()->Weapons;
}

TArray<FString> UDHLoadoutSettings::GetLoadout()
{
	FString Stored;
	TArray<FString> Loadout;
	if (GConfig->GetString(LoadoutSection, LoadoutKey, Stored, GGameUserSettingsIni))
	{
		Stored.ParseIntoArray(Loadout, TEXT(","));
	}
	return Loadout.IsEmpty() ? GetDefault<UDHLoadoutSettings>()->DefaultLoadout : Loadout;
}

void UDHLoadoutSettings::ChooseLoadout(APlayerController* Player, const TArray<FString>& Loadout)
{
	GConfig->SetString(LoadoutSection, LoadoutKey, *FString::Join(Loadout, TEXT(",")), GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
	OnLoadoutChosen.Broadcast(Player, Loadout);
}
