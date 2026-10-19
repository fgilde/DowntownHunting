#pragma once

#include "CoreMinimal.h"

class UWorld;

/**
 * Experience for the local player at the end of a match, added to the locally stored progress (FDHProgression):
 * 200 for playing, 100 per kill, 50 more per headshot kill, 500 for a win. Once per map: from the match end screen
 * (with a win) or, in modes without one, when the map is left (UDHGameRulesComponent::EndPlay).
 */
namespace DHXP
{
	struct FResult
	{
		int32 Gained = 0;
		int32 XP = 0;
		int32 OldLevel = 1;
		int32 NewLevel = 1;
		TArray<FString> Unlocked;
	};

	/** False if already awarded for this match or there is no local player. */
	bool Award(UWorld* World, bool bWon, FResult& OutResult);
}
