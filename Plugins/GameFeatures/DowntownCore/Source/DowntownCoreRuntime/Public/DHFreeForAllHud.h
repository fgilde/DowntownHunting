#pragma once

#include "Blueprint/UserWidget.h"

#include "DHFreeForAllHud.generated.h"

class UTextBlock;

/**
 * Free-for-all score at the top of the HUD (HUD.Slot.TeamScore, in place of Lyra's two-team score widget):
 * own kills, match clock, leader, and the winner once the match is over. Built in code.
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
};
