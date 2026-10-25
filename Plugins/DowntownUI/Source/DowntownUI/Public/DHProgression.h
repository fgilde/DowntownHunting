#pragma once

#include "CoreMinimal.h"

/**
 * Player level from experience points, stored locally (GameUserSettings.ini) like the other menu choices: each player
 * keeps their own progress, also in online matches on other hosts. Weapons and attachments unlock with the level
 * (loadout screen). XP is awarded at the end of a match (DowntownCore, DHXP).
 */
class DOWNTOWNUI_API FDHProgression
{
public:
	static int32 GetXP();
	static int32 GetLevel() { return LevelForXP(GetXP()); }
	static int32 LevelForXP(int32 XP);

	/** XP at which a level starts: 0, 1000, 3000, 6000, 10000, ... */
	static int32 LevelStartXP(int32 Level);

	/** Adds XP and returns the new total. */
	static int32 AddXP(int32 Amount);

	/** Level a weapon or attachment id unlocks at (1 = from the start). */
	static int32 UnlockLevel(const FString& Id);
	static bool IsUnlocked(const FString& Id) { return IsDevMode() || GetLevel() >= UnlockLevel(Id); }

	/**
	 * Developer mode: a file fg.txt next to the game's exe (or in the project folder) with the line DEVDBG=true.
	 * Unlocks everything, shows a DEV badge in the menu and gives the host's player Lyra's cheats.
	 */
	static bool IsDevMode();

	/** Ids that unlock exactly at this level, for the level-up message. */
	static TArray<FString> UnlocksAt(int32 Level);
};
