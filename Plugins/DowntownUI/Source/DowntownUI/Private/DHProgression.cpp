#include "DHProgression.h"

#include "Misc/ConfigCacheIni.h"

namespace DHProgressionSettings
{
	const TCHAR* Section = TEXT("DowntownHunting");
	const TCHAR* Key = TEXT("XP");

	struct FUnlock
	{
		const TCHAR* Id;
		int32 Level;
	};
	constexpr FUnlock Unlocks[] = {
		{ TEXT("MP5"), 2 }, { TEXT("ExtMag"), 2 }, { TEXT("M1911"), 3 }, { TEXT("Sniper"), 3 }, { TEXT("Laser"), 3 }, { TEXT("AK47"), 4 }, { TEXT("Barrel"), 5 },
		{ TEXT("Leap"), 2 }, { TEXT("Adrenaline"), 3 }, { TEXT("Recon"), 4 }, { TEXT("QuickRecharge"), 5 }, { TEXT("Cloak"), 6 }, { TEXT("LongLasting"), 7 },
		{ TEXT("SecondAbility"), 8 },
	};
}

int32 FDHProgression::GetXP()
{
	int32 XP = 0;
	GConfig->GetInt(DHProgressionSettings::Section, DHProgressionSettings::Key, XP, GGameUserSettingsIni);
	return FMath::Max(0, XP);
}

int32 FDHProgression::LevelStartXP(int32 Level)
{
	return 500 * (Level - 1) * Level;
}

int32 FDHProgression::LevelForXP(int32 XP)
{
	int32 Level = 1;
	while (XP >= LevelStartXP(Level + 1))
	{
		++Level;
	}
	return Level;
}

int32 FDHProgression::AddXP(int32 Amount)
{
	const int32 XP = GetXP() + FMath::Max(0, Amount);
	GConfig->SetInt(DHProgressionSettings::Section, DHProgressionSettings::Key, XP, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
	return XP;
}

int32 FDHProgression::UnlockLevel(const FString& Id)
{
	for (const DHProgressionSettings::FUnlock& Unlock : DHProgressionSettings::Unlocks)
	{
		if (Id == Unlock.Id)
		{
			return Unlock.Level;
		}
	}
	return 1;
}

TArray<FString> FDHProgression::UnlocksAt(int32 Level)
{
	TArray<FString> Ids;
	for (const DHProgressionSettings::FUnlock& Unlock : DHProgressionSettings::Unlocks)
	{
		if (Unlock.Level == Level)
		{
			Ids.Add(Unlock.Id);
		}
	}
	return Ids;
}
