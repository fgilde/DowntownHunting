#pragma once

#include "CommonActivatableWidget.h"

#include "DHLoadoutScreen.generated.h"

class UDHCardButton;
class UEditableTextBox;
class UHorizontalBox;
class UImage;
class UProgressBar;
class UTextBlock;
class UVerticalBox;
class UWidget;

/**
 * Loadout builder, built in code. Left: the NumLoadouts loadouts as slots with their name, weapons and abilities; the
 * highlighted one is in use, picking a slot switches to it (also LB / RB, or Q / E). Right: rows for the primary and
 * secondary weapon (each with an attachment), the special ability, a perk, the second ability and three of the
 * killstreaks (picking a fourth drops the one picked longest ago, picking a chosen one drops it); below them a panel
 * explains whatever card has the focus (weapon stats, recharge and duration, unlock level). Y / R renames the loadout
 * in use (type a name, or roll a random one). Everything above the player's level is locked (FDHProgression). Picking a
 * weapon or ability that sits in the other slot swaps the two. Stored via UDHLoadoutSettings; in a match weapons apply
 * from the next spawn, abilities right away. Back (Esc / gamepad B) closes the screen (or the rename box).
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
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual bool NativeOnHandleBackAction() override;

private:
	enum class EKind : uint8 { Slot, Weapon, Attachment, Ability, Perk, Streak, Button };

	struct FCard
	{
		TObjectPtr<UDHCardButton> Button;
		EKind Kind = EKind::Weapon;
		int32 Entry = 0;  // UDHLoadoutSettings::EEntry, or the loadout index on slots
		FString Id;       // weapon, attachment, ability or perk id, empty for "none"; the action on buttons
		bool bLocked = false;
		TObjectPtr<UTextBlock> Title;   // slots: name
		TObjectPtr<UTextBlock> Detail;  // slots: weapons and abilities
	};

	void HandleCardClicked(UDHCardButton* Card);
	void HandleRenameCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UHorizontalBox* AddRow(UVerticalBox* Column, const FText& Title);
	UDHCardButton* AddCard(UHorizontalBox* Row, EKind Kind, int32 Entry, const FString& Id, UWidget* Content, bool bLocked);
	void AddSlots(UVerticalBox* Column);
	void AddWeaponRow(UVerticalBox* Column, const FText& Title, int32 Entry);
	void AddAttachmentRow(UVerticalBox* Column, int32 Entry);
	void AddAbilityRow(UVerticalBox* Column, const FText& Title, int32 Entry);
	void AddPerkRow(UVerticalBox* Column);
	void AddStreakRow(UVerticalBox* Column);
	UWidget* MakeDetails();
	UWidget* MakeRenameBox();
	UDHCardButton* MakeTextButton(const FString& Id, const FText& Label, int32 Size);

	bool IsChosen(const FCard& Card) const;
	void Pick(const FCard& Card);
	void Refresh();
	void ShowDetails(const FCard* Card);
	void SwitchLoadout(int32 Delta);
	void OpenRename();
	void CloseRename(bool bSave);
	void UpdatePrompts();

	TArray<FCard> Cards;
	TArray<FString> CurrentLoadout;
	int32 CurrentIndex = 0;
	int32 ShownDetails = INDEX_NONE;
	bool bPromptsForGamepad = false;
	int32 RandomName = 0;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> KeepAlive;  // buttons and labels referenced from Cards

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HeaderText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SummaryText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PromptText;

	// Details panel.
	UPROPERTY(Transient)
	TObjectPtr<UImage> DetailIcon;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailTitle;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailKind;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailText;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> StatsBox;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UProgressBar>> StatBars;

	// Rename box.
	UPROPERTY(Transient)
	TObjectPtr<UWidget> RenameBox;

	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> RenameText;

	UPROPERTY(Transient)
	TObjectPtr<UDHCardButton> RenameButton;
};
