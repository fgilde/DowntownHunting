#include "DHPlayScreen.h"

#include "Blueprint/WidgetTree.h"
#include "CommonSessionSubsystem.h"
#include "CommonUIExtensions.h"
#include "CommonUserSubsystem.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "DHCardButton.h"
#include "DHJoinScreen.h"
#include "DHScreenStyle.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "GameplayTagContainer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DHPlayScreen)

namespace
{
	FText TextOf(const UObject* Object, FName Name)
	{
		const FTextProperty* Property = FindFProperty<FTextProperty>(Object->GetClass(), Name);
		return Property ? Property->GetPropertyValue_InContainer(Object) : FText::GetEmpty();
	}

	// Position in Modes.json (ExtraArgs "MenuOrder", written by sync_modes.py); modes without it go last.
	int32 MenuOrder(const UObject* Object)
	{
		const FMapProperty* Property = FindFProperty<FMapProperty>(Object->GetClass(), TEXT("ExtraArgs"));
		const TMap<FString, FString>* Args = Property ? Property->ContainerPtrToValuePtr<TMap<FString, FString>>(Object) : nullptr;
		const FString* Order = Args ? Args->Find(TEXT("MenuOrder")) : nullptr;
		return Order ? FCString::Atoi(**Order) : MAX_int32;
	}

}

UDHPlayScreen::UDHPlayScreen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsBackHandler = true;
}

void UDHPlayScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
	WidgetTree->RootWidget = Root;
	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>();
	Dim->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.6f));
	UOverlaySlot* DimSlot = Root->AddChildToOverlay(Dim);
	DimSlot->SetHorizontalAlignment(HAlign_Fill);
	DimSlot->SetVerticalAlignment(VAlign_Fill);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	UOverlaySlot* ColumnSlot = Root->AddChildToOverlay(Column);
	ColumnSlot->SetHorizontalAlignment(HAlign_Center);
	ColumnSlot->SetVerticalAlignment(VAlign_Fill);
	ColumnSlot->SetPadding(FMargin(60, 50, 60, 40));

	// Title row with the switches on the right.
	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>();
	Column->AddChildToVerticalBox(Header)->SetPadding(FMargin(8, 0, 8, 18));
	UTextBlock* Title = DHScreenStyle::MakeText(WidgetTree, TEXT("Orbitron"), 40, DHScreenStyle::Accent);
	Title->SetText(NSLOCTEXT("DowntownHunting", "PlayTitle", "PLAY"));
	UHorizontalBoxSlot* TitleSlot = Header->AddChildToHorizontalBox(Title);
	TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	TitleSlot->SetVerticalAlignment(VAlign_Center);

	auto AddSwitch = [this, Header](const FString& Id) -> UTextBlock*
	{
		UDHCardButton* Button = WidgetTree->ConstructWidget<UDHCardButton>();
		Button->SetBackgroundColor(DHScreenStyle::Card);
		Button->CardId = Id;
		Button->OnCardClicked.BindUObject(this, &ThisClass::HandleCardClicked);
		UTextBlock* Label = DHScreenStyle::MakeText(WidgetTree, TEXT("Orbitron"), 15, DHScreenStyle::Text);
		Button->AddChild(Label);
		UHorizontalBoxSlot* Slot = Header->AddChildToHorizontalBox(Button);
		Slot->SetVerticalAlignment(VAlign_Center);
		Slot->SetPadding(FMargin(10, 0, 0, 0));
		Switches.Add(Button);
		KeepAlive.Add(Button);
		return Label;
	};
	NetworkText = AddSwitch(TEXT("Network"));
	BotsText = AddSwitch(TEXT("Bots"));
	AddSwitch(TEXT("Join"))->SetText(NSLOCTEXT("DowntownHunting", "PlayJoin", "JOIN A GAME"));
	AddSwitch(TEXT("Back"))->SetText(NSLOCTEXT("DowntownHunting", "Back", "BACK"));
	UpdateSwitches();

	// The modes, in Modes.json order.
	TArray<FPrimaryAssetId> Ids;
	UAssetManager::Get().GetPrimaryAssetIdList(FPrimaryAssetType(TEXT("LyraUserFacingExperienceDefinition")), Ids);
	for (const FPrimaryAssetId& Id : Ids)
	{
		UObject* Definition = UAssetManager::Get().GetPrimaryAssetPath(Id).TryLoad();
		const FBoolProperty* Show = Definition ? FindFProperty<FBoolProperty>(Definition->GetClass(), TEXT("bShowInFrontEnd")) : nullptr;
		if (Show && Show->GetPropertyValue_InContainer(Definition))
		{
			Modes.Add({ Definition, nullptr, TextOf(Definition, TEXT("TileTitle")), TextOf(Definition, TEXT("TileSubTitle")), TextOf(Definition, TEXT("TileDescription")) });
		}
	}
	Modes.Sort([](const FMode& A, const FMode& B) { return MenuOrder(A.Definition) < MenuOrder(B.Definition); });

	auto Row = [this, Column](const FText& Label) -> UHorizontalBox*
	{
		UTextBlock* Heading = DHScreenStyle::MakeText(WidgetTree, TEXT("Orbitron"), 20, DHScreenStyle::Text);
		Heading->SetText(Label);
		Column->AddChildToVerticalBox(Heading)->SetPadding(FMargin(8, 10, 8, 4));
		UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
		Scroll->SetOrientation(Orient_Horizontal);
		Scroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
		Column->AddChildToVerticalBox(Scroll);
		UHorizontalBox* Cards = WidgetTree->ConstructWidget<UHorizontalBox>();
		Scroll->AddChild(Cards);
		return Cards;
	};

	// Modes, each once, in the order they first appear.
	UHorizontalBox* KindRow = Row(NSLOCTEXT("DowntownHunting", "PlayModes", "MODE"));
	for (int32 Index = 0; Index < Modes.Num(); ++Index)
	{
		KeepAlive.Add(Modes[Index].Definition);
		const FString Kind = Modes[Index].Title.ToString();
		if (!Kinds.Contains(Kind))
		{
			UDHCardButton* Card = MakeCard(Modes[Index], FText::FromString(Kind.ToUpper()), 240.f, TEXT("Kind"), Kinds.Num());
			KindRow->AddChildToHorizontalBox(Card)->SetPadding(FMargin(8));
			Kinds.Add(Kind);
			KindButtons.Add(Card);
		}
	}
	MapRow = Row(NSLOCTEXT("DowntownHunting", "PlayMaps", "MAP"));
	SelectMode(0);

	// Gamepad: from the modes up to the switches, from the switches down into the modes.
	for (int32 Index = 0; Index < KindButtons.Num(); ++Index)
	{
		KindButtons[Index]->SetNavigationRuleExplicit(EUINavigation::Up, Switches[FMath::Min(Index, Switches.Num() - 1)]);
	}
	for (UDHCardButton* Switch : Switches)
	{
		if (!KindButtons.IsEmpty())
		{
			Switch->SetNavigationRuleExplicit(EUINavigation::Down, KindButtons[0]);
		}
	}

	DescriptionText = DHScreenStyle::MakeText(WidgetTree, TEXT("NotoSans"), 16, DHScreenStyle::Text);
	DescriptionText->SetAutoWrapText(true);
	UVerticalBoxSlot* DescriptionSlot = Column->AddChildToVerticalBox(DescriptionText);
	DescriptionSlot->SetPadding(FMargin(8, 16, 8, 0));
}

UDHCardButton* UDHPlayScreen::MakeCard(const FMode& Entry, const FText& Caption, float Width, const FString& Id, int32 CardIndex)
{
	UDHCardButton* Card = WidgetTree->ConstructWidget<UDHCardButton>();
	Card->SetBackgroundColor(DHScreenStyle::Card);
	Card->CardSlot = CardIndex;
	Card->CardId = Id;
	Card->OnCardClicked.BindUObject(this, &ThisClass::HandleCardClicked);

	UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
	USizeBox* ImageBox = WidgetTree->ConstructWidget<USizeBox>();
	ImageBox->SetWidthOverride(Width);
	ImageBox->SetHeightOverride(Width / 2.f);
	UImage* Tile = WidgetTree->ConstructWidget<UImage>();
	const FObjectProperty* Icon = FindFProperty<FObjectProperty>(Entry.Definition->GetClass(), TEXT("TileIcon"));
	if (UTexture2D* Texture = Icon ? Cast<UTexture2D>(Icon->GetObjectPropertyValue_InContainer(Entry.Definition)) : nullptr)
	{
		Tile->SetBrushFromTexture(Texture);
	}
	ImageBox->AddChild(Tile);
	Content->AddChildToVerticalBox(ImageBox);
	UTextBlock* Label = DHScreenStyle::MakeText(WidgetTree, TEXT("Orbitron"), 15, DHScreenStyle::Text);
	Label->SetText(Caption);
	UScaleBox* Fit = WidgetTree->ConstructWidget<UScaleBox>();  // long names shrink instead of being cut
	Fit->SetStretch(EStretch::ScaleToFit);
	Fit->SetStretchDirection(EStretchDirection::DownOnly);
	Fit->AddChild(Label);
	UVerticalBoxSlot* LabelSlot = Content->AddChildToVerticalBox(Fit);
	LabelSlot->SetPadding(FMargin(4, 6, 4, 6));
	LabelSlot->SetHorizontalAlignment(HAlign_Left);
	USizeBox* CardBox = WidgetTree->ConstructWidget<USizeBox>();  // same width for every card of a row
	CardBox->SetWidthOverride(Width);
	CardBox->AddChild(Content);
	Card->AddChild(CardBox);
	KeepAlive.Add(Card);
	return Card;
}

void UDHPlayScreen::SelectMode(int32 Kind)
{
	if (!Kinds.IsValidIndex(Kind))
	{
		return;
	}
	SelectedKind = Kind;
	MapRow->ClearChildren();
	MapButtons.Reset();
	for (int32 Index = 0; Index < Modes.Num(); ++Index)
	{
		if (Modes[Index].Title.ToString() == Kinds[Kind])
		{
			UDHCardButton* Card = MakeCard(Modes[Index], FText::FromString(Modes[Index].Subtitle.ToString().ToUpper()), 320.f, TEXT("Map"), Index);
			Card->SetNavigationRuleExplicit(EUINavigation::Up, KindButtons[Kind]);
			MapRow->AddChildToHorizontalBox(Card)->SetPadding(FMargin(8));
			MapButtons.Add(Card);
		}
	}
}

UWidget* UDHPlayScreen::NativeGetDesiredFocusTarget() const
{
	return KindButtons.IsEmpty() ? nullptr : KindButtons[0].Get();
}

void UDHPlayScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	auto Highlight = [this, InDeltaTime](UDHCardButton* Card, bool bSelected, const FText& Description)
	{
		const bool bFocused = Card->HasAnyUserFocus() || Card->IsHovered();
		Card->SetBackgroundColor(bFocused ? DHScreenStyle::Focus : bSelected ? DHScreenStyle::Selected : DHScreenStyle::Card);
		Card->Animate(InDeltaTime);
		if (Card->HasAnyUserFocus())
		{
			DescriptionText->SetText(Description);
			if (UScrollBox* Scroll = Cast<UScrollBox>(Card->GetParent() ? Card->GetParent()->GetParent() : nullptr))
			{
				Scroll->ScrollWidgetIntoView(Card, true, EDescendantScrollDestination::IntoView);
			}
		}
	};
	for (int32 Index = 0; Index < KindButtons.Num(); ++Index)
	{
		// A mode's description: the first map's (they describe mode and map together).
		const FMode* First = Modes.FindByPredicate([this, Index](const FMode& Mode) { return Mode.Title.ToString() == Kinds[Index]; });
		Highlight(KindButtons[Index], Index == SelectedKind, First ? First->Description : FText::GetEmpty());
	}
	for (UDHCardButton* Card : MapButtons)
	{
		Highlight(Card, false, Modes.IsValidIndex(Card->CardSlot) ? Modes[Card->CardSlot].Description : FText::GetEmpty());
	}
	for (UDHCardButton* Switch : Switches)
	{
		const bool bFocused = Switch->HasAnyUserFocus() || Switch->IsHovered();
		Switch->SetBackgroundColor(bFocused ? DHScreenStyle::Focus : DHScreenStyle::Card);
		Switch->Animate(InDeltaTime);
	}
}

void UDHPlayScreen::UpdateSwitches()
{
	static const FText NetworkNames[] = {
		NSLOCTEXT("DowntownHunting", "NetOffline", "OFFLINE"), NSLOCTEXT("DowntownHunting", "NetLan", "LAN"), NSLOCTEXT("DowntownHunting", "NetOnline", "ONLINE") };
	NetworkText->SetText(FText::Format(NSLOCTEXT("DowntownHunting", "PlayNetwork", "NETWORK: {0}"), NetworkNames[Network]));
	static const FText BotNames[] = {
		NSLOCTEXT("DowntownHunting", "PlayBotsOff", "BOTS: OFF"), NSLOCTEXT("DowntownHunting", "PlayBotsEasy", "BOTS: EASY"),
		NSLOCTEXT("DowntownHunting", "PlayBotsNormal", "BOTS: NORMAL"), NSLOCTEXT("DowntownHunting", "PlayBotsHard", "BOTS: HARD"),
		NSLOCTEXT("DowntownHunting", "PlayBotsMixed", "BOTS: MIXED") };
	BotsText->SetText(BotNames[Bots]);
}

void UDHPlayScreen::HandleCardClicked(UDHCardButton* Card)
{
	if (Card->CardId == TEXT("Network"))
	{
		Network = (Network + 1) % 3;
		UpdateSwitches();
	}
	else if (Card->CardId == TEXT("Bots"))
	{
		Bots = (Bots + 1) % 5;
		UpdateSwitches();
	}
	else if (Card->CardId == TEXT("Back"))
	{
		DeactivateWidget();
	}
	else if (Card->CardId == TEXT("Join"))
	{
		UCommonUIExtensions::PushContentToLayer_ForPlayer(GetOwningLocalPlayer(), FGameplayTag::RequestGameplayTag(TEXT("UI.Layer.Menu")), UDHJoinScreen::StaticClass());
	}
	else if (Card->CardId == TEXT("Kind"))
	{
		SelectMode(Card->CardSlot);
		if (!MapButtons.IsEmpty())
		{
			MapButtons[0]->SetFocus();
		}
	}
	else if (Modes.IsValidIndex(Card->CardSlot))
	{
		UObject* Definition = Modes[Card->CardSlot].Definition;
		if (Network == int32(ECommonSessionOnlineMode::Online))
		{
			// Online hosting needs the online login first (Steam etc.); hosts when it completes.
			UCommonUserSubsystem* Users = GetGameInstance()->GetSubsystem<UCommonUserSubsystem>();
			PendingOnline = Definition;
			Users->OnUserInitializeComplete.AddUniqueDynamic(this, &ThisClass::HandleUserInitialized);
			Users->TryToLoginForOnlinePlay(0);
		}
		else
		{
			Host(Definition);
		}
	}
}

void UDHPlayScreen::HandleUserInitialized(const UCommonUserInfo* UserInfo, bool bSuccess, FText Error, ECommonUserPrivilege RequestedPrivilege, ECommonUserOnlineContext OnlineContext)
{
	GetGameInstance()->GetSubsystem<UCommonUserSubsystem>()->OnUserInitializeComplete.RemoveDynamic(this, &ThisClass::HandleUserInitialized);
	if (bSuccess && PendingOnline)
	{
		Host(PendingOnline);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("DH play: online login failed: %s"), *Error.ToString());
		DescriptionText->SetText(FText::Format(NSLOCTEXT("DowntownHunting", "PlayLoginFailed", "Online login failed: {0}"), Error));
	}
	PendingOnline = nullptr;
}

void UDHPlayScreen::Host(UObject* Definition)
{
	// ULyraUserFacingExperienceDefinition::CreateHostingRequest (LyraGame is not linked from here).
	UFunction* Create = Definition ? Definition->FindFunction(TEXT("CreateHostingRequest")) : nullptr;
	if (!Create)
	{
		return;
	}
	struct
	{
		const UObject* WorldContextObject;
		UCommonSession_HostSessionRequest* ReturnValue;
	} Params{ this, nullptr };
	Definition->ProcessEvent(Create, &Params);
	UCommonSession_HostSessionRequest* Request = Params.ReturnValue;
	if (!Request)
	{
		return;
	}
	Request->OnlineMode = ECommonSessionOnlineMode(Network);
	if (Bots == 0)
	{
		Request->ExtraArgs.Add(TEXT("NumBots"), TEXT("0"));
	}
	else
	{
		Request->ExtraArgs.Add(TEXT("BotSkill"), FString::FromInt(Bots - 1));  // UDHGameRulesComponent
	}
	GetGameInstance()->GetSubsystem<UCommonSessionSubsystem>()->HostSession(GetOwningPlayer(), Request);
}
