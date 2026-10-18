#pragma once

#include "Blueprint/UserWidget.h"

#include "DHFreeForAllHud.generated.h"

class UHorizontalBox;
class UTextBlock;

/**
 * Match score at the top of the HUD (HUD.Slot.TeamScore, in place of Lyra's two-team score widget), for
 * UDHFreeForAllScoring: own kills, match clock, leader; for UDHTeamScoring: every team's kills in its colour and the clock.
 * Shows the winner once the match is over. Built in code.
 */
UCLASS()
class UDHFreeForAllHud : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UPROPERTY()
	TObjectPtr<UTextBlock> OwnText;

	UPROPERTY()
	TObjectPtr<UTextBlock> ClockText;

	UPROPERTY()
	TObjectPtr<UTextBlock> LeaderText;

	UPROPERTY()
	TObjectPtr<UHorizontalBox> TeamRow;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> TeamTexts;

	// Full-screen result, shown once the match is over.
	UPROPERTY()
	TObjectPtr<UUserWidget> EndScreen;

	UTextBlock* MakeText(int32 Size, const FLinearColor& Colour, float MinWidth) const;
	void UpdateTeams();
};
