#pragma once

#include "CommonActivatableWidget.h"

#include "DHLoadoutScreen.generated.h"

class UButton;
class UHorizontalBox;
class UTextBlock;
class UVerticalBox;

/**
 * Loadout builder, built in code: tabs for the NumLoadouts loadouts (the chosen tab is the one used), and rows for the
 * primary and secondary weapon (cards with the HUD weapon icons) with an attachment each, the special ability, a perk
 * and the second ability. Everything above the player's level is locked (FDHProgression). Picking a weapon or ability
 * that sits in the other slot swaps the two. Stored via UDHLoadoutSettings; in a match weapons apply from the next
 * spawn, abilities right away. Back (Esc / gamepad B) closes the screen.
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
	enum class EKind : uint8 { Tab, Weapon, Attachment, Ability, Perk };

	struct FCard
	{
		TObjectPtr<UButton> Button;
		EKind Kind = EKind::Weapon;
		int32 Entry = 0;  // UDHLoadoutSettings::EEntry, or the loadout index on tabs
		FString Id;       // weapon, attachment, ability or perk id, empty for "none"
		TObjectPtr<UTextBlock> Label;  // tabs only
	};

	void HandleCardClicked(class UDHCardButton* Card);

	UFUNCTION()
	void HandleBackClicked();

	UHorizontalBox* AddRow(UVerticalBox* Column, const FText& Title);
	class UDHCardButton* AddCard(UHorizontalBox* Row, EKind Kind, int32 Entry, const FString& Id, UWidget* Content, bool bLocked);
	void AddWeaponRow(UVerticalBox* Column, const FText& Title, int32 Entry);
	void AddAttachmentRow(UVerticalBox* Column, int32 Entry);
	void AddAbilityRow(UVerticalBox* Column, const FText& Title, int32 Entry);
	void AddPerkRow(UVerticalBox* Column);
	bool IsChosen(const FCard& Card) const;
	void Pick(const FCard& Card);
	void Refresh();

	TArray<FCard> Cards;
	TArray<FString> CurrentLoadout;
	int32 CurrentIndex = 0;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> KeepAlive;  // buttons and labels referenced from Cards

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SummaryText;
};
