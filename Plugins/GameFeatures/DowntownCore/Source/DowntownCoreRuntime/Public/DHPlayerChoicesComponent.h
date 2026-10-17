#pragma once

#include "Components/ControllerComponent.h"

#include "DHPlayerChoicesComponent.generated.h"

class APlayerController;

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

	/** Primary and secondary weapon; given from the next spawn. */
	UFUNCTION(Server, Reliable)
	void ServerSetLoadout(const TArray<FString>& NewLoadout);

	/** Server: the player's loadout, empty for bots and before the client sent it. */
	const TArray<FName>& GetLoadout() const { return Loadout; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleCharacterChosen(APlayerController* Player, const FString& Id);
	void HandleLoadoutChosen(APlayerController* Player, const TArray<FString>& NewLoadout);
	void ApplyCharacter(const FString& Id);

	FString AppliedId;
	TArray<FName> Loadout;
	FDelegateHandle ChosenHandle;
	FDelegateHandle LoadoutHandle;
};
