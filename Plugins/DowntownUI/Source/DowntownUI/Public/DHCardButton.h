#pragma once

#include "Components/Button.h"

#include "DHCardButton.generated.h"

class UDHCardButton;

DECLARE_DELEGATE_OneParam(FDHCardClicked, UDHCardButton* /*Card*/);

/**
 * Button of the menu screens' cards that says which card was clicked (UButton's OnClicked has no sender,
 * and guessing via hover picked the card under the mouse when a gamepad pressed A).
 */
UCLASS()
class UDHCardButton : public UButton
{
	GENERATED_BODY()

public:
	UDHCardButton(const FObjectInitializer& ObjectInitializer);

	FString CardId;
	int32 CardSlot = 0;
	FDHCardClicked OnCardClicked;

	virtual void SynchronizeProperties() override;

private:
	UFUNCTION()
	void HandleClicked();
};
