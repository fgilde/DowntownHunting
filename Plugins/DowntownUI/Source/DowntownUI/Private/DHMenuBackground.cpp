#include "DHMenuBackground.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "FileMediaSource.h"
#include "HAL/FileManager.h"
#include "MediaPlayer.h"
#include "Misc/App.h"
#include "MediaTexture.h"
#include "Misc/Paths.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DHMenuBackground)

void UDHMenuBackground::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	FrontEndWorld = GetWorld();

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
	WidgetTree->RootWidget = Root;
	for (int32 Index = 0; Index < 2; ++Index)
	{
		UMediaPlayer* Player = NewObject<UMediaPlayer>(this);
		Player->SetLooping(true);
		Player->PlayOnOpen = true;
		UMediaTexture* Texture = NewObject<UMediaTexture>(this);
		Texture->SetMediaPlayer(Player);
		Texture->UpdateResource();
		UImage* Layer = WidgetTree->ConstructWidget<UImage>();
		Layer->SetBrushResourceObject(Texture);
		Layer->SetRenderOpacity(0.f);
		Layer->SetRenderTransformPivot(FVector2D(0.35f, 0.5f));
		UOverlaySlot* LayerSlot = Root->AddChildToOverlay(Layer);
		LayerSlot->SetHorizontalAlignment(HAlign_Fill);
		LayerSlot->SetVerticalAlignment(VAlign_Fill);
		Players.Add(Player);
		Textures.Add(Texture);
		Layers.Add(Layer);
	}
}

void UDHMenuBackground::Show(const FString& Id)
{
	if (Id == Shown)
	{
		return;
	}
	const FString Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("Movies/Menu") / (TEXT("DH_Menu_") + Id + TEXT(".mp4")));
	if (!IFileManager::Get().FileExists(*Path))
	{
		return;  // no clip for this entry: keep the current one
	}
	Shown = Id;
	Front = 1 - Front;
	Fade = 0.f;

	// Draw the new clip above the old one.
	UOverlay* Root = Cast<UOverlay>(WidgetTree->RootWidget);
	UImage* Layer = Layers[Front];
	Root->RemoveChild(Layer);
	UOverlaySlot* LayerSlot = Root->AddChildToOverlay(Layer);
	LayerSlot->SetHorizontalAlignment(HAlign_Fill);
	LayerSlot->SetVerticalAlignment(VAlign_Fill);

	UFileMediaSource* Source = NewObject<UFileMediaSource>(this);
	Source->SetFilePath(Path);
	Sources.Add(Source);
	Players[Front]->OpenSource(Source);
}

void UDHMenuBackground::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (GetOwningPlayer() && GetOwningPlayer()->GetWorld() != FrontEndWorld.Get())
	{
		for (UMediaPlayer* Player : Players)
		{
			Player->Close();
		}
		DeactivateWidget();  // a match was loaded: the front end is gone
		return;
	}

	// Recording with a fixed time step (DH.Record): game time runs slower than real time, so the clip plays slower by
	// the same factor and stays in step with the recorded frames.
	if (FApp::UseFixedTimeStep() && Players[Front]->IsPlaying())
	{
		const double Now = FPlatformTime::Seconds();
		if (LastRealTime > 0.0)
		{
			const float Wanted = FMath::Clamp(InDeltaTime / float(Now - LastRealTime), 0.02f, 1.f);
			RecordRate = FMath::Lerp(RecordRate, Wanted, 0.3f);
			Players[Front]->SetRate(RecordRate);
		}
		LastRealTime = Now;
	}

	// The newest clip fades in and settles from a slight zoom once its player delivers frames.
	if (Players[Front]->IsPlaying())
	{
		Fade = FMath::FInterpConstantTo(Fade, 1.f, InDeltaTime, 1.4f);
	}
	const float Ease = FMath::InterpEaseInOut(0.f, 1.f, Fade, 2.f);
	Layers[Front]->SetRenderOpacity(Ease);
	Layers[Front]->SetRenderScale(FVector2D(1.f + 0.07f * (1.f - Ease)));
	if (Fade >= 1.f && Players[1 - Front]->IsPlaying())
	{
		Players[1 - Front]->Pause();
		Layers[1 - Front]->SetRenderOpacity(0.f);
	}
	else if (Fade < 1.f)
	{
		Layers[1 - Front]->SetRenderOpacity(1.f);
	}
}
