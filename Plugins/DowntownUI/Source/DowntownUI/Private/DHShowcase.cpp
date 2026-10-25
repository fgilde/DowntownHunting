#include "DHShowcase.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "DHScreenStyle.h"
#include "Engine/Texture2D.h"
#include "FileMediaSource.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "MediaPlayer.h"
#include "MediaTexture.h"
#include "Misc/Paths.h"
#include "Sound/SoundBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DHShowcase)

namespace
{
	const FVector2D PortraitSize(540.f, 720.f);  // splash art is 3:4
	const FVector2D WideSize(800.f, 450.f);      // menu art is 16:9

	UTextBlock* BigText(UWidgetTree* Tree, int32 Size, const FLinearColor& Colour, int32 Outline, const FLinearColor& OutlineColour)
	{
		UTextBlock* Text = DHScreenStyle::MakeText(Tree, TEXT("Orbitron"), Size, Colour);
		FSlateFontInfo Font = Text->GetFont();
		Font.OutlineSettings.OutlineSize = Outline;
		Font.OutlineSettings.OutlineColor = OutlineColour;
		Text->SetFont(Font);
		return Text;
	}
}

void UDHShowcase::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::HitTestInvisible);

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
	WidgetTree->RootWidget = Root;

	// Huge word drifting behind everything.
	Word = BigText(WidgetTree, 170, FLinearColor(1.f, 1.f, 1.f, 0.07f), 3, FLinearColor(1.f, 1.f, 1.f, 0.35f));
	Word->SetShadowOffset(FVector2D::ZeroVector);
	Word->SetRenderTransformAngle(-8.f);
	UOverlaySlot* WordSlot = Root->AddChildToOverlay(Word);
	WordSlot->SetHorizontalAlignment(HAlign_Left);
	WordSlot->SetVerticalAlignment(VAlign_Bottom);
	WordSlot->SetPadding(FMargin(0, 0, 0, 40));

	// Colour slab behind the panel.
	USizeBox* SlabBox = WidgetTree->ConstructWidget<USizeBox>();
	SlabBox->SetWidthOverride(470.f);
	SlabBox->SetHeightOverride(1400.f);
	Slab = WidgetTree->ConstructWidget<UBorder>();
	SlabBox->AddChild(Slab);
	SlabBox->SetRenderTransformAngle(12.f);
	UOverlaySlot* SlabSlot = Root->AddChildToOverlay(SlabBox);
	SlabSlot->SetHorizontalAlignment(HAlign_Left);
	SlabSlot->SetVerticalAlignment(VAlign_Center);
	SlabSlot->SetPadding(FMargin(170, 0, 0, 0));

	// Comic panel: thick black frame, picture, clip on top, caption across the bottom.
	Panel = WidgetTree->ConstructWidget<USizeBox>();
	UBorder* Frame = WidgetTree->ConstructWidget<UBorder>();
	Frame->SetBrushColor(FLinearColor::Black);
	Frame->SetPadding(FMargin(7.f));
	Panel->AddChild(Frame);
	UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>();
	Frame->SetContent(Layers);
	Picture = WidgetTree->ConstructWidget<UImage>();
	UOverlaySlot* PictureSlot = Layers->AddChildToOverlay(Picture);
	PictureSlot->SetHorizontalAlignment(HAlign_Fill);
	PictureSlot->SetVerticalAlignment(VAlign_Fill);
	Player = NewObject<UMediaPlayer>(this);
	Player->SetLooping(true);
	Player->PlayOnOpen = true;
	UMediaTexture* VideoTexture = NewObject<UMediaTexture>(this);
	VideoTexture->SetMediaPlayer(Player);
	VideoTexture->UpdateResource();
	KeepAlive.Add(VideoTexture);
	Video = WidgetTree->ConstructWidget<UImage>();
	Video->SetBrushResourceObject(VideoTexture);
	Video->SetRenderOpacity(0.f);
	UOverlaySlot* VideoSlot = Layers->AddChildToOverlay(Video);
	VideoSlot->SetHorizontalAlignment(HAlign_Fill);
	VideoSlot->SetVerticalAlignment(VAlign_Fill);
	Caption = BigText(WidgetTree, 58, DHScreenStyle::Accent, 5, FLinearColor::Black);
	Caption->SetShadowOffset(FVector2D(5, 5));
	Caption->SetRenderTransformAngle(-5.f);
	UOverlaySlot* CaptionSlot = Layers->AddChildToOverlay(Caption);
	CaptionSlot->SetHorizontalAlignment(HAlign_Left);
	CaptionSlot->SetVerticalAlignment(VAlign_Bottom);
	CaptionSlot->SetPadding(FMargin(24, 0, 0, 22));
	Panel->SetRenderTransformPivot(FVector2D(0.5f, 1.f));
	UOverlaySlot* PanelSlot = Root->AddChildToOverlay(Panel);
	PanelSlot->SetHorizontalAlignment(HAlign_Left);
	PanelSlot->SetVerticalAlignment(VAlign_Center);
	PanelSlot->SetPadding(FMargin(90, 60, 0, 0));
}

void UDHShowcase::ShowCharacter(const FString& Id, const FText& InWord, const FLinearColor& SlabColour)
{
	const FString Name = Id.IsEmpty() ? TEXT("Random") : Id;
	UTexture2D* Splash = LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("/Game/DH/UI/Splash/T_DH_Splash_%s.T_DH_Splash_%s"), *Name, *Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
	const FString Clip = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("Movies/Characters") / FString::Printf(TEXT("DH_Char_%s.mp4"), *Name));
	Present(Splash, Clip, FText::FromString(Id.IsEmpty() ? TEXT("???") : Id.ToUpper()), InWord, SlabColour, /*bWide=*/ false);
}

void UDHShowcase::ShowArt(UTexture2D* Art, const FText& InCaption, const FText& InWord, const FLinearColor& SlabColour)
{
	Present(Art, FString(), InCaption, InWord, SlabColour, /*bWide=*/ true);
}

void UDHShowcase::Present(UTexture2D* InPicture, const FString& Clip, const FText& InCaption, const FText& InWord, const FLinearColor& SlabColour, bool bWide)
{
	const FString Key = GetPathNameSafe(InPicture) + InCaption.ToString();
	if (Key == Shown)
	{
		return;
	}
	Shown = Key;
	const FVector2D Size = bWide ? WideSize : PortraitSize;
	Panel->SetWidthOverride(Size.X);
	Panel->SetHeightOverride(Size.Y);
	Picture->SetBrushFromTexture(InPicture);
	Picture->SetColorAndOpacity(InPicture ? FLinearColor::White : FLinearColor::Transparent);
	Caption->SetText(InCaption);
	Word->SetText(InWord);
	Slab->SetBrushColor(SlabColour);
	Offset = -900.f;
	Velocity = 0.f;
	Time = 0.f;
	if (USoundBase* Whoosh = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/MetaSounds/sfx_MeleeWhoosh_nl_meta_Preset.sfx_MeleeWhoosh_nl_meta_Preset")))
	{
		UGameplayStatics::PlaySound2D(this, Whoosh, 0.3f, FMath::FRandRange(1.2f, 1.4f));
	}

	VideoFade = 0.f;
	Player->Close();
	if (!Clip.IsEmpty() && IFileManager::Get().FileExists(*Clip))
	{
		UFileMediaSource* Source = NewObject<UFileMediaSource>(this);
		Source->SetFilePath(Clip);
		KeepAlive.Add(Source);
		Player->OpenSource(Source);
	}
}

void UDHShowcase::Stop()
{
	Player->Close();
}

void UDHShowcase::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Time += InDeltaTime;

	// Springs in from the left with an overshoot; then hovers a little.
	const float Step = FMath::Min(InDeltaTime, 1.f / 30.f);
	Velocity += -Offset * 170.f * Step - Velocity * 13.f * Step;
	Offset += Velocity * Step;
	FWidgetTransform Transform;
	Transform.Translation = FVector2D(Offset, 6.f * FMath::Sin(Time * 1.6f));
	Transform.Angle = Offset * 0.012f + 0.6f * FMath::Sin(Time * 0.9f);
	Transform.Shear = FVector2D(-4.f, 0.f);
	Panel->SetRenderTransform(Transform);
	Panel->SetRenderOpacity(FMath::Clamp(1.f + Offset / 600.f, 0.f, 1.f));
	Slab->GetParent()->SetRenderTranslation(FVector2D(Offset * 0.55f, 0.f));
	Word->SetRenderTranslation(FVector2D(Offset * 0.25f + 24.f * FMath::Sin(Time * 0.5f), 0.f));
	VideoFade = FMath::FInterpConstantTo(VideoFade, Player->IsPlaying() ? 1.f : 0.f, InDeltaTime, 4.f);
	Video->SetRenderOpacity(VideoFade);
}
