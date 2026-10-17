#pragma once

#include "CommonActivatableWidget.h"

#include "DHMenuBackground.generated.h"

class UImage;
class UMediaPlayer;
class UMediaTexture;
class UWorld;

/**
 * Front-end background: the city as full-screen video, one looping camera move per main menu entry
 * (Content/Movies/Menu/DH_Menu_<Id>.mp4). Lives on the UI.Layer.Game layer under all menu screens, so it stays
 * visible behind Play, Character, Options and so on. Show() cross-fades to another clip. Removes itself when the
 * front end is left. Pushed by UDHMainMenu.
 */
UCLASS()
class DOWNTOWNUI_API UDHMenuBackground : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	void Show(const FString& Id);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	FString Shown;

	// Two video layers: the newest fades in over the other, then they swap roles.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMediaPlayer>> Players;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMediaTexture>> Textures;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> Layers;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> Sources;

	TWeakObjectPtr<UWorld> FrontEndWorld;
	int32 Front = 0;
	float Fade = 1.f;
};
