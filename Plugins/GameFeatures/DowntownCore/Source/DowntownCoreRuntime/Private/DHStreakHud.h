#pragma once

#include "Blueprint/UserWidget.h"

#include "DHStreakHud.generated.h"

class UBorder;
class UTextBlock;
class UVerticalBox;

/**
 * Killstreaks on the right edge, CoD style: the three the player took along (DHKillstreaks::ChosenStreaks), each with
 * its kill count, how far the current streak is, and READY with the button to call it in once earned. Keyboard: 4 / 5 / 6
 * call in the first, second, third; gamepad: D-pad left calls in the selected one, D-pad down selects the next (a newly
 * earned one is selected by itself). Strikes open the map to pick the spot (UDHStrikeMap). Added and driven by
 * UDHPlayerRulesComponent for the local player.
 */
UCLASS()
class UDHStreakHud : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Calls in the killstreak in a slot (0-2), or the selected one with INDEX_NONE. */
	void Activate(int32 Which);

	/** Selects the next killstreak (gamepad). */
	void SelectNext();

	/**
	 * A menu is open over the match (pause, loadout, strike map...): the HUD widgets added to the viewport sit above
	 * Lyra's layers, so they hide themselves while this is true.
	 */
	static bool IsMenuOpen(const APlayerController* Player);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	struct FTile
	{
		TObjectPtr<UBorder> Frame;
		TObjectPtr<UTextBlock> Kills;
		TObjectPtr<UTextBlock> Name;
		TObjectPtr<UTextBlock> State;
		TObjectPtr<UBorder> KeyBadge;
		TObjectPtr<UTextBlock> Key;
		int32 LastReady = INDEX_NONE;  // none seen yet
		float Pop = 0.f;
		float PopVelocity = 0.f;
	};

	TArray<FString> CurrentStreaks() const;

	FTile Tiles[3];
	int32 Selected = 0;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StreakText;
};
