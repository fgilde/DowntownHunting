#pragma once

#include "Blueprint/UserWidget.h"

#include "DHMatchEndScreen.generated.h"

class UTextBlock;
class UVerticalBox;

/**
 * End of match for the local player (UDHFreeForAllScoring modes): VICTORY / DEFEAT, the winner, and the best players
 * with kills and deaths, fading in over the game. Built in code, shown by UDHFreeForAllHud until the next map loads.
 */
UCLASS()
class UDHMatchEndScreen : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void Fill();

	UPROPERTY()
	TObjectPtr<UTextBlock> ResultText;

	UPROPERTY()
	TObjectPtr<UTextBlock> WinnerText;

	UPROPERTY()
	TObjectPtr<UVerticalBox> Table;

	float Age = 0.f;
	bool bFilled = false;
};
