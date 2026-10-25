#pragma once

#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"

#include "DHKillstreaks.generated.h"

class APlayerState;
class UNiagaraSystem;
class UPointLightComponent;
class USoundBase;

/**
 * Killstreaks CoD style: kills without dying in between (stat tag DH.Streak on the Lyra player state) earn the rewards
 * the player took along in the loadout (three of UDHLoadoutSettings::GetStreaks, bots the defaults). An earned reward
 * waits (stat tag DH.Streak.Ready.<Id>, kept through deaths) until the player calls it in (UDHStreakHud: 4 / 5 / 6,
 * D-pad left); bots use theirs at once.
 * - Radar: enemies on the minimap of the player and the teammates for a while (DH.Streak.RadarUntil).
 * - Mortar, Airstrike, Carpet Bomb: ADHAirstrike on the spot the player picks on the map (UDHStrikeMap); its kills
 *   count for the player.
 * - Hunter: full health and faster for a while (DH.Streak.HunterUntil, applied by UDHPlayerRulesComponent).
 * The timed rewards are stored as server times in replicated stat tags, so clients can read them.
 */
namespace DHKillstreaks
{
	constexpr float RadarSeconds = 30.f;
	constexpr float HunterSeconds = 20.f;
	constexpr float HunterSpeed = 1.3f;

	/** Server: a kill (or a team kill / suicide, which only resets the victim). */
	void OnElimination(APlayerState* Killer, APlayerState* Victim, bool bCountsAsKill);

	/** The killstreaks a player takes along, fewest kills first (server: as sent, owning client: own settings). */
	TArray<FString> ChosenStreaks(const APlayerState* Player);

	/** How many of a killstreak the player has earned and not used yet (replicated). */
	int32 ReadyCount(const APlayerState* Player, const FString& Id);

	/** Mortar, Airstrike, Carpet Bomb: called in on a spot of the map. */
	bool IsStrike(const FString& Id);

	/** Radius of a strike's impacts around its spot (for the target circle on the map). */
	float StrikeSpread(const FString& Id);

	/** Server: uses an earned killstreak (strikes on Target, or on the biggest group of enemies with bAutoTarget). */
	bool Use(APlayerState* Player, const FString& Id, const FVector& Target, bool bAutoTarget);

	/** The viewer's own radar or one of a teammate's is running. */
	bool HasRadar(const APlayerState* Viewer);

	bool IsHunting(const APlayerState* Player);

	/** Server: a timed effect (stat tag holding the server time it ends at) runs for Seconds from now; 0 ends it. */
	void SetUntil(APlayerState* Player, FGameplayTag Tag, float Seconds);
	bool IsActive(const APlayerState* Player, FGameplayTag Tag);

	FGameplayTag RadarTag();
}

/** Airstrike of a killstreak: a blinking flare for a few seconds, then explosions around the spot. */
UCLASS(NotPlaceable)
class ADHAirstrike : public AActor
{
	GENERATED_BODY()

public:
	ADHAirstrike();

	/** Server: whose kills these are. */
	TWeakObjectPtr<APlayerState> Striker;

	/** Server, set right after spawning: number of impacts and their spread around the spot. */
	int32 ImpactCount = 6;
	float Spread = 900.f;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	void Impact(const FVector& Location);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastImpact(FVector_NetQuantize Location);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPointLightComponent> Flare;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> ExplosionEffect;


	float Age = 0.f;
	int32 Impacts = 0;
};
