#include "DHMainMenu.h"

#include "Blueprint/WidgetTree.h"
#include "CommonUIExtensions.h"
#include "Components/Border.h"
#include "Components/ProgressBar.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "DHCardButton.h"
#include "DHCharacterSelectScreen.h"
#include "DHControllerScreen.h"
#include "DHLoadoutScreen.h"
#include "DHMenuBackground.h"
#include "DHPlayScreen.h"
#include "DHIntroScreen.h"
#include "DHProgression.h"
#include "DHScreenStyle.h"
#include "Engine/Texture2D.h"
#include "Sound/SoundBase.h"
#include "GameplayTagContainer.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Messaging/CommonGameDialog.h"
#include "Messaging/CommonMessagingSubsystem.h"
#include "Misc/Paths.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DHMainMenu)

namespace
{
	const FLinearColor Clear(0.f, 0.f, 0.f, 0.f);

	/** Black, transparent on the left 40 %, getting dark towards the right edge where the entries are. */
	UTexture2D* MakeGradient()
	{
		constexpr int32 Width = 256;
		UTexture2D* Texture = UTexture2D::CreateTransient(Width, 1, PF_B8G8R8A8);
		FColor* Pixels = static_cast<FColor*>(Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE));
		for (int32 X = 0; X < Width; ++X)
		{
			const float T = FMath::SmoothStep(0.35f, 0.8f, X / float(Width - 1));
			Pixels[X] = FColor(4, 3, 2, uint8(T * 215.f));
		}
		Texture->GetPlatformData()->Mips[0].BulkData.Unlock();
		Texture->UpdateResource();
		return Texture;
	}
}

void UDHMainMenu::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
	WidgetTree->RootWidget = Root;
	auto Fill = [Root](UWidget* Widget)
	{
		UOverlaySlot* WidgetSlot = Root->AddChildToOverlay(Widget);
		WidgetSlot->SetHorizontalAlignment(HAlign_Fill);
		WidgetSlot->SetVerticalAlignment(VAlign_Fill);
	};

	UTexture2D* Gradient = MakeGradient();
	KeepAlive.Add(Gradient);
	UImage* Shade = WidgetTree->ConstructWidget<UImage>();
	Shade->SetBrushFromTexture(Gradient);
	Fill(Shade);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	UOverlaySlot* ColumnSlot = Root->AddChildToOverlay(Column);
	ColumnSlot->SetHorizontalAlignment(HAlign_Right);
	ColumnSlot->SetVerticalAlignment(VAlign_Center);
	ColumnSlot->SetPadding(FMargin(0, 0, 110, 0));

	if (UTexture2D* Logo = LoadObject<UTexture2D>(nullptr, TEXT("/Game/DH/UI/T_DH_Logo.T_DH_Logo")))
	{
		UImage* LogoImage = WidgetTree->ConstructWidget<UImage>();
		LogoImage->SetBrushFromTexture(Logo);
		USizeBox* LogoBox = WidgetTree->ConstructWidget<USizeBox>();
		const float Width = 480.f;
		LogoBox->SetWidthOverride(Width);
		LogoBox->SetHeightOverride(Width * Logo->GetSizeY() / FMath::Max(1, Logo->GetSizeX()));
		LogoBox->AddChild(LogoImage);
		UVerticalBoxSlot* LogoSlot = Column->AddChildToVerticalBox(LogoBox);
		LogoSlot->SetHorizontalAlignment(HAlign_Left);
		LogoSlot->SetPadding(FMargin(16, 0, 0, 40));
	}

	// Level badge, top left.
	UVerticalBox* Badge = WidgetTree->ConstructWidget<UVerticalBox>();
	UOverlaySlot* BadgeSlot = Root->AddChildToOverlay(Badge);
	BadgeSlot->SetHorizontalAlignment(HAlign_Left);
	BadgeSlot->SetVerticalAlignment(VAlign_Top);
	BadgeSlot->SetPadding(FMargin(60, 50, 0, 0));
	LevelText = DHScreenStyle::MakeText(WidgetTree, TEXT("Orbitron"), 28, DHScreenStyle::Accent);
	Badge->AddChildToVerticalBox(LevelText);
	USizeBox* BarBox = WidgetTree->ConstructWidget<USizeBox>();
	BarBox->SetWidthOverride(260.f);
	BarBox->SetHeightOverride(6.f);
	XPBar = WidgetTree->ConstructWidget<UProgressBar>();
	XPBar->SetFillColorAndOpacity(DHScreenStyle::Accent);
	BarBox->AddChild(XPBar);
	Badge->AddChildToVerticalBox(BarBox)->SetPadding(FMargin(0, 6, 0, 4));
	XPText = DHScreenStyle::MakeText(WidgetTree, TEXT("NotoSans"), 14, DHScreenStyle::Text);
	Badge->AddChildToVerticalBox(XPText);

	AddEntry(Column, TEXT("Play"), NSLOCTEXT("DowntownHunting", "MenuPlay", "PLAY"),
		NSLOCTEXT("DowntownHunting", "MenuPlaySub", "Host a match with bots, on LAN or online, or join one"));
	AddEntry(Column, TEXT("Character"), NSLOCTEXT("DowntownHunting", "MenuCharacter", "CHARACTER"),
		NSLOCTEXT("DowntownHunting", "MenuCharacterSub", "Pick who you hunt as"));
	AddEntry(Column, TEXT("Loadout"), NSLOCTEXT("DowntownHunting", "MenuLoadout", "LOADOUT"),
		NSLOCTEXT("DowntownHunting", "MenuLoadoutSub", "Weapons, abilities and perks, five loadouts"));
	AddEntry(Column, TEXT("Options"), NSLOCTEXT("DowntownHunting", "MenuOptions", "OPTIONS"),
		NSLOCTEXT("DowntownHunting", "MenuOptionsSub", "Video, audio, controls, gamepad"));
	AddEntry(Column, TEXT("Controller"), NSLOCTEXT("DowntownHunting", "MenuController", "CONTROLLER"),
		NSLOCTEXT("DowntownHunting", "MenuControllerSub", "Gamepad button layout"));
	AddEntry(Column, TEXT("Credits"), NSLOCTEXT("DowntownHunting", "MenuCredits", "CREDITS"),
		NSLOCTEXT("DowntownHunting", "MenuCreditsSub", "Who made this city"));
	AddEntry(Column, TEXT("Quit"), NSLOCTEXT("DowntownHunting", "MenuQuit", "QUIT"),
		NSLOCTEXT("DowntownHunting", "MenuQuitSub", "Back to the desktop"));
}

void UDHMainMenu::AddEntry(UVerticalBox* Column, const FString& Id, const FText& Title, const FText& Subtitle)
{
	FEntry Entry;
	Entry.Id = Id;
	Entry.Button = WidgetTree->ConstructWidget<UDHCardButton>();
	Entry.Button->SetBackgroundColor(Clear);
	Entry.Button->CardId = Id;
	Entry.Button->OnCardClicked.BindUObject(this, &ThisClass::HandleEntryClicked);

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	Entry.Bar = WidgetTree->ConstructWidget<UBorder>();
	Entry.Bar->SetBrushColor(DHScreenStyle::Accent);
	Entry.Bar->SetPadding(FMargin(3, 0));
	Entry.Bar->SetRenderOpacity(0.f);
	UHorizontalBoxSlot* BarSlot = Row->AddChildToHorizontalBox(Entry.Bar);
	BarSlot->SetVerticalAlignment(VAlign_Fill);
	BarSlot->SetPadding(FMargin(0, 6, 18, 6));

	UVerticalBox* Texts = WidgetTree->ConstructWidget<UVerticalBox>();
	UTextBlock* TitleText = WidgetTree->ConstructWidget<UTextBlock>();
	TitleText->SetText(Title);
	TitleText->SetFont(DHScreenStyle::Font(TEXT("Orbitron"), 34));
	TitleText->SetShadowOffset(FVector2D(2, 2));
	TitleText->SetShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.6f));
	Texts->AddChildToVerticalBox(TitleText);
	Entry.SubtitleText = WidgetTree->ConstructWidget<UTextBlock>();
	Entry.SubtitleText->SetText(Subtitle);
	Entry.SubtitleText->SetFont(DHScreenStyle::Font(TEXT("NotoSans"), 15));
	Entry.SubtitleText->SetColorAndOpacity(DHScreenStyle::Text);
	Entry.SubtitleText->SetRenderOpacity(0.f);
	Texts->AddChildToVerticalBox(Entry.SubtitleText);
	Row->AddChildToHorizontalBox(Texts);
	Entry.Button->AddChild(Row);

	UVerticalBoxSlot* EntrySlot = Column->AddChildToVerticalBox(Entry.Button);
	EntrySlot->SetHorizontalAlignment(HAlign_Left);
	EntrySlot->SetPadding(FMargin(0, 6));
	KeepAlive.Add(Entry.Button);
	Entries.Add(Entry);
}

void UDHMainMenu::RefreshLevel()
{
	const int32 XP = FDHProgression::GetXP();
	const int32 Level = FDHProgression::LevelForXP(XP);
	const int32 From = FDHProgression::LevelStartXP(Level), To = FDHProgression::LevelStartXP(Level + 1);
	LevelText->SetText(FText::Format(NSLOCTEXT("DowntownHunting", "MenuLevel", "LEVEL {0}"), Level));
	XPText->SetText(FText::Format(NSLOCTEXT("DowntownHunting", "MenuXP", "{0} / {1} XP"), XP, To));
	XPBar->SetPercent(float(XP - From) / FMath::Max(1, To - From));
}

void UDHMainMenu::NativeOnActivated()
{
	Super::NativeOnActivated();
	UDHControllerScreen::ApplyBindings(GetOwningLocalPlayer());
	UDHIntroScreen::PlayOnce(GetOwningPlayer());
	RefreshLevel();
	if (!Entries.IsEmpty())
	{
		ShowBackground(Entries[0].Id);
	}
}

UWidget* UDHMainMenu::NativeGetDesiredFocusTarget() const
{
	return Entries.IsEmpty() ? nullptr : Entries[0].Button.Get();
}

void UDHMainMenu::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Focus wins over hover, so the gamepad and the mouse never highlight two entries.
	int32 Focused = Entries.IndexOfByPredicate([](const FEntry& Entry) { return Entry.Button->HasAnyUserFocus(); });
	if (Focused == INDEX_NONE)
	{
		Focused = Entries.IndexOfByPredicate([](const FEntry& Entry) { return Entry.Button->IsHovered(); });
	}
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		FEntry& Entry = Entries[Index];
		Entry.Focus = FMath::FInterpTo(Entry.Focus, Index == Focused ? 1.f : 0.f, InDeltaTime, 12.f);
		Entry.Bar->SetRenderOpacity(Entry.Focus);
		Entry.SubtitleText->SetRenderOpacity(Entry.Focus * 0.85f);
		Entry.Button->SetRenderTranslation(FVector2D(-28.f * Entry.Focus, 0.f));
		Entry.Button->SetRenderOpacity(0.55f + 0.45f * Entry.Focus);
	}
	if (Entries.IsValidIndex(Focused) && Focused != LastFocused)
	{
		if (LastFocused != INDEX_NONE)
		{
			PlayUISound(TEXT("/Game/Audio/Sounds/UI/sfx_UI_MainMenu_Highlight_nl_meta_Preset.sfx_UI_MainMenu_Highlight_nl_meta_Preset"));
		}
		LastFocused = Focused;
		ShowBackground(Entries[Focused].Id);
	}
}

void UDHMainMenu::PlayUISound(const TCHAR* Path)
{
	if (USoundBase* Sound = LoadObject<USoundBase>(nullptr, Path))
	{
		UGameplayStatics::PlaySound2D(this, Sound);
	}
}

void UDHMainMenu::ShowBackground(const FString& Id)
{
	if (!Background.IsValid())
	{
		// Below every menu screen, so the city stays behind Play, Character, Options...
		Background = Cast<UDHMenuBackground>(UCommonUIExtensions::PushContentToLayer_ForPlayer(
			GetOwningLocalPlayer(), FGameplayTag::RequestGameplayTag(TEXT("UI.Layer.Game")), UDHMenuBackground::StaticClass()));
	}
	if (Background.IsValid())
	{
		Background->Show(Id);
	}
}

void UDHMainMenu::HandleEntryClicked(UDHCardButton* Button)
{
	PlayUISound(TEXT("/Game/Audio/Sounds/UI/sfx_UI_MainMenu_Select_nl_meta_Preset.sfx_UI_MainMenu_Select_nl_meta_Preset"));
	Open(Button->CardId);
}

void UDHMainMenu::Open(const FString& Id)
{
	ULocalPlayer* Player = GetOwningLocalPlayer();
	const FGameplayTag MenuLayer = FGameplayTag::RequestGameplayTag(TEXT("UI.Layer.Menu"));
	auto Push = [Player, &MenuLayer](UClass* Class)
	{
		if (Class)
		{
			UCommonUIExtensions::PushContentToLayer_ForPlayer(Player, MenuLayer, Class);
		}
	};
	if (Id == TEXT("Play"))
	{
		Push(UDHPlayScreen::StaticClass());
	}
	else if (Id == TEXT("Character"))
	{
		Push(UDHCharacterSelectScreen::StaticClass());
	}
	else if (Id == TEXT("Loadout"))
	{
		Push(UDHLoadoutScreen::StaticClass());
	}
	else if (Id == TEXT("Options"))
	{
		Push(LoadClass<UCommonActivatableWidget>(nullptr, TEXT("/Game/UI/Settings/W_LyraSettingScreen.W_LyraSettingScreen_C")));
	}
	else if (Id == TEXT("Controller"))
	{
		Push(UDHControllerScreen::StaticClass());
	}
	else if (Id == TEXT("Credits"))
	{
		Push(LoadClass<UCommonActivatableWidget>(nullptr, TEXT("/Game/UI/Credits/W_Credits.W_Credits_C")));
	}
	else if (Id == TEXT("Quit"))
	{
		if (UCommonMessagingSubsystem* Messaging = Player ? Player->GetSubsystem<UCommonMessagingSubsystem>() : nullptr)
		{
			Messaging->ShowConfirmation(
				UCommonGameDialogDescriptor::CreateConfirmationYesNo(NSLOCTEXT("DowntownHunting", "QuitHeader", "QUIT"),
					NSLOCTEXT("DowntownHunting", "QuitBody", "Leave Downtown Hunting?")),
				FCommonMessagingResultDelegate::CreateWeakLambda(this, [this](ECommonMessagingResult Result)
				{
					if (Result == ECommonMessagingResult::Confirmed)
					{
						UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
					}
				}));
		}
	}
}
