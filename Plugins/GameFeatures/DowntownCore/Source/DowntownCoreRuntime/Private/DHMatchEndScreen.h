#pragma once

#include "Blueprint/UserWidget.h"

#include "DHMatchEndScreen.generated.h"

class UTextBlock;
class UVerticalBox;

/**
 * End of match for the local player (UDHFreeForAllScoring modes): VICTORY / DEFEAT, the winner, and the best players
 * with kills and deaths, fading in over the game. Built in code, shown by UDHFreeForAllHud until the next map loads.
 * With bLiveScoreboard it is the in-match scoreboard instead (all players by team, kills, deaths, K/D, refreshed live),
 * shown while the scoreboard input (Tab / gamepad Select) is held (UDHPlayerRulesComponent).
 */
UCLASS()
class UDHMatchEndScreen : public UUserWidget
{
	GENERATED_BODY()

public:
	bool bLiveScoreboard = false;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void Fill();
	void FillTable();

	UPROPERTY()
	TObjectPtr<UTextBlock> ResultText;

	UPROPERTY()
	TObjectPtr<UTextBlock> WinnerText;

	UPROPERTY()
	TObjectPtr<UTextBlock> XPText;

	UPROPERTY()
	TObjectPtr<UVerticalBox> Table;

	// Table texts, 4 per row (name, kills, deaths, K/D), row 0 is the header.
	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> Cells;

	float Age = 0.f;
	float RefreshTimer = 0.f;
	bool bFilled = false;
};
