#pragma once

#include "Components/ControllerComponent.h"

#include "DHCharacterSelectComponent.generated.h"

class APlayerController;

/**
 * Gives a controller its character (list and stored choice: UDHCharacterSettings in DowntownUI).
 * Players send their choice to the server at start and whenever they pick another one in the character screen;
 * bots get a random one. Works through Lyra's ULyraControllerComponent_CharacterParts on the same controller
 * (added by the experience), called by reflection because that class is not exported from LyraGame.
 */
UCLASS(meta = (BlueprintSpawnableComponent))
class UDHCharacterSelectComponent : public UControllerComponent
{
	GENERATED_BODY()

public:
	UDHCharacterSelectComponent(const FObjectInitializer& ObjectInitializer);

	/** Switch this player's character; applied to the current pawn right away. */
	UFUNCTION(Server, Reliable)
	void ServerSetCharacter(const FString& Id);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleCharacterChosen(APlayerController* Player, const FString& Id);
	void ApplyCharacter(const FString& Id);

	FString AppliedId;
	FDelegateHandle ChosenHandle;
};
