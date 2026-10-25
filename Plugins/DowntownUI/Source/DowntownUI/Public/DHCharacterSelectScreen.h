#pragma once

#include "CommonActivatableWidget.h"

#include "DHCharacterSelectScreen.generated.h"

class UAudioComponent;
class UButton;
class UDHCardButton;
class UImage;
class UMediaPlayer;
class UTextBlock;
class UWidget;

/**
 * Character selection: a grid of cards (splash art + name, plus "Random") built in code, no widget blueprint.
 * The focused or hovered character (else the chosen one) springs in as a big splash panel on the left, with its name,
 * its voice line (/Game/DH/UI/Voices/DH_Voice_<Id>) and, when there is one, its looping clip
 * (Content/Movies/Characters/DH_Char_<Id>.mp4) over the still.
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
	virtual void NativeDestruct() override;

private:
	void HandleCardClicked(class UDHCardButton* Card);

	UFUNCTION()
	void HandleBackClicked();

	UDHCardButton* MakeCard(const FString& Id, const FText& Label);
	void Select(const FString& Id);
	void RefreshHighlight();
	void ShowSplash(const FString& Id);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UDHCardButton>> Cards;

	TArray<FString> CardIds;
	FString CurrentId;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CurrentText;

	// Splash panel
	UPROPERTY(Transient)
	TObjectPtr<UWidget> Splash;

	UPROPERTY(Transient)
	TObjectPtr<UImage> SplashImage;

	UPROPERTY(Transient)
	TObjectPtr<UImage> SplashVideo;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SplashName;

	UPROPERTY(Transient)
	TObjectPtr<UMediaPlayer> VideoPlayer;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Voice;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> KeepAlive;

	FString SplashId = TEXT("-");
	float SplashOffset = 0.f;  // spring: slides in from the left
	float SplashVelocity = 0.f;
	float VideoFade = 0.f;
	double VoiceAllowedAt = 0.0;  // no voice spam while scrolling through the grid
};
