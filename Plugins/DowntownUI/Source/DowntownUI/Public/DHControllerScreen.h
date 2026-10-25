#pragma once

#include "CommonActivatableWidget.h"

#include "DHControllerScreen.generated.h"

class UDHCardButton;
class ULocalPlayer;
class UTextBlock;

/**
 * Gamepad button layout, built in code: one row per game action with its button; pick a row, press a button. A button
 * already used by another row swaps over to it. "Restore defaults" brings back the layout of the mapping assets.
 * Choices are stored in GameUserSettings.ini (Gamepad_<Action>=<Key>) and applied by changing the gamepad mappings of
 * the loaded input mapping contexts (Lyra's, ShooterCore's and DowntownCore's) in memory: ApplyBindings, called by the
 * main menu and when a player pawn sets up its input. Lyra's own options only rebind keyboard keys.
 */
UCLASS()
class DOWNTOWNUI_API UDHControllerScreen : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UDHControllerScreen(const FObjectInitializer& ObjectInitializer);

	/** Puts the stored layout into the mapping contexts and rebuilds the player's input. */
	static void ApplyBindings(const ULocalPlayer* Player);

	/** Logs the buttons the player's input really maps each action to (DH.PadDump). */
	static void LogBindings(const ULocalPlayer* Player);

	/** Name of a key for screens and the HUD: A, LB, RIGHT STICK PRESS (short: RS), E, Left Shift... */
	static FText KeyLabel(const FKey& Key, bool bShort = false);

	/** The key the player's input maps an action to (asset path), for the input type in use (gamepad or not). */
	static FText ActionKeyLabel(const ULocalPlayer* Player, const TCHAR* ActionPath, bool bShort = false);

	/** Called by the input capture: a gamepad button for the row waiting for one (none = cancelled). */
	void FinishCapture(const FKey& Key);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnDeactivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void HandleCardClicked(UDHCardButton* Card);
	void StopCapture();
	void Refresh();

	TArray<TObjectPtr<UDHCardButton>> Buttons;  // rows, then restore and back
	TArray<TObjectPtr<UTextBlock>> KeyTexts;
	int32 CapturingRow = INDEX_NONE;
	TSharedPtr<class IInputProcessor> Capture;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> KeepAlive;
};
