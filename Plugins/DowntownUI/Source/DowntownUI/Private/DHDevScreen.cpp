#include "DHDevScreen.h"

#include "Blueprint/WidgetTree.h"
#include "CommonUIExtensions.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "DHCardButton.h"
#include "DHProgression.h"
#include "DHScreenStyle.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DHDevScreen)

bool UDHDevScreen::bSlowMotion = false;
bool UDHDevScreen::bShowFps = false;

namespace
{
	TWeakObjectPtr<UDHDevScreen> ActiveDevScreen;

	struct FEntryInfo
	{
		const TCHAR* Id;
		FText Name;
		bool bToggle;
	};

	TArray<FEntryInfo> EntryInfos()
	{
		return {
			{ TEXT("NoCooldowns"), NSLOCTEXT("DowntownHunting", "DevNoCooldowns", "NO COOLDOWNS"), true },
			{ TEXT("God"), NSLOCTEXT("DowntownHunting", "DevGod", "GOD MODE"), true },
			{ TEXT("InfiniteAmmo"), NSLOCTEXT("DowntownHunting", "DevAmmo", "ENDLESS AMMO"), true },
			{ TEXT("SlowMotion"), NSLOCTEXT("DowntownHunting", "DevSlomo", "SLOW MOTION"), true },
			{ TEXT("Fps"), NSLOCTEXT("DowntownHunting", "DevFps", "SHOW FPS"), true },
			{ TEXT("LevelUp"), NSLOCTEXT("DowntownHunting", "DevLevelUp", "LEVEL UP"), false },
			{ TEXT("Streak"), NSLOCTEXT("DowntownHunting", "DevStreak", "KILLSTREAK +3"), false },
			{ TEXT("AddBot"), NSLOCTEXT("DowntownHunting", "DevAddBot", "ADD BOT"), false },
			{ TEXT("RemoveBot"), NSLOCTEXT("DowntownHunting", "DevRemoveBot", "REMOVE BOT"), false },
			{ TEXT("Heal"), NSLOCTEXT("DowntownHunting", "DevHeal", "HEAL"), false },
			{ TEXT("Respawn"), NSLOCTEXT("DowntownHunting", "DevRespawn", "KILL ME (RESPAWN)"), false },
		};
	}

	ULocalPlayer* FirstPlayer()
	{
		const UGameInstance* GameInstance = GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetFirstGamePlayer() : nullptr;
	}
}

/** F10, or Y while the gamepad's View button is held: opens or closes the dev screen; the key does nothing else. */
class FDHDevHotkey : public IInputProcessor
{
public:
	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		const FKey Key = InKeyEvent.GetKey();
		if (Key == EKeys::Gamepad_Special_Left)
		{
			bViewHeld = true;
		}
		if (InKeyEvent.IsRepeat() || !(Key == EKeys::F10 || (Key == EKeys::Gamepad_FaceButton_Top && bViewHeld)))
		{
			return false;
		}
		UDHDevScreen::Open(FirstPlayer());
		return true;
	}

	virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		if (InKeyEvent.GetKey() == EKeys::Gamepad_Special_Left)
		{
			bViewHeld = false;
		}
		return false;
	}

	virtual const TCHAR* GetDebugName() const override { return TEXT("DHDevHotkey"); }

private:
	bool bViewHeld = false;
};

UDHDevScreen::UDHDevScreen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsBackHandler = true;
	bIsModal = true;
}

void UDHDevScreen::InstallHotkey()
{
	static bool bInstalled = false;
	if (!bInstalled && FDHProgression::IsDevMode() && FSlateApplication::IsInitialized())
	{
		bInstalled = true;
		FSlateApplication::Get().RegisterInputPreProcessor(MakeShared<FDHDevHotkey>());
	}
}

void UDHDevScreen::Open(ULocalPlayer* Player)
{
	if (ActiveDevScreen.IsValid() && ActiveDevScreen->IsActivated())
	{
		ActiveDevScreen->DeactivateWidget();
		return;
	}
	if (Player && FDHProgression::IsDevMode())
	{
		ActiveDevScreen = Cast<UDHDevScreen>(UCommonUIExtensions::PushContentToLayer_ForPlayer(Player,
			FGameplayTag::RequestGameplayTag(TEXT("UI.Layer.Menu")), StaticClass()));
	}
}

void UDHDevScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
	WidgetTree->RootWidget = Root;
	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>();
	Dim->SetBrushColor(DHScreenStyle::Dim);
	UOverlaySlot* DimSlot = Root->AddChildToOverlay(Dim);
	DimSlot->SetHorizontalAlignment(HAlign_Fill);
	DimSlot->SetVerticalAlignment(VAlign_Fill);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	UOverlaySlot* ColumnSlot = Root->AddChildToOverlay(Column);
	ColumnSlot->SetHorizontalAlignment(HAlign_Center);
	ColumnSlot->SetVerticalAlignment(VAlign_Center);

	UTextBlock* Title = DHScreenStyle::MakeText(WidgetTree, TEXT("Orbitron"), 34, DHScreenStyle::Accent);
	Title->SetText(NSLOCTEXT("DowntownHunting", "DevTitle", "DEV TOOLS"));
	Column->AddChildToVerticalBox(Title);
	UTextBlock* Hint = DHScreenStyle::MakeText(WidgetTree, TEXT("NotoSans"), 14, DHScreenStyle::Text);
	Hint->SetText(NSLOCTEXT("DowntownHunting", "DevHint",
		"Open anywhere: F10, or hold VIEW and press Y.  Switches act in matches you host (and offline).\n"
		"Console: the ^ key (German keyboard) or `. Try: God, slomo 0.3, AddPlayerBot, DH.Streak 5, DH.Teleport X Y Z, stat unit."));
	Column->AddChildToVerticalBox(Hint)->SetPadding(FMargin(0, 4, 0, 16));

	// Two columns: switches, actions.
	UHorizontalBox* ListRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	Column->AddChildToVerticalBox(ListRow);
	UVerticalBox* Lists[2] = { WidgetTree->ConstructWidget<UVerticalBox>(), WidgetTree->ConstructWidget<UVerticalBox>() };
	for (UVerticalBox* List : Lists)
	{
		ListRow->AddChildToHorizontalBox(List)->SetPadding(FMargin(0, 0, 24, 0));
	}
	for (const FEntryInfo& Info : EntryInfos())
	{
		FEntry Entry{ Info.Id, Info.Name, Info.bToggle };
		Entry.Button = WidgetTree->ConstructWidget<UDHCardButton>();
		Entry.Button->SetBackgroundColor(DHScreenStyle::Card);
		Entry.Button->CardId = Info.Id;
		Entry.Button->PopScale = 0.03f;
		Entry.Button->OnCardClicked.BindUObject(this, &ThisClass::HandleCardClicked);
		UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>();
		USizeBox* NameBox = WidgetTree->ConstructWidget<USizeBox>();
		NameBox->SetWidthOverride(260.f);
		UTextBlock* Name = DHScreenStyle::MakeText(WidgetTree, TEXT("Orbitron"), 16, DHScreenStyle::Text);
		Name->SetText(Info.Name);
		NameBox->AddChild(Name);
		Line->AddChildToHorizontalBox(NameBox)->SetVerticalAlignment(VAlign_Center);
		USizeBox* StateBox = WidgetTree->ConstructWidget<USizeBox>();
		StateBox->SetWidthOverride(70.f);
		Entry.State = DHScreenStyle::MakeText(WidgetTree, TEXT("Orbitron"), 16, DHScreenStyle::Accent);
		StateBox->AddChild(Entry.State);
		Line->AddChildToHorizontalBox(StateBox)->SetVerticalAlignment(VAlign_Center);
		Entry.Button->AddChild(Line);
		Lists[Info.bToggle ? 0 : 1]->AddChildToVerticalBox(Entry.Button)->SetPadding(FMargin(0, 3));
		KeepAlive.Add(Entry.Button);
		Entries.Add(Entry);
	}

	FEntry Back{ TEXT("Back"), NSLOCTEXT("DowntownHunting", "Back", "BACK"), false };
	Back.Button = WidgetTree->ConstructWidget<UDHCardButton>();
	Back.Button->SetBackgroundColor(DHScreenStyle::Card);
	Back.Button->CardId = Back.Id;
	Back.Button->OnCardClicked.BindUObject(this, &ThisClass::HandleCardClicked);
	UTextBlock* BackText = DHScreenStyle::MakeText(WidgetTree, TEXT("Orbitron"), 18, DHScreenStyle::Text);
	BackText->SetText(Back.Name);
	Back.Button->AddChild(BackText);
	UVerticalBoxSlot* BackSlot = Column->AddChildToVerticalBox(Back.Button);
	BackSlot->SetHorizontalAlignment(HAlign_Left);
	BackSlot->SetPadding(FMargin(0, 16, 0, 0));
	KeepAlive.Add(Back.Button);
	Entries.Add(Back);
	RefreshStates();
}

UWidget* UDHDevScreen::NativeGetDesiredFocusTarget() const
{
	return Entries.IsEmpty() ? nullptr : Entries[0].Button.Get();
}

void UDHDevScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	for (const FEntry& Entry : Entries)
	{
		const bool bActive = Entry.Button->IsActive();
		Entry.Button->SetBackgroundColor(Entry.bToggle && IsOn(Entry.Id) ? DHScreenStyle::Selected : bActive ? DHScreenStyle::Focus : DHScreenStyle::Card);
		Entry.Button->Animate(InDeltaTime);
	}
}

bool UDHDevScreen::IsOn(const FString& Id) const
{
	if (Id == TEXT("SlowMotion"))
	{
		return bSlowMotion;
	}
	if (Id == TEXT("Fps"))
	{
		return bShowFps;
	}
	return FDHProgression::DevFlag(*Id);
}

void UDHDevScreen::RefreshStates()
{
	for (const FEntry& Entry : Entries)
	{
		if (Entry.State)
		{
			Entry.State->SetText(!Entry.bToggle ? FText::FromString(TEXT(">"))
				: IsOn(Entry.Id) ? NSLOCTEXT("DowntownHunting", "DevOn", "ON") : NSLOCTEXT("DowntownHunting", "DevOff", "OFF"));
		}
	}
}

void UDHDevScreen::HandleCardClicked(UDHCardButton* Card)
{
	const FString& Id = Card->CardId;
	APlayerController* PC = GetOwningPlayer();
	auto Console = [PC](const TCHAR* Command)
	{
		if (PC)
		{
			PC->ConsoleCommand(Command);
		}
	};
	if (Id == TEXT("Back"))
	{
		DeactivateWidget();
		return;
	}
	if (Id == TEXT("SlowMotion"))
	{
		bSlowMotion = !bSlowMotion;
		Console(bSlowMotion ? TEXT("slomo 0.35") : TEXT("slomo 1"));
	}
	else if (Id == TEXT("Fps"))
	{
		bShowFps = !bShowFps;
		Console(TEXT("stat fps"));
	}
	else if (Id == TEXT("LevelUp"))
	{
		FDHProgression::AddXP(FDHProgression::LevelStartXP(FDHProgression::GetLevel() + 1) - FDHProgression::GetXP());
	}
	else if (Id == TEXT("Streak"))
	{
		Console(TEXT("DH.Streak 3"));
	}
	else if (Id == TEXT("AddBot"))
	{
		Console(TEXT("AddPlayerBot"));
	}
	else if (Id == TEXT("RemoveBot"))
	{
		Console(TEXT("RemovePlayerBot"));
	}
	else if (Id == TEXT("Heal"))
	{
		Console(TEXT("HealSelf 100"));
	}
	else if (Id == TEXT("Respawn"))
	{
		Console(TEXT("DamageSelfDestruct"));
		DeactivateWidget();
	}
	else
	{
		FDHProgression::SetDevFlag(*Id, !FDHProgression::DevFlag(*Id));
	}
	RefreshStates();
}
