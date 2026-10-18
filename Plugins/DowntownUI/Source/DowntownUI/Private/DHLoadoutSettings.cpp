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

TArray<FDHAttachmentEntry> UDHLoadoutSettings::GetAttachments()
{
	return {
		{ FString(), NSLOCTEXT("DowntownHunting", "AttachNone", "NONE"), NSLOCTEXT("DowntownHunting", "AttachNoneInfo", "Stock weapon") },
		{ TEXT("ExtMag"), NSLOCTEXT("DowntownHunting", "AttachExtMag", "EXTENDED MAG"), NSLOCTEXT("DowntownHunting", "AttachExtMagInfo", "+50% magazine") },
		{ TEXT("Laser"), NSLOCTEXT("DowntownHunting", "AttachLaser", "LASER SIGHT"), NSLOCTEXT("DowntownHunting", "AttachLaserInfo", "Tighter spread") },
		{ TEXT("Barrel"), NSLOCTEXT("DowntownHunting", "AttachBarrel", "LONG BARREL"), NSLOCTEXT("DowntownHunting", "AttachBarrelInfo", "Damage reaches further") },
	};
}

void UDHLoadoutSettings::SplitEntry(const FString& Entry, FString& OutWeapon, FString& OutAttachment)
{
	OutAttachment.Reset();
	if (!Entry.Split(TEXT(":"), &OutWeapon, &OutAttachment))
	{
		OutWeapon = Entry;
	}
}

void UDHLoadoutSettings::ChooseLoadout(APlayerController* Player, const TArray<FString>& Loadout)
{
	GConfig->SetString(LoadoutSection, LoadoutKey, *FString::Join(Loadout, TEXT(",")), GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
	OnLoadoutChosen.Broadcast(Player, Loadout);
}
