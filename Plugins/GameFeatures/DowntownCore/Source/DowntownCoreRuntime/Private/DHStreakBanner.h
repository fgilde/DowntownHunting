#pragma once

#include "Blueprint/UserWidget.h"

#include "DHStreakBanner.generated.h"

class UTextBlock;

/** Short banner at the top of the screen for a killstreak and its reward (DHKillstreaks), fades out by itself. */
UCLASS()
class UDHStreakBanner : public UUserWidget
{
	GENERATED_BODY()

public:
	void Show(const FText& Text, bool bReward);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UPROPERTY()
	TObjectPtr<UTextBlock> Label;

	float Age = 0.f;
	float Seconds = 2.5f;
};
