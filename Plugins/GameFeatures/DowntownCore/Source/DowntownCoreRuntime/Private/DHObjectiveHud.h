#pragma once

#include "Blueprint/UserWidget.h"

#include "DHObjectiveHud.generated.h"

class UCanvasPanel;
class UProgressBar;
class UTextBlock;

/**
 * Search & Destroy overlay for the local player: the team's task, a marker with distance over each bomb site and the
 * plant/defuse progress. Built in code, added to the viewport by UDHPlayerRulesComponent.
 */
UCLASS()
class UDHObjectiveHud : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UPROPERTY()
	TObjectPtr<UCanvasPanel> Canvas;

	UPROPERTY()
	TObjectPtr<UTextBlock> TaskText;

	UPROPERTY()
	TObjectPtr<UTextBlock> ProgressText;

	UPROPERTY()
	TObjectPtr<UProgressBar> ProgressBar;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> Markers;
};
