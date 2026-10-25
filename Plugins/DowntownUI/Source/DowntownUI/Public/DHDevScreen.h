#pragma once

#include "CommonActivatableWidget.h"

#include "DHDevScreen.generated.h"

class UDHCardButton;
class ULocalPlayer;
class UTextBlock;

/**
 * Developer tools (only with fg.txt, FDHProgression::IsDevMode), built in code: switches stored as dev flags (no
 * cooldowns, god mode, endless ammo, read by DowntownCore on the host), slow motion and the FPS counter, and one-shot
 * actions through the console (bots, killstreak, level up, heal, respawn). Opened with F10 or by holding the gamepad's
 * View button and pressing Y, anywhere (InstallHotkey), and from the main menu entry DEV TOOLS.
 */
UCLASS()
class DOWNTOWNUI_API UDHDevScreen : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UDHDevScreen(const FObjectInitializer& ObjectInitializer);

	/** In dev mode: registers the F10 / View+Y hotkey once per run. */
	static void InstallHotkey();

	static void Open(ULocalPlayer* Player);

protected:
	virtual void NativeOnInitialized() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	struct FEntry
	{
		FString Id;
		FText Name;
		bool bToggle = false;
		TObjectPtr<UDHCardButton> Button;
		TObjectPtr<UTextBlock> State;
	};

	void HandleCardClicked(UDHCardButton* Card);
	bool IsOn(const FString& Id) const;
	void RefreshStates();

	TArray<FEntry> Entries;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> KeepAlive;

	// Switches that are console state, not stored: remembered for this run.
	static bool bSlowMotion;
	static bool bShowFps;
};
