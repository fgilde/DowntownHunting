#pragma once

#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"

#include "DHKillstreaks.generated.h"

class APlayerState;
class UNiagaraSystem;
class UPointLightComponent;
class USoundBase;

/**
 * Killstreaks: kills without dying in between (stat tag DH.Streak on the Lyra player state) give rewards.
 * - 3 Radar: enemies on the minimap of the player and the teammates for a while (DH.Streak.RadarUntil).
 * - 5 Airstrike: ADHAirstrike on the biggest group of enemies; its kills count for the player.
 * - 7 Hunter: full health and faster for a while (DH.Streak.HunterUntil, applied by UDHPlayerRulesComponent).
 * The timed rewards are stored as server times in replicated stat tags, so clients can read them.
 */
namespace DHKillstreaks
{
	enum class EReward : uint8
	{
		None,
		Radar,
		Airstrike,
		Hunter,
	};

	constexpr float RadarSeconds = 12.f;
	constexpr float HunterSeconds = 20.f;
	constexpr float HunterSpeed = 1.3f;

	/** Server: a kill (or a team kill / suicide, which only resets the victim). */
	void OnElimination(APlayerState* Killer, APlayerState* Victim, bool bCountsAsKill);

	/** The viewer's own radar or one of a teammate's is running. */
	bool HasRadar(const APlayerState* Viewer);

	bool IsHunting(const APlayerState* Player);

	/** Server: a timed effect (stat tag holding the server time it ends at) runs for Seconds from now; 0 ends it. */
	void SetUntil(APlayerState* Player, FGameplayTag Tag, float Seconds);
	bool IsActive(const APlayerState* Player, FGameplayTag Tag);

	FGameplayTag RadarTag();

	FText RewardText(int32 Streak, EReward Reward);
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
