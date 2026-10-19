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
	// Winning team (team modes) or player (free-for-all), set when the match is over.
	int32 GetWinnerTeam() const { return WinnerTeam; }
	const APlayerState* GetWinnerPlayer() const { return WinnerPlayer; }

	bool IsTeamScoring() const { return bTeamScoring; }

	static int32 GetKills(const APlayerState* PlayerState);
	static int32 GetDeaths(const APlayerState* PlayerState);
	static int32 GetTeamKills(const UObject* WorldContextObject, int32 TeamId);
	// Player with the most kills, nullptr before the first kill.
	const APlayerState* GetLeader() const;
	// Round result or similar announcement for the HUD, empty when there is none.
	const FString& GetAnnouncement() const { return Announcement; }

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

	// Hooks for match variants (UDHRoundScoring). Server only.
	// Warmup is over: start the clock (default: whole match, MatchMinutes).
	virtual void StartClock();
	virtual void OnClockExpired() { EndMatch(); }
	// A kill of another player (stats are counted already); default: team/player score and kill limit.
	virtual void OnKill(class ALyraPlayerState* Killer, class ALyraPlayerState* Victim, int32 KillerTeam);
	// Every death in the playing phase, also suicides and falls.
	virtual void OnDeath(class ALyraPlayerState* Victim) {}
	void EndMatch();
	void SetClock(double Seconds);

	UPROPERTY(Replicated)
	int32 KillLimit = 20;

	UPROPERTY(Replicated)
	FString Announcement;

	int32 MatchMinutes = 10;
	FTimerHandle ClockTimer;

private:
	void ReadRules();
	void OnElimination(FGameplayTag Channel, const FLyraVerbMessage& Message);
	void StartPhase(const TSoftClassPtr<ULyraGamePhaseAbility>& Phase, FName EndedCallback);
	UFUNCTION()
	void OnWarmupEnded(const ULyraGamePhaseAbility* Phase);

	// Server world time when the match (or round) ends, 0 while warming up.
	UPROPERTY(Replicated)
	double MatchEndTime = 0.0;

	UPROPERTY(Replicated)
	FString WinnerName;

	UPROPERTY(Replicated)
	int32 WinnerTeam = INDEX_NONE;

	UPROPERTY(Replicated)
	TObjectPtr<APlayerState> WinnerPlayer;

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

/**
 * Elimination: rounds without respawn (the experience grants no auto respawn). A team wins the round when nobody of the
 * other teams is left, or with more survivors when the round clock (TimeLimit, default 2 minutes) runs out. Round wins
 * are the team score; ScoreLimit rounds (default 4) win the match. Between rounds everyone is restarted.
 * Base for Search & Destroy.
 */
UCLASS(meta = (BlueprintSpawnableComponent))
class UDHRoundScoring : public UDHFreeForAllScoring
{
	GENERATED_BODY()

public:
	UDHRoundScoring(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void StartClock() override;
	virtual void OnClockExpired() override;
	virtual void OnKill(ALyraPlayerState* Killer, ALyraPlayerState* Victim, int32 KillerTeam) override {}
	virtual void OnDeath(ALyraPlayerState* Victim) override;

	virtual void CheckRoundOver();
	void EndRound(int32 RoundWinner, const FString& Reason = FString());
	void StartNextRound();
	// Players alive per team.
	TMap<int32, int32> CountAlive() const;

	bool bRoundOver = false;
	int32 Round = 0;
	FTimerHandle RoundTimer;
};

class ADHBombSite;

/**
 * Search & Destroy on top of Elimination: two teams swap attack and defence every round. Attackers plant the bomb by
 * standing in site A or B for PlantSeconds without a defender there, defenders defuse it the same way (DefuseSeconds).
 * Attackers win by the explosion or by eliminating all defenders; defenders win by defusing, by eliminating all
 * attackers before the plant, or when the round clock runs out without a plant. The sites are spawned at the first
 * and last control point of the map.
 */
UCLASS(meta = (BlueprintSpawnableComponent))
class UDHSearchAndDestroyScoring : public UDHRoundScoring
{
	GENERATED_BODY()

public:
	UDHSearchAndDestroyScoring(const FObjectInitializer& ObjectInitializer);

	static constexpr float PlantSeconds = 5.f;
	static constexpr float DefuseSeconds = 7.f;
	static constexpr float BombSeconds = 40.f;

	int32 GetAttackingTeam() const { return AttackingTeam; }
	const TArray<TObjectPtr<ADHBombSite>>& GetSites() const { return Sites; }
	const ADHBombSite* GetPlantedSite() const { return PlantedSite; }
	// Plant or defuse progress 0..1 and who is at it.
	float GetProgress() const { return Progress; }
	bool IsDefusing() const { return bDefusing; }

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void StartClock() override;
	virtual void OnClockExpired() override;
	virtual void CheckRoundOver() override;

private:
	void TickObjective();
	void GuideBots();
	int32 DefendingTeam() const;

	UPROPERTY(Replicated)
	int32 AttackingTeam = INDEX_NONE;

	UPROPERTY(Replicated)
	TArray<TObjectPtr<ADHBombSite>> Sites;

	UPROPERTY(Replicated)
	TObjectPtr<ADHBombSite> PlantedSite;

	UPROPERTY(Replicated)
	float Progress = 0.f;

	UPROPERTY(Replicated)
	bool bDefusing = false;

	FTimerHandle ObjectiveTimer;
	int32 ObjectiveTicks = 0;
};

/**
 * King of the Hill: one hill (a ring like the bomb sites) stands at a control point of the map and moves to the next
 * one every minute. A team alone on the hill scores a point per second, contested hills score nothing; kills do not
 * score. First team to ScoreLimit points (default 120) wins, or the leader when the clock (TimeLimit) runs out.
 */
UCLASS(meta = (BlueprintSpawnableComponent))
class UDHHillScoring : public UDHTeamScoring
{
	GENERATED_BODY()

public:
	UDHHillScoring(const FObjectInitializer& ObjectInitializer);

	static constexpr float MoveSeconds = 60.f;

	const ADHBombSite* GetHill() const { return Hill; }

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void StartClock() override;
	virtual void OnClockExpired() override;
	virtual void OnKill(ALyraPlayerState* Killer, ALyraPlayerState* Victim, int32 KillerTeam) override {}

private:
	void TickHill();
	void MoveHill();
	void GuideBots();

	UPROPERTY(Replicated)
	TObjectPtr<ADHBombSite> Hill;

	TArray<FVector> Spots;
	int32 SpotIndex = 0;
	float HillSeconds = 0.f;
	float PointSeconds = 0.f;
	int32 Ticks = 0;
	FTimerHandle HillTimer;
};
