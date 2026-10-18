#pragma once

#include "Components/ControllerComponent.h"
#include "Engine/NetSerialization.h"

#include "DHPlayerChoicesComponent.generated.h"

class APlayerController;
class APlayerState;

/**
 * Carries a player's menu choices to the server: character (UDHCharacterSettings) and loadout (UDHLoadoutSettings).
 * Players send both at start and whenever they change them in the menu screens; bots get a random character.
 * The character is applied through Lyra's ULyraControllerComponent_CharacterParts on the same controller
 * (added by the experience), called by reflection because that class is not exported from LyraGame.
 * The loadout is read by UDHGameRulesComponent when the mode's rule is Loadout=Player.
 */
UCLASS(meta = (BlueprintSpawnableComponent))
class UDHPlayerChoicesComponent : public UControllerComponent
{
	GENERATED_BODY()

public:
	UDHPlayerChoicesComponent(const FObjectInitializer& ObjectInitializer);

	/** Switch this player's character; applied to the current pawn right away. */
	UFUNCTION(Server, Reliable)
	void ServerSetCharacter(const FString& Id);

	/**
	 * Player name (UDHCharacterSettings::GetPlayerName). With an empty name the server only replaces names that start
	 * with OfflineName (the computer name the offline online subsystem uses) by a "Hunter <number>" name.
	 */
	UFUNCTION(Server, Reliable)
	void ServerSetPlayerName(const FString& Name, const FString& OfflineName);

	/** Killcam on the victim's machine (UDHKillcam): who killed this player, with how much health left, from how far. */
	UFUNCTION(Client, Reliable)
	void ClientShowKiller(APlayerState* Killer, float KillerHealth, float Distance);

	/** Kill marker / headshot on the shooter's screen (UDHHitMarker). */
	UFUNCTION(Client, Unreliable)
	void ClientHitFeedback(bool bKill, bool bHeadshot);

	/** Killstreak banner on the killer's screen (DHKillstreaks::EReward as Reward). */
	UFUNCTION(Client, Reliable)
	void ClientStreak(int32 Streak, uint8 Reward);

	/** Moves of DHMovement, done on the owning client first and repeated here on the server. */
	UFUNCTION(Server, Reliable)
	void ServerSlide();

	UFUNCTION(Server, Reliable)
	void ServerWallJump(FVector_NetQuantizeNormal WallNormal);

	/** Primary and secondary weapon; given from the next spawn. */
	UFUNCTION(Server, Reliable)
	void ServerSetLoadout(const TArray<FString>& NewLoadout);

	/** Server: the player's loadout, empty for bots and before the client sent it. */
	const TArray<FName>& GetLoadout() const { return Loadout; }

	/** Server: attachment chosen for a weapon of the loadout (ExtMag, Laser, Barrel), None without one. */
	FName GetAttachment(FName Weapon) const { return Attachments.FindRef(Weapon); }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleCharacterChosen(APlayerController* Player, const FString& Id);
	void HandleLoadoutChosen(APlayerController* Player, const TArray<FString>& NewLoadout);
	void ApplyCharacter(const FString& Id);

	double LastWallJump = -1.0;

	UPROPERTY(Transient)
	TObjectPtr<class UDHHitMarker> HitMarker;
	FString AppliedId;
	TArray<FName> Loadout;
	TMap<FName, FName> Attachments;
	FDelegateHandle ChosenHandle;
	FDelegateHandle LoadoutHandle;
};
