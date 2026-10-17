#pragma once

#include "Blueprint/UserWidget.h"

#include "DHGunGameHud.generated.h"

class UTextBlock;

/** Gun Game progress of the local player (top left): current step, weapon and the next one. Built in code. */
UCLASS()
class UDHGunGameHud : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UPROPERTY()
	TObjectPtr<UTextBlock> StepText;

	UPROPERTY()
	TObjectPtr<UTextBlock> NextText;

	int32 ShownLevel = INDEX_NONE;
};
