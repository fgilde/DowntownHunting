#pragma once

#include "CommonActivatableWidget.h"

#include "DHCharacterSelectScreen.generated.h"

class UButton;
class UTextBlock;

/**
 * Character selection: a grid of cards (portrait + name, plus "Random") built in code, no widget blueprint.
 * Picking a card stores the choice (UDHCharacterSelectComponent::SetPreferredCharacter) and, in a match,
 * tells the server so the character changes right away. Back (Esc / gamepad B) closes the screen.
 * Pushed onto UI.Layer.Menu from the main menu's "Character" button.
 */
UCLASS()
class UDHCharacterSelectScreen : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UDHCharacterSelectScreen(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void HandleCardClicked(class UDHCardButton* Card);

	UFUNCTION()
	void HandleBackClicked();

	UButton* MakeCard(const FString& Id, const FText& Label);
	void Select(const FString& Id);
	void RefreshHighlight();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> Cards;

	TArray<FString> CardIds;
	FString CurrentId;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CurrentText;
};
