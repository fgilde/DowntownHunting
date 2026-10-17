#pragma once

#include "Components/GameStateComponent.h"
#include "GameFramework/GameplayMessageSubsystem.h"

#include "DHFreeForAllScoring.generated.h"

class APlayerState;
class ULyraGamePhaseAbility;
struct FLyraVerbMessage;

/**
 * Free-for-all scoring on the game state, in place of Lyra's team scoring (B_TeamDeathMatchScoring only knows two teams).
 * Every player is on a team of their own (B_DH_TeamSetup_FFA), so Lyra's ally/enemy logic, bots and kill feed work as is.
 * Server: runs warmup -> playing -> post game like Lyra's scoring, counts eliminations and deaths on the player states
 * (Lyra's scoreboard reads them) and ends the match when a player reaches the kill limit or the clock runs out.
 * Kill limit and match length: rules ScoreLimit / TimeLimit (UDHGameRulesComponent), else 20 kills and 10 minutes.
 * UDHTeamScoring below is the same for any number of teams.
 */
UCLASS(meta = (BlueprintSpawnableComponent))
class UDHFreeForAllScoring : public UGameStateComponent
{
	GENERATED_BODY()

public:
	UDHFreeForAllScoring(const FObjectInitializer& ObjectInitializer);

	static const UDHFreeForAllScoring* Find(const UObject* WorldContextObject);

	int32 GetKillLimit() const { return KillLimit; }
	// Seconds left; negative while the clock has not started (warmup).
	float GetSecondsLeft() const;
	bool IsMatchOver() const { return !WinnerName.IsEmpty(); }
	const FString& GetWinnerName() const { return WinnerName; }

	bool IsTeamScoring() const { return bTeamScoring; }

	static int32 GetKills(const APlayerState* PlayerState);
	static int32 GetTeamKills(const UObject* WorldContextObject, int32 TeamId);
	// Player with the most kills, nullptr before the first kill.
	const APlayerState* GetLeader() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditDefaultsOnly, Category = "Phases")
	TSoftClassPtr<ULyraGamePhaseAbility> WarmupPhase;

	UPROPERTY(EditDefaultsOnly, Category = "Phases")
	TSoftClassPtr<ULyraGamePhaseAbility> PostGamePhase;

	// Teams play for a shared score (the kill limit counts team kills, team kills of your own team do not count).
	bool bTeamScoring = false;

private:
	void ReadRules();
	void OnElimination(FGameplayTag Channel, const FLyraVerbMessage& Message);
	void StartPhase(const TSoftClassPtr<ULyraGamePhaseAbility>& Phase, FName EndedCallback);
	UFUNCTION()
	void OnWarmupEnded(const ULyraGamePhaseAbility* Phase);
	void EndMatch();

protected:
	UPROPERTY(Replicated)
	int32 KillLimit = 20;

private:

	// Server world time when the match ends, 0 while warming up.
	UPROPERTY(Replicated)
	double MatchEndTime = 0.0;

	UPROPERTY(Replicated)
	FString WinnerName;

	int32 MatchMinutes = 10;
	FTimerHandle ClockTimer;
	FGameplayMessageListenerHandle EliminationListener;
};

/** Team deathmatch for any number of teams (Lyra's B_TeamDeathMatchScoring knows two): first team to the kill limit wins. */
UCLASS(meta = (BlueprintSpawnableComponent))
class UDHTeamScoring : public UDHFreeForAllScoring
{
	GENERATED_BODY()

public:
	UDHTeamScoring(const FObjectInitializer& ObjectInitializer);
};
