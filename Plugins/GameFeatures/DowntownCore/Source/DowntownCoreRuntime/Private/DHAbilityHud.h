#pragma once

#include "Blueprint/UserWidget.h"

#include "DHAbilityHud.generated.h"

class UProgressBar;
class UTextBlock;
class UWidget;

/** The local player's loadout abilities (bottom centre): name, recharge bar, ready / active / seconds left. Built in code. */
UCLASS()
class UDHAbilityHud : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UPROPERTY()
	TObjectPtr<UWidget> Boxes[2];

	UPROPERTY()
	TObjectPtr<UTextBlock> Names[2];

	UPROPERTY()
	TObjectPtr<UTextBlock> States[2];

	UPROPERTY()
	TObjectPtr<UProgressBar> Bars[2];
};
