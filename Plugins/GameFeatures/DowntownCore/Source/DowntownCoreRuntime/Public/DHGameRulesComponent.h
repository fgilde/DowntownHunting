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
 *   BotSkill=0 easy | 1 normal | 2 hard | 3 mixed (each bot its own)
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

	/** Rule Hardcore=1: little health, no minimap. */
	bool IsHardcore() const { return bHardcore; }

	static constexpr float HardcoreHealth = 30.f;

	/** Server: how well a bot shoots, 0 easy, 1 normal, 2 hard (rule BotSkill; mixed picks one per bot). */
	int32 GetBotSkill(const APlayerState* Bot) const;

	// Rule values, 0 = not set (server only).
	int32 GetScoreLimit() const { return ScoreLimit; }
	int32 GetTimeLimit() const { return TimeLimit; }
	const TArray<FName>& GetGunGame() const { return GunGame; }
	// Gun Game step of a player (stat tag DH.GunGame.Level on the Lyra player state, replicated).
	static int32 GetGunGameLevel(const APlayerState* PlayerState);

	// Server only. Item definition classes to give the controller's pawn, in quick bar order.
	TArray<UClass*> GetLoadoutItems(const AController* Controller) const;

	/** Name (WeaponItems key) of a weapon item definition class, None if it is not one of them. */
	FName GetWeaponName(const UClass* ItemClass) const;

	/**
	 * Server: the announcer says something on every client (UDHAnnouncer: events, and how each client words them from
	 * its side). Team: the team the event is about; Subject: the player it is about (personal lines only they hear).
	 */
	static void Announce(const UObject* WorldContextObject, FName Event, int32 Team = INDEX_NONE, APlayerState* Subject = nullptr);

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

	UPROPERTY(Replicated)
	bool bHardcore = false;

	TArray<FName> Loadout;
	bool bPlayerLoadout = false;

	UFUNCTION(NetMulticast, Reliable)
	void MulticastAnnounce(FName Event, int32 Team, APlayerState* Subject);

private:
	void ApplyLimits();
	void WatchMatch();
	void OnElimination(FGameplayTag Channel, const FLyraVerbMessage& Message);
	void OnDamage(FGameplayTag Channel, const FLyraVerbMessage& Message);
	void ShowKiller(APlayerState* Killer, APlayerState* Victim) const;
	void WinGunGame(const FLyraVerbMessage& Message);

	int32 BotSkill = 1;
	int32 ScoreLimit = 0;
	int32 TimeLimit = 0;
	bool bTimeLimitApplied = false;
	FTimerHandle LimitsTimer;
	FTimerHandle WatchTimer;
	FTimerHandle ViewerTeamTimer;
	int32 ViewerTeam = INDEX_NONE;

	// Server: what WatchMatch saw last (clock, leading team, owners of Lyra's control points, match over).
	double LastClock = -1.0;
	bool bMatchStarted = false;
	bool bOneMinuteSaid = false;
	bool bMatchEndSaid = false;
	int32 Leader = INDEX_NONE;
	TMap<TWeakObjectPtr<AActor>, int32> PointOwners;
	int32 KillsInMatch = 0;
	TMap<TWeakObjectPtr<APlayerState>, TPair<double, int32>> RecentKills;  // last kill time, kills in quick succession

	// Client: announcer and the city around (not on a dedicated server).
	UPROPERTY(Transient)
	TObjectPtr<class UDHAnnouncer> Announcer;

	UPROPERTY(Transient)
	TObjectPtr<class UDHAmbience> Ambience;

	UPROPERTY(Replicated)
	TArray<FName> GunGame;

	bool bGunGameWon = false;
	FGameplayMessageListenerHandle EliminationListener;
	FGameplayMessageListenerHandle DamageListener;

	// Whether the last hit on a player (state) was a headshot, for the kill marker.
	TMap<TWeakObjectPtr<const AActor>, bool> LastHitHeadshot;

	// Lowest damage per weapon instance (distance falloff taken out): its body damage.
	TMap<TWeakObjectPtr<UObject>, float> WeaponBodyDamage;
};
