#pragma once

#include "CommonActivatableWidget.h"

#include "DHLoadoutScreen.generated.h"

class UButton;
class UHorizontalBox;
class UTextBlock;

/**
 * Loadout builder: pick the primary and the secondary weapon (cards with the HUD weapon icons), built in code.
 * Picking a weapon that sits in the other slot swaps the two. Stored via UDHLoadoutSettings; in a match the
 * new loadout applies from the next spawn. Back (Esc / gamepad B) closes the screen.
 */
UCLASS()
class UDHLoadoutScreen : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UDHLoadoutScreen(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void HandleCardClicked(class UDHCardButton* Card);

	UFUNCTION()
	void HandleBackClicked();

	void AddRow(class UVerticalBox* Column, const FText& Title, int32 SlotIndex);
	void Pick(int32 SlotIndex, const FString& WeaponId);
	void Refresh();

	struct FCard
	{
		TObjectPtr<UButton> Button;
		int32 SlotIndex = 0;
		FString WeaponId;
	};
	TArray<FCard> Cards;
	TArray<FString> CurrentLoadout;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> CardButtons;  // keeps the buttons referenced for GC

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SummaryText;
};
