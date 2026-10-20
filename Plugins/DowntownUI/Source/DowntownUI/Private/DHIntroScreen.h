#pragma once

#include "Blueprint/UserWidget.h"

#include "DHIntroScreen.generated.h"

class UMediaPlayer;
class UMediaSoundComponent;
class UMediaTexture;

/**
 * The intro movie (Content/Movies/DH_Intro.mp4) over the main menu, once per start of the game. Any key, mouse button
 * or gamepad button skips it. Not a startup movie: those can only be skipped once the engine has finished loading,
 * which takes about as long as the movie.
 */
UCLASS()
class UDHIntroScreen : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Shows the intro if it has not run yet in this session (and -nomovie is not set). */
	static void PlayOnce(APlayerController* Player);

	void Finish();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UFUNCTION()
	void HandleEndReached();

	UPROPERTY(Transient)
	TObjectPtr<UMediaPlayer> Player;

	UPROPERTY(Transient)
	TObjectPtr<UMediaTexture> Texture;

	UPROPERTY(Transient)
	TObjectPtr<UMediaSoundComponent> Sound;

	UPROPERTY(Transient)
	TObjectPtr<UObject> Source;

	TSharedPtr<class FDHIntroSkipInput> SkipInput;
	float FadeOut = -1.f;
	bool bStarted = false;
};
