#pragma once

#include "Components/GameStateComponent.h"
#include "GameFramework/GameplayMessageSubsystem.h"

#include "DHGameRulesComponent.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogDHRules, Log, All);

class AController;
class APlayerState;
struct FLyraVerbMessage;

UENUM(BlueprintType)
enum class EDHPerspectiveRule : uint8
{
	ThirdPerson,
	FirstPerson,
	// Each player switches between first and third person.
	Choice,
};

/**
 * Match rules of a Downtown Hunting mode, read on the server from the travel URL options that the mode's
 * playlist entry puts there (ExtraArgs), and replicated to clients:
 *   Perspective=TP|FP|Choice   WeaponSwitch=1|0   Loadout=Pistol,Rifle,Shotgun,AK47,MP5,M1911 | Player
 *   ScoreLimit=<points to win, 0 = Lyra's default>   TimeLimit=<minutes, 0 = Lyra's default>
 *   GunGame=<weapon list>: everybody starts with the first weapon, each kill gives the next one, a kill with the last
 *   one wins the match for the team (no weapon switching, Loadout is ignored).
 * Loadout=Player: each player brings the loadout from the loadout builder (UDHPlayerChoicesComponent), bots get a random primary + pistol.
 * Added to the game state by every DH experience.
 */
UCLASS(meta = (BlueprintSpawnableComponent))
class UDHGameRulesComponent : public UGameStateComponent
{
	GENERATED_BODY()

public:
	UDHGameRulesComponent(const FObjectInitializer& ObjectInitializer);

	static const UDHGameRulesComponent* Find(const UObject* WorldContextObject);

	EDHPerspectiveRule GetPerspectiveRule() const { return Perspective; }
	bool AllowsWeaponSwitch() const { return bWeaponSwitch; }

	// Rule values, 0 = not set (server only).
	int32 GetScoreLimit() const { return ScoreLimit; }
	int32 GetTimeLimit() const { return TimeLimit; }
	const TArray<FName>& GetGunGame() const { return GunGame; }
	// Gun Game step of a player (stat tag DH.GunGame.Level on the Lyra player state, replicated).
	static int32 GetGunGameLevel(const APlayerState* PlayerState);

	// Server only. Item definition classes to give the controller's pawn, in quick bar order.
	TArray<UClass*> GetLoadoutItems(const AController* Controller) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Loadout names (URL option) to inventory item definitions.
	UPROPERTY(EditDefaultsOnly, Category = "Rules")
	TMap<FName, TSoftClassPtr<UObject>> WeaponItems;

	UPROPERTY(Replicated)
	EDHPerspectiveRule Perspective = EDHPerspectiveRule::ThirdPerson;

	UPROPERTY(Replicated)
	bool bWeaponSwitch = true;

	TArray<FName> Loadout;
	bool bPlayerLoadout = false;

private:
	void ApplyLimits();
	void OnElimination(FGameplayTag Channel, const FLyraVerbMessage& Message);
	void ShowKiller(APlayerState* Killer, APlayerState* Victim) const;
	void WinGunGame(const FLyraVerbMessage& Message);

	int32 ScoreLimit = 0;
	int32 TimeLimit = 0;
	bool bTimeLimitApplied = false;
	FTimerHandle LimitsTimer;

	UPROPERTY(Replicated)
	TArray<FName> GunGame;

	bool bGunGameWon = false;
	FGameplayMessageListenerHandle EliminationListener;
};
