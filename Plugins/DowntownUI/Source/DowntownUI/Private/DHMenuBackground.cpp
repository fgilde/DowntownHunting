#include "DHMenuBackground.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Engine/Texture2D.h"
#include "FileMediaSource.h"
#include "HAL/FileManager.h"
#include "MediaPlayer.h"
#include "Misc/App.h"
#include "MediaTexture.h"
#include "Misc/Paths.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DHMenuBackground)

namespace
{
	/** Greyscale alpha mask (white, alpha from the function of the pixel's 0..1 position) as a texture. */
	template <typename TAlpha>
	UTexture2D* MakeMask(int32 Size, TAlpha Alpha)
	{
		UTexture2D* Texture = UTexture2D::CreateTransient(Size, Size, PF_B8G8R8A8);
		FColor* Pixels = static_cast<FColor*>(Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE));
		for (int32 Y = 0; Y < Size; ++Y)
		{
			for (int32 X = 0; X < Size; ++X)
			{
				Pixels[Y * Size + X] = FColor(255, 255, 255, uint8(255.f * FMath::Clamp(Alpha((X + 0.5f) / Size, (Y + 0.5f) / Size), 0.f, 1.f)));
			}
		}
		Texture->GetPlatformData()->Mips[0].BulkData.Unlock();
		Texture->UpdateResource();
		return Texture;
	}

	UImage* MaskImage(UWidgetTree* Tree, UTexture2D* Texture, const FLinearColor& Colour, float Tile)
	{
		UImage* Image = Tree->ConstructWidget<UImage>();
		FSlateBrush Brush;
		Brush.SetResourceObject(Texture);
		Brush.ImageSize = FVector2D(Tile > 0.f ? Tile : Texture->GetSizeX());
		Brush.Tiling = Tile > 0.f ? ESlateBrushTileType::Both : ESlateBrushTileType::NoTile;
		Image->SetBrush(Brush);
		Image->SetColorAndOpacity(Colour);
		return Image;
	}
}

void UDHMenuBackground::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	FrontEndWorld = GetWorld();

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
	WidgetTree->RootWidget = Root;
	auto Fill = [Root](UWidget* Widget, float Bleed = 0.f)
	{
		UOverlaySlot* LayerSlot = Root->AddChildToOverlay(Widget);
		LayerSlot->SetHorizontalAlignment(HAlign_Fill);
		LayerSlot->SetVerticalAlignment(VAlign_Fill);
		LayerSlot->SetPadding(FMargin(-Bleed));
	};
	Videos = WidgetTree->ConstructWidget<UOverlay>();
	Fill(Videos);

	// Comic dressing over the city: halftone dots, slowly travelling diagonal stripes, a dark vignette.
	UTexture2D* Dot = MakeMask(32, [](float X, float Y) { return 1.f - FMath::SmoothStep(0.18f, 0.24f, FVector2f(X - 0.5f, Y - 0.5f).Size()); });
	UTexture2D* Stripe = MakeMask(64, [](float X, float Y) { return FMath::Frac(X + Y) < 0.28f ? 1.f : 0.f; });
	UTexture2D* Vignette = MakeMask(256, [](float X, float Y) { return FMath::SmoothStep(0.42f, 0.95f, FVector2f(X - 0.5f, Y - 0.5f).Size() * 1.25f); });
	KeepAlive.Append({ Dot, Stripe, Vignette });
	Fill(MaskImage(WidgetTree, Dot, FLinearColor(1.f, 0.1f, 0.55f, 0.10f), 9.f));
	Stripes = MaskImage(WidgetTree, Stripe, FLinearColor(1.f, 0.82f, 0.f, 0.045f), 96.f);
	Fill(Stripes, 96.f);
	Fill(MaskImage(WidgetTree, Vignette, FLinearColor(0.f, 0.f, 0.02f, 0.85f), 0.f));

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
		Layer->SetColorAndOpacity(FLinearColor(0.62f, 0.52f, 0.85f));  // dusk tint: the menus' neon reads better
		Layer->SetRenderTransformPivot(FVector2D(0.35f, 0.5f));
		UOverlaySlot* LayerSlot = Videos->AddChildToOverlay(Layer);
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
	UImage* Layer = Layers[Front];
	Videos->RemoveChild(Layer);
	UOverlaySlot* LayerSlot = Videos->AddChildToOverlay(Layer);
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

	StripeTime += InDeltaTime;
	Stripes->SetRenderTranslation(FVector2D(FMath::Fmod(StripeTime * 14.f, 96.f), 0.f));

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
