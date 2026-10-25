#include "DHLoadoutSettings.h"

#include "Misc/ConfigCacheIni.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DHLoadoutSettings)

FDHLoadoutChosen UDHLoadoutSettings::OnLoadoutChosen;

namespace
{
	const TCHAR* LoadoutSection = TEXT("DowntownHunting");
	const TCHAR* ActiveKey = TEXT("ActiveLoadout");

	// The first loadout keeps the key of the times when there was only one.
	FString LoadoutKey(int32 Index)
	{
		return Index == 0 ? TEXT("Loadout") : FString::Printf(TEXT("Loadout%d"), Index + 1);
	}
}

TArray<FDHWeaponEntry> UDHLoadoutSettings::GetWeapons()
{
	return GetDefault<UDHLoadoutSettings>()->Weapons;
}

TArray<FString> UDHLoadoutSettings::GetLoadoutAt(int32 Index)
{
	FString Stored;
	TArray<FString> Loadout;
	if (GConfig->GetString(LoadoutSection, *LoadoutKey(Index), Stored, GGameUserSettingsIni))
	{
		Stored.ParseIntoArray(Loadout, TEXT(","), /*InCullEmpty=*/ false);
	}
	if (Loadout.IsEmpty() || Loadout[0].IsEmpty())
	{
		Loadout = GetDefault<UDHLoadoutSettings>()->DefaultLoadout;
	}
	Loadout.SetNum(NumEntries);
	if (Loadout[Ability].IsEmpty())
	{
		Loadout[Ability] = TEXT("Dash");
	}
	return Loadout;
}

int32 UDHLoadoutSettings::GetActiveLoadout()
{
	int32 Index = 0;
	GConfig->GetInt(LoadoutSection, ActiveKey, Index, GGameUserSettingsIni);
	return FMath::Clamp(Index, 0, NumLoadouts - 1);
}

FString UDHLoadoutSettings::GetLoadoutName(int32 Index)
{
	FString Name;
	GConfig->GetString(LoadoutSection, *FString::Printf(TEXT("LoadoutName%d"), Index + 1), Name, GGameUserSettingsIni);
	Name.TrimStartAndEndInline();
	return Name.IsEmpty() ? FString::Printf(TEXT("LOADOUT %d"), Index + 1) : Name;
}

void UDHLoadoutSettings::SetLoadoutName(int32 Index, const FString& Name)
{
	GConfig->SetString(LoadoutSection, *FString::Printf(TEXT("LoadoutName%d"), Index + 1), *Name.TrimStartAndEnd().Left(18).ToUpper(), GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

void UDHLoadoutSettings::SetActiveLoadout(APlayerController* Player, int32 Index)
{
	GConfig->SetInt(LoadoutSection, ActiveKey, FMath::Clamp(Index, 0, NumLoadouts - 1), GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
	OnLoadoutChosen.Broadcast(Player, GetLoadout());
}

TArray<FDHAbilityEntry> UDHLoadoutSettings::GetAbilities()
{
	return {
		{ TEXT("Dash"), NSLOCTEXT("DowntownHunting", "AbilityDash", "DASH"), NSLOCTEXT("DowntownHunting", "AbilityDashInfo", "Quick burst in the direction you move"), 6.f, 0.f },
		{ TEXT("Leap"), NSLOCTEXT("DowntownHunting", "AbilityLeap", "MEGA JUMP"), NSLOCTEXT("DowntownHunting", "AbilityLeapInfo", "Launches you onto cars, walls and porches"), 10.f, 0.f },
		{ TEXT("Adrenaline"), NSLOCTEXT("DowntownHunting", "AbilityAdrenaline", "ADRENALINE"), NSLOCTEXT("DowntownHunting", "AbilityAdrenalineInfo", "Run 50 % faster for a few seconds"), 20.f, 4.f },
		{ TEXT("Recon"), NSLOCTEXT("DowntownHunting", "AbilityRecon", "RECON PULSE"), NSLOCTEXT("DowntownHunting", "AbilityReconInfo", "Enemies show on the minimap of your team"), 30.f, 5.f },
		{ TEXT("Cloak"), NSLOCTEXT("DowntownHunting", "AbilityCloak", "CLOAK"), NSLOCTEXT("DowntownHunting", "AbilityCloakInfo", "Invisible to everyone, bots too; shooting ends it"), 35.f, 5.f },
	};
}

const FDHAbilityEntry* UDHLoadoutSettings::FindAbility(const FString& Id)
{
	static const TArray<FDHAbilityEntry> Abilities = GetAbilities();
	return Abilities.FindByPredicate([&Id](const FDHAbilityEntry& Entry) { return Entry.Id == Id; });
}

TArray<FDHAbilityEntry> UDHLoadoutSettings::GetPerks()
{
	return {
		{ FString(), NSLOCTEXT("DowntownHunting", "PerkNone", "NO PERK"), FText::GetEmpty() },
		{ TEXT("QuickRecharge"), NSLOCTEXT("DowntownHunting", "PerkQuickRecharge", "QUICK RECHARGE"), NSLOCTEXT("DowntownHunting", "PerkQuickRechargeInfo", "Abilities recharge 25 % faster") },
		{ TEXT("LongLasting"), NSLOCTEXT("DowntownHunting", "PerkLongLasting", "LONG LASTING"), NSLOCTEXT("DowntownHunting", "PerkLongLastingInfo", "Ability effects last 50 % longer") },
	};
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

FString UDHLoadoutSettings::DisplayName(const FString& Id)
{
	if (Id == TEXT("SecondAbility"))
	{
		return NSLOCTEXT("DowntownHunting", "SecondAbilitySlot", "2nd ability slot").ToString();
	}
	if (const FDHAbilityEntry* Ability = FindAbility(Id))
	{
		return Ability->Name.ToString();
	}
	for (const FDHAbilityEntry& Perk : GetPerks())
	{
		if (!Id.IsEmpty() && Perk.Id == Id)
		{
			return Perk.Name.ToString();
		}
	}
	for (const FDHAttachmentEntry& Attachment : GetAttachments())
	{
		if (!Id.IsEmpty() && Attachment.Id == Id)
		{
			return Attachment.Name.ToString();
		}
	}
	return Id;
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
	GConfig->SetString(LoadoutSection, *LoadoutKey(GetActiveLoadout()), *FString::Join(Loadout, TEXT(",")), GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
	OnLoadoutChosen.Broadcast(Player, Loadout);
}
