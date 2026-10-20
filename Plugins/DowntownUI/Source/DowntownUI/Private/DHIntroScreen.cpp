#include "DHIntroScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "FileMediaSource.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "MediaPlayer.h"
#include "MediaSoundComponent.h"
#include "MediaTexture.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DHIntroScreen)

/** Any key, mouse or gamepad button skips the intro (before the menu sees it). */
class FDHIntroSkipInput : public IInputProcessor
{
public:
	explicit FDHIntroSkipInput(UDHIntroScreen* InScreen) : Screen(InScreen) {}

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override { return Skip(); }

	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override { return Skip(); }

	virtual const TCHAR* GetDebugName() const override { return TEXT("DHIntroSkip"); }

private:
	bool Skip()
	{
		if (UDHIntroScreen* Intro = Screen.Get())
		{
			Intro->Finish();
		}
		return true;  // the press only skips, it does not also select a menu entry
	}

	TWeakObjectPtr<UDHIntroScreen> Screen;
};

namespace DHIntro
{
	bool bPlayed = false;

	FString MoviePath() { return FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("Movies/DH_Intro.mp4")); }
}

void UDHIntroScreen::PlayOnce(APlayerController* Player)
{
	if (DHIntro::bPlayed || !Player || FParse::Param(FCommandLine::Get(), TEXT("nomovie")) || !IFileManager::Get().FileExists(*DHIntro::MoviePath()))
	{
		return;
	}
	DHIntro::bPlayed = true;
	CreateWidget<UDHIntroScreen>(Player)->AddToViewport(1000);
}

void UDHIntroScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
	WidgetTree->RootWidget = Root;
	UBorder* Black = WidgetTree->ConstructWidget<UBorder>();
	Black->SetBrushColor(FLinearColor::Black);
	UOverlaySlot* BlackSlot = Root->AddChildToOverlay(Black);
	BlackSlot->SetHorizontalAlignment(HAlign_Fill);
	BlackSlot->SetVerticalAlignment(VAlign_Fill);

	Player = NewObject<UMediaPlayer>(this);
	Player->PlayOnOpen = true;
	Player->OnEndReached.AddDynamic(this, &ThisClass::HandleEndReached);
	Texture = NewObject<UMediaTexture>(this);
	Texture->SetMediaPlayer(Player);
	Texture->UpdateResource();
	UImage* Video = WidgetTree->ConstructWidget<UImage>();
	Video->SetBrushResourceObject(Texture);
	UOverlaySlot* VideoSlot = Root->AddChildToOverlay(Video);
	VideoSlot->SetHorizontalAlignment(HAlign_Fill);
	VideoSlot->SetVerticalAlignment(VAlign_Fill);

	// Media players in UMG are silent: the sound plays through a component on the player controller.
	if (APlayerController* Owner = GetOwningPlayer())
	{
		Sound = NewObject<UMediaSoundComponent>(Owner);
		Sound->SetMediaPlayer(Player);
		Sound->RegisterComponent();
		Sound->Start();
	}
	UFileMediaSource* File = NewObject<UFileMediaSource>(this);
	File->SetFilePath(DHIntro::MoviePath());
	Source = File;
	Player->OpenSource(File);

	SkipInput = MakeShared<FDHIntroSkipInput>(this);
	FSlateApplication::Get().RegisterInputPreProcessor(SkipInput, 0);
}

void UDHIntroScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (FadeOut >= 0.f)
	{
		FadeOut += InDeltaTime;
		SetRenderOpacity(FMath::Clamp(1.f - FadeOut / 0.35f, 0.f, 1.f));
		if (FadeOut >= 0.35f)
		{
			RemoveFromParent();
		}
	}
}

void UDHIntroScreen::HandleEndReached()
{
	Finish();
}

void UDHIntroScreen::Finish()
{
	if (FadeOut >= 0.f)
	{
		return;
	}
	FadeOut = 0.f;
	Player->Close();
	if (SkipInput.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(SkipInput);
	}
}

void UDHIntroScreen::NativeDestruct()
{
	if (SkipInput.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(SkipInput);
	}
	SkipInput.Reset();
	if (Sound)
	{
		Sound->Stop();
		Sound->DestroyComponent();
		Sound = nullptr;
	}
	Super::NativeDestruct();
}
