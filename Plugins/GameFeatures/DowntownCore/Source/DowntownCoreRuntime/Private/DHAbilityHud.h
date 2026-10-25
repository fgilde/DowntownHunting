#pragma once

#include "Blueprint/UserWidget.h"

#include "DHAbilityHud.generated.h"

class UBorder;
class UImage;
class USizeBox;
class UTextBlock;
class UWidget;

/**
 * The local player's loadout abilities (bottom centre), arcade style, built in code: per ability a slanted tile with
 * its icon (/Game/DH/UI/Abilities), the button that fires it (the key in use: gamepad or keyboard), a shade that drains
 * while it recharges with the seconds left, a cyan frame while the effect runs and a pop with a sound when it is ready.
 * Full-screen effects while the own abilities run: a cyan shimmering frame while cloaked, a red heartbeat with speed lines
 * on adrenaline, sonar rings from the recon pulse, speed lines at the edges while sprinting.
 * In developer mode also the hint how to open the dev screen.
 */
UCLASS()
class UDHAbilityHud : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void UpdateScreenEffects(float DeltaTime);

	struct FSlot
	{
		TObjectPtr<UWidget> Box;
		TObjectPtr<UWidget> Tile;
		TObjectPtr<UBorder> Frame;
		TObjectPtr<UImage> Icon;
		TObjectPtr<USizeBox> Shade;
		TObjectPtr<UTextBlock> Seconds;
		TObjectPtr<UTextBlock> Key;
		TObjectPtr<UTextBlock> Name;
		TObjectPtr<UTextBlock> State;
		FString ShownAbility;
		bool bWasReady = true;
		float Pop = 0.f;
		float PopVelocity = 0.f;
	};

	FSlot Slots[2];

	// Full-screen effects of the own abilities and moves.
	UPROPERTY(Transient)
	TObjectPtr<UImage> Vignette;

	UPROPERTY(Transient)
	TObjectPtr<UImage> SpeedLines;

	UPROPERTY(Transient)
	TObjectPtr<UImage> Sonar;

	float Speed = 0.f;      // smoothed sprint look 0..1
	float SonarTime = 10.f;
	bool bWasRecon = false;
	float EffectTime = 0.f;
	float KeyRefresh = 0.f;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> KeepAlive;
};
