#include "DHCharacterSelectScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Components/AudioComponent.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "DHCardButton.h"
#include "DHCharacterSettings.h"
#include "DHScreenStyle.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "FileMediaSource.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "MediaPlayer.h"
#include "MediaTexture.h"
#include "Misc/Paths.h"
#include "Sound/SoundBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DHCharacterSelectScreen)

namespace
{
	const int32 Columns = 5;
	const FLinearColor CardColor = DHScreenStyle::Card;
	const FVector2D SplashSize(660.f, 880.f);  // the art is 3:4

	/** Splash art (arcade comic style), else the old portrait photo. */
	UTexture2D* Picture(const FString& Id)
	{
		const FString Name = Id.IsEmpty() ? TEXT("Random") : Id;
		for (const TCHAR* Prefix : { TEXT("/Game/DH/UI/Splash/T_DH_Splash_"), TEXT("/Game/DH/UI/Portraits/T_DH_Portrait_") })
		{
			const FString Asset = FString(Prefix) + Name;
			if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *(Asset + TEXT(".") + FPaths::GetCleanFilename(Asset)), nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				return Texture;
			}
		}
		return nullptr;
	}

	FString ClipPath(const FString& Id)
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("Movies/Characters") / FString::Printf(TEXT("DH_Char_%s.mp4"), Id.IsEmpty() ? TEXT("Random") : *Id));
	}
}

UDHCharacterSelectScreen::UDHCharacterSelectScreen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsBackHandler = true;
	bIsModal = true;
}

void UDHCharacterSelectScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
	WidgetTree->RootWidget = Root;

	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>();
	Dim->SetBrushColor(DHScreenStyle::Dim);
	UOverlaySlot* DimSlot = Root->AddChildToOverlay(Dim);
	DimSlot->SetHorizontalAlignment(HAlign_Fill);
	DimSlot->SetVerticalAlignment(VAlign_Fill);

	// Splash panel, left: still picture, the clip above it, the name across the bottom.
	USizeBox* SplashBox = WidgetTree->ConstructWidget<USizeBox>();
	SplashBox->SetWidthOverride(SplashSize.X);
	SplashBox->SetHeightOverride(SplashSize.Y);
	UBorder* Frame = WidgetTree->ConstructWidget<UBorder>();  // comic panel: thick black frame
	Frame->SetBrushColor(FLinearColor::Black);
	Frame->SetPadding(FMargin(7.f));
	SplashBox->AddChild(Frame);
	UOverlay* SplashLayers = WidgetTree->ConstructWidget<UOverlay>();
	Frame->SetContent(SplashLayers);
	auto Fill = [](UOverlaySlot* LayerSlot)
	{
		LayerSlot->SetHorizontalAlignment(HAlign_Fill);
		LayerSlot->SetVerticalAlignment(VAlign_Fill);
	};
	SplashImage = WidgetTree->ConstructWidget<UImage>();
	Fill(SplashLayers->AddChildToOverlay(SplashImage));
	VideoPlayer = NewObject<UMediaPlayer>(this);
	VideoPlayer->SetLooping(true);
	VideoPlayer->PlayOnOpen = true;
	UMediaTexture* VideoTexture = NewObject<UMediaTexture>(this);
	VideoTexture->SetMediaPlayer(VideoPlayer);
	VideoTexture->UpdateResource();
	KeepAlive.Add(VideoTexture);
	SplashVideo = WidgetTree->ConstructWidget<UImage>();
	SplashVideo->SetBrushResourceObject(VideoTexture);
	SplashVideo->SetRenderOpacity(0.f);
	Fill(SplashLayers->AddChildToOverlay(SplashVideo));
	SplashName = DHScreenStyle::MakeText(WidgetTree, TEXT("Orbitron"), 72, DHScreenStyle::Accent);
	FSlateFontInfo NameFont = SplashName->GetFont();
	NameFont.OutlineSettings.OutlineSize = 5;  // readable on the bright art
	NameFont.OutlineSettings.OutlineColor = FLinearColor::Black;
	SplashName->SetFont(NameFont);
	SplashName->SetShadowOffset(FVector2D(5, 5));
	SplashName->SetShadowColorAndOpacity(DHScreenStyle::Selected);
	SplashName->SetRenderTransformAngle(-5.f);
	UOverlaySlot* NameSlot = SplashLayers->AddChildToOverlay(SplashName);
	NameSlot->SetHorizontalAlignment(HAlign_Left);
	NameSlot->SetVerticalAlignment(VAlign_Bottom);
	NameSlot->SetPadding(FMargin(28, 0, 0, 30));
	Splash = SplashBox;
	Splash->SetRenderTransformPivot(FVector2D(0.5f, 1.f));
	UOverlaySlot* SplashSlot = Root->AddChildToOverlay(SplashBox);
	SplashSlot->SetHorizontalAlignment(HAlign_Left);
	SplashSlot->SetVerticalAlignment(VAlign_Center);
	SplashSlot->SetPadding(FMargin(70, 0, 0, 0));

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	UOverlaySlot* ColumnSlot = Root->AddChildToOverlay(Column);
	ColumnSlot->SetHorizontalAlignment(HAlign_Right);
	ColumnSlot->SetVerticalAlignment(VAlign_Center);
	ColumnSlot->SetPadding(FMargin(0, 0, 80, 0));

	UTextBlock* Title = DHScreenStyle::MakeText(WidgetTree, TEXT("Orbitron"), 44, DHScreenStyle::Accent);
	Title->SetText(NSLOCTEXT("DowntownHunting", "ChooseHunter", "CHOOSE YOUR HUNTER"));
	Title->SetShadowOffset(FVector2D(4, 4));
	Title->SetShadowColorAndOpacity(DHScreenStyle::Selected);
	Title->SetRenderTransformAngle(-2.f);
	Column->AddChildToVerticalBox(Title)->SetHorizontalAlignment(HAlign_Center);

	CurrentText = DHScreenStyle::MakeText(WidgetTree, TEXT("NotoSans"), 18, DHScreenStyle::Text);
	UVerticalBoxSlot* CurrentSlot = Column->AddChildToVerticalBox(CurrentText);
	CurrentSlot->SetHorizontalAlignment(HAlign_Center);
	CurrentSlot->SetPadding(FMargin(0, 6, 0, 24));

	UUniformGridPanel* Grid = WidgetTree->ConstructWidget<UUniformGridPanel>();
	Grid->SetSlotPadding(FMargin(8));
	Column->AddChildToVerticalBox(Grid)->SetHorizontalAlignment(HAlign_Center);

	TArray<TPair<FString, FText>> Entries;
	Entries.Emplace(FString(), NSLOCTEXT("DowntownHunting", "Random", "RANDOM"));
	for (const FDHCharacterEntry& Entry : UDHCharacterSettings::GetCharacters())
	{
		Entries.Emplace(Entry.Id, FText::FromString(Entry.Id.ToUpper()));
	}
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		UButton* Card = MakeCard(Entries[Index].Key, Entries[Index].Value);
		UUniformGridSlot* CardSlot = Grid->AddChildToUniformGrid(Card, Index / Columns, Index % Columns);
		CardSlot->SetHorizontalAlignment(HAlign_Fill);
		CardSlot->SetVerticalAlignment(VAlign_Fill);
	}

	UButton* Back = WidgetTree->ConstructWidget<UButton>();
	Back->SetBackgroundColor(CardColor);
	UTextBlock* BackText = WidgetTree->ConstructWidget<UTextBlock>();
	BackText->SetText(NSLOCTEXT("DowntownHunting", "Back", "BACK"));
	BackText->SetFont(DHScreenStyle::Font(TEXT("Orbitron"), 20));
	Back->AddChild(BackText);
	Back->OnClicked.AddDynamic(this, &ThisClass::HandleBackClicked);
	UVerticalBoxSlot* BackSlot = Column->AddChildToVerticalBox(Back);
	BackSlot->SetHorizontalAlignment(HAlign_Center);
	BackSlot->SetPadding(FMargin(0, 28, 0, 0));
}

UDHCardButton* UDHCharacterSelectScreen::MakeCard(const FString& Id, const FText& Label)
{
	UDHCardButton* Card = WidgetTree->ConstructWidget<UDHCardButton>();
	Card->SetBackgroundColor(CardColor);
	Card->CardId = Id;
	Card->OnCardClicked.BindUObject(this, &ThisClass::HandleCardClicked);

	UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
	USizeBox* PictureBox = WidgetTree->ConstructWidget<USizeBox>();
	PictureBox->SetWidthOverride(120.f);
	PictureBox->SetHeightOverride(160.f);
	UImage* Image = WidgetTree->ConstructWidget<UImage>();
	if (UTexture2D* Texture = Picture(Id))
	{
		Image->SetBrushFromTexture(Texture);
	}
	else
	{
		Image->SetColorAndOpacity(FLinearColor(0.12f, 0.11f, 0.1f));
	}
	PictureBox->AddChild(Image);
	Content->AddChildToVerticalBox(PictureBox);

	UTextBlock* Name = DHScreenStyle::MakeText(WidgetTree, TEXT("Orbitron"), 15, DHScreenStyle::Text);
	Name->SetText(Label);
	UVerticalBoxSlot* NameSlot = Content->AddChildToVerticalBox(Name);
	NameSlot->SetHorizontalAlignment(HAlign_Center);
	NameSlot->SetPadding(FMargin(0, 6, 0, 2));

	Card->AddChild(Content);
	Cards.Add(Card);
	CardIds.Add(Id);
	return Card;
}

void UDHCharacterSelectScreen::NativeOnActivated()
{
	Super::NativeOnActivated();
	RefreshHighlight();
}

UWidget* UDHCharacterSelectScreen::NativeGetDesiredFocusTarget() const
{
	const int32 Index = CardIds.IndexOfByKey(UDHCharacterSettings::GetPreferredCharacter());
	return Cards.IsValidIndex(Index) ? Cards[Index].Get() : (Cards.IsEmpty() ? nullptr : Cards[0].Get());
}

void UDHCharacterSelectScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// UButton draws no focus state; show which card the gamepad/keyboard is on.
	FString Shown = CurrentId;
	for (int32 Index = 0; Index < Cards.Num(); ++Index)
	{
		const bool bFocused = Cards[Index]->IsActive();
		Cards[Index]->SetBackgroundColor(CardIds[Index] == CurrentId ? DHScreenStyle::Selected : bFocused ? DHScreenStyle::Focus : CardColor);
		Cards[Index]->Animate(InDeltaTime);
		if (bFocused)
		{
			Shown = CardIds[Index];
		}
	}
	ShowSplash(Shown);

	// Splash springs in from the left, overshoots a little; the clip fades in over the still once it plays.
	const float Step = FMath::Min(InDeltaTime, 1.f / 30.f);
	SplashVelocity += -SplashOffset * 220.f * Step - SplashVelocity * 15.f * Step;
	SplashOffset += SplashVelocity * Step;
	FWidgetTransform Transform;
	Transform.Translation = FVector2D(SplashOffset, 0.f);
	Transform.Angle = SplashOffset * 0.012f;
	Transform.Shear = FVector2D(-4.f, 0.f);
	Splash->SetRenderTransform(Transform);
	Splash->SetRenderOpacity(FMath::Clamp(1.f + SplashOffset / 450.f, 0.f, 1.f));
	VideoFade = FMath::FInterpConstantTo(VideoFade, VideoPlayer->IsPlaying() ? 1.f : 0.f, InDeltaTime, 4.f);
	SplashVideo->SetRenderOpacity(VideoFade);

	if (VoiceAllowedAt > 0.0 && FPlatformTime::Seconds() >= VoiceAllowedAt)
	{
		VoiceAllowedAt = 0.0;
		const FString Name = SplashId.IsEmpty() ? TEXT("Random") : SplashId;
		const FString Path = FString::Printf(TEXT("/Game/DH/UI/Voices/DH_Voice_%s.DH_Voice_%s"), *Name, *Name);
		if (USoundBase* Sound = LoadObject<USoundBase>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			Voice = UGameplayStatics::SpawnSound2D(this, Sound);
		}
	}
}

void UDHCharacterSelectScreen::NativeDestruct()
{
	VideoPlayer->Close();
	if (Voice)
	{
		Voice->Stop();
	}
	Super::NativeDestruct();
}

void UDHCharacterSelectScreen::ShowSplash(const FString& Id)
{
	if (Id == SplashId)
	{
		return;
	}
	SplashId = Id;
	UTexture2D* Texture = Picture(Id);
	SplashImage->SetBrushFromTexture(Texture);
	SplashImage->SetColorAndOpacity(Texture ? FLinearColor::White : FLinearColor(0.f, 0.f, 0.f, 0.f));
	SplashName->SetText(FText::FromString(Id.IsEmpty() ? TEXT("RANDOM") : Id.ToUpper()));
	SplashOffset = -420.f;
	SplashVelocity = 0.f;
	if (USoundBase* Whoosh = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/MetaSounds/sfx_MeleeWhoosh_nl_meta_Preset.sfx_MeleeWhoosh_nl_meta_Preset")))
	{
		UGameplayStatics::PlaySound2D(this, Whoosh, 0.35f, FMath::FRandRange(1.25f, 1.45f));
	}

	VideoFade = 0.f;
	VideoPlayer->Close();
	const FString Clip = ClipPath(Id);
	if (IFileManager::Get().FileExists(*Clip))
	{
		UFileMediaSource* Source = NewObject<UFileMediaSource>(this);
		Source->SetFilePath(Clip);
		KeepAlive.Add(Source);
		VideoPlayer->OpenSource(Source);
	}

	// The voice line once the selection rests for a moment, not for every card scrolled past.
	if (Voice)
	{
		Voice->FadeOut(0.15f, 0.f);
		Voice = nullptr;
	}
	VoiceAllowedAt = FPlatformTime::Seconds() + 0.3;
}

void UDHCharacterSelectScreen::HandleCardClicked(UDHCardButton* Card)
{
	Select(Card->CardId);
}

void UDHCharacterSelectScreen::HandleBackClicked()
{
	DeactivateWidget();
}

void UDHCharacterSelectScreen::Select(const FString& Id)
{
	UDHCharacterSettings::ChooseCharacter(GetOwningPlayer(), Id);
	RefreshHighlight();
}

void UDHCharacterSelectScreen::RefreshHighlight()
{
	const FString Current = UDHCharacterSettings::GetPreferredCharacter();
	CurrentId = Current;
	CurrentText->SetText(FText::Format(NSLOCTEXT("DowntownHunting", "Current", "Current: {0}"),
		FText::FromString(Current.IsEmpty() ? TEXT("Random") : Current)));
}
