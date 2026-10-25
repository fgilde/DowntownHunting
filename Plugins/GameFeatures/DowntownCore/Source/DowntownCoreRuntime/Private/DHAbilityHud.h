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
	float KeyRefresh = 0.f;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> KeepAlive;
};
