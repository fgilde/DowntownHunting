#pragma once

#include "Blueprint/UserWidget.h"

#include "DHKillcam.generated.h"

class ACameraActor;
class APlayerState;
class UTextBlock;

/**
 * Killcam for the local player after being killed: a camera follows the killer from behind for a few seconds
 * (own camera actor, the killer's pawn camera only works for its own player) and a banner says who it was, how far
 * away and with how much health left. Started by UDHPlayerChoicesComponent::ClientShowKiller, ends by itself or when
 * the player respawns.
 */
UCLASS()
class UDHKillcam : public UUserWidget
{
	GENERATED_BODY()

public:
	void Start(APlayerState* Killer, float KillerHealth, float Distance);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

private:
	void Finish();

	UPROPERTY()
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY()
	TObjectPtr<UTextBlock> InfoText;

	UPROPERTY()
	TObjectPtr<ACameraActor> Camera;

	TWeakObjectPtr<APlayerState> KillerState;
	TWeakObjectPtr<APawn> DeadPawn;
	float Age = 0.f;
	bool bFollowing = false;
};
