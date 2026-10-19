#pragma once

#include "CommonActivatableWidget.h"

#include "DHLoadoutScreen.generated.h"

class UButton;
class UHorizontalBox;
class UTextBlock;

/**
 * Loadout builder: pick the primary and the secondary weapon (cards with the HUD weapon icons) and an attachment for
 * each, built in code. Weapons and attachments above the player's level are locked (FDHProgression). Picking a weapon that sits in the other slot swaps the two. Stored via UDHLoadoutSettings; in a match the
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
	void AddAttachmentRow(class UVerticalBox* Column, int32 SlotIndex);
	void Pick(int32 SlotIndex, const FString& WeaponId);
	void PickAttachment(int32 SlotIndex, const FString& AttachmentId);
	void Store(const FString (&Weapons)[2], const FString (&Attachments)[2]);
	void Refresh();

	struct FCard
	{
		TObjectPtr<UButton> Button;
		int32 SlotIndex = 0;
		FString WeaponId;      // or the attachment id on attachment cards
		bool bAttachment = false;
		bool bLocked = false;  // below the unlock level (FDHProgression)
	};
	TArray<FCard> Cards;
	TArray<FString> CurrentLoadout;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> CardButtons;  // keeps the buttons referenced for GC

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SummaryText;
};
