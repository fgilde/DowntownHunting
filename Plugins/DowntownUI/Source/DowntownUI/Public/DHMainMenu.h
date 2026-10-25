#pragma once

#include "CommonActivatableWidget.h"

#include "DHMainMenu.generated.h"

class UBorder;
class UDHCardButton;
class UDHMenuBackground;
class UTextBlock;
class UWidget;

/**
 * Main menu, built in code: entries on the right over a dark gradient, the city behind them as full-screen video
 * (UDHMenuBackground), a stage on the left (UDHShowcase: a hunter or the loadout's weapon per entry) and its own music
 * (/Game/DH/UI/Music, in place of Lyra's ambient front-end music). DEV TOOLS in developer mode. Every entry has its own slow camera move through the city (Content/Movies/Menu/DH_Menu_<Entry>.mp4,
 * made by Tools/Python/make_menu_backgrounds.py + Tools/Branding/make_menu_movies.py); moving the focus cross-fades to it.
 * Set as MainScreenClass of Lyra's front-end state component in place of W_LyraFrontEnd.
 */
UCLASS()
class DOWNTOWNUI_API UDHMainMenu : public UCommonActivatableWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	struct FEntry
	{
		FString Id;
		FText Title;
		FText Subtitle;
		TObjectPtr<UDHCardButton> Button;
		TObjectPtr<UBorder> Bar;
		TObjectPtr<UTextBlock> TitleText;
		TObjectPtr<UTextBlock> SubtitleText;
		float Focus = 0.f;  // animated 0..1
	};

	void AddEntry(class UVerticalBox* Column, const FString& Id, const FText& Title, const FText& Subtitle);
	void HandleEntryClicked(UDHCardButton* Button);
	void ShowBackground(const FString& Id);
	void ShowShowcase(const FString& Id);
	void StartMusic();
	void SilenceLyraMusic();
	void Open(const FString& Id);
	void RefreshLevel();

	TArray<FEntry> Entries;
	int32 LastFocused = INDEX_NONE;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> KeepAlive;  // buttons, gradient texture

	TWeakObjectPtr<UDHMenuBackground> Background;

	// Left stage: a hunter or a weapon springs in for the focused entry.
	UPROPERTY(Transient)
	TObjectPtr<class UDHShowcase> Showcase;
	FString LastRandomHunter;

	UPROPERTY(Transient)
	TObjectPtr<class UAudioComponent> Music;
	float MusicCheck = 0.f;

	// Player level badge, top left (FDHProgression).
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LevelText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> XPText;

	UPROPERTY(Transient)
	TObjectPtr<class UProgressBar> XPBar;
};
