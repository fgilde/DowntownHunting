#include "DHXP.h"

#include "DHLoadoutSettings.h"
#include "DHProgression.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Player/LyraPlayerState.h"

namespace DHXP
{
	namespace
	{
		// Once per match: with seamless travel the rules component ends again in the transition world, so a second
		// award shortly after the first is the same match.
		double LastAward = -1.0e9;
	}

	bool Award(UWorld* World, bool bWon, FResult& OutResult)
	{
		const APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
		const ALyraPlayerState* State = Player ? Player->GetPlayerState<ALyraPlayerState>() : nullptr;
		if (!State || FPlatformTime::Seconds() - LastAward < 60.0 || World->GetTimeSeconds() < 30.0)
		{
			return false;
		}
		LastAward = FPlatformTime::Seconds();
		const int32 Kills = State->GetStatTagStackCount(FGameplayTag::RequestGameplayTag(TEXT("ShooterGame.Score.Eliminations")));
		const int32 Headshots = State->GetStatTagStackCount(FGameplayTag::RequestGameplayTag(TEXT("DH.Headshots")));
		OutResult.Gained = 200 + 100 * Kills + 50 * Headshots + (bWon ? 500 : 0);
		OutResult.OldLevel = FDHProgression::GetLevel();
		OutResult.XP = FDHProgression::AddXP(OutResult.Gained);
		OutResult.NewLevel = FDHProgression::LevelForXP(OutResult.XP);
		for (int32 Level = OutResult.OldLevel + 1; Level <= OutResult.NewLevel; ++Level)
		{
			for (const FString& Id : FDHProgression::UnlocksAt(Level))
			{
				OutResult.Unlocked.Add(UDHLoadoutSettings::DisplayName(Id));
			}
		}
		UE_LOG(LogTemp, Log, TEXT("DH XP: +%d (%d kills, %d headshots, won %d) -> %d, level %d"), OutResult.Gained, Kills, Headshots, bWon, OutResult.XP, OutResult.NewLevel);
		return true;
	}
}
