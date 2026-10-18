#include "DHJoinScreen.h"

#include "Blueprint/WidgetTree.h"
#include "CommonSessionSubsystem.h"
#include "CommonUserSubsystem.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "DHCardButton.h"
#include "DHScreenStyle.h"
#include "Engine/GameInstance.h"
#include "Misc/Paths.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DHJoinScreen)

namespace
{
	FString Setting(const UCommonSession_SearchResult* Result, const TCHAR* Key)
	{
		FString Value;
		bool bFound = false;
		Result->GetStringSetting(FName(Key), Value, bFound);
		return bFound ? Value : FString();
	}

	// "DA_DH_TDM_Downtown" -> "TDM Downtown", "/Game/DH/Maps/DH_Park" -> "Waterfront"
	FString Pretty(FString Text)
	{
		Text.RemoveFromStart(TEXT("DA_DH_"));
		Text = FPaths::GetBaseFilename(Text);
		static const TMap<FString, FString> Maps = { { TEXT("DH_Block"), TEXT("Downtown") }, { TEXT("DH_Park"), TEXT("Waterfront") }, { TEXT("DH_Depot"), TEXT("Depot") } };
		if (const FString* Name = Maps.Find(Text))
		{
			return *Name;
		}
		return Text.Replace(TEXT("_"), TEXT(" "));
	}
}

UDHJoinScreen::UDHJoinScreen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsBackHandler = true;
}

void UDHJoinScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
	WidgetTree->RootWidget = Root;
	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>();
	Dim->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.65f));
	UOverlaySlot* DimSlot = Root->AddChildToOverlay(Dim);
	DimSlot->SetHorizontalAlignment(HAlign_Fill);
	DimSlot->SetVerticalAlignment(VAlign_Fill);

	USizeBox* Width = WidgetTree->ConstructWidget<USizeBox>();
	Width->SetWidthOverride(1100.f);
	UOverlaySlot* WidthSlot = Root->AddChildToOverlay(Width);
	WidthSlot->SetHorizontalAlignment(HAlign_Center);
	WidthSlot->SetVerticalAlignment(VAlign_Fill);
	WidthSlot->SetPadding(FMargin(0, 50, 0, 40));
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	Width->AddChild(Column);

	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>();
	Column->AddChildToVerticalBox(Header)->SetPadding(FMargin(0, 0, 0, 18));
	UTextBlock* Title = DHScreenStyle::MakeText(WidgetTree, TEXT("Orbitron"), 40, DHScreenStyle::Accent);
	Title->SetText(NSLOCTEXT("DowntownHunting", "JoinTitle", "JOIN A GAME"));
	UHorizontalBoxSlot* TitleSlot = Header->AddChildToHorizontalBox(Title);
	TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	TitleSlot->SetVerticalAlignment(VAlign_Center);
	for (const TCHAR* Id : { TEXT("Network"), TEXT("Refresh") })
	{
		UDHCardButton* Button = WidgetTree->ConstructWidget<UDHCardButton>();
		Button->CardId = Id;
		Button->OnCardClicked.BindUObject(this, &ThisClass::HandleCardClicked);
		UTextBlock* Label = DHScreenStyle::MakeText(WidgetTree, TEXT("Orbitron"), 15, DHScreenStyle::Text);
		Label->SetText(NSLOCTEXT("DowntownHunting", "JoinRefresh", "REFRESH"));
		if (FCString::Strcmp(Id, TEXT("Network")) == 0)
		{
			NetworkText = Label;
		}
		Button->AddChild(Label);
		UHorizontalBoxSlot* ButtonSlot = Header->AddChildToHorizontalBox(Button);
		ButtonSlot->SetVerticalAlignment(VAlign_Center);
		ButtonSlot->SetPadding(FMargin(10, 0, 0, 0));
		Buttons.Add(Button);
	}
	UpdateNetworkText();

	StatusText = DHScreenStyle::MakeText(WidgetTree, TEXT("NotoSans"), 16, DHScreenStyle::Text);
	Column->AddChildToVerticalBox(StatusText)->SetPadding(FMargin(4, 0, 0, 10));

	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
	Column->AddChildToVerticalBox(Scroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	List = WidgetTree->ConstructWidget<UVerticalBox>();
	Scroll->AddChild(List);
}

void UDHJoinScreen::NativeOnActivated()
{
	Super::NativeOnActivated();
	Search();
}

UWidget* UDHJoinScreen::NativeGetDesiredFocusTarget() const
{
	return Buttons.Num() > 2 ? Buttons[2].Get() : Buttons.Num() > 1 ? Buttons[1].Get() : nullptr;
}

void UDHJoinScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	for (UDHCardButton* Button : Buttons)
	{
		const bool bFocused = Button->HasAnyUserFocus() || Button->IsHovered();
		Button->SetBackgroundColor(bFocused ? DHScreenStyle::Focus : DHScreenStyle::Card);
	}
}

void UDHJoinScreen::UpdateNetworkText()
{
	NetworkText->SetText(bOnline ? NSLOCTEXT("DowntownHunting", "JoinOnline", "NETWORK: ONLINE") : NSLOCTEXT("DowntownHunting", "JoinLan", "NETWORK: LAN"));
}

void UDHJoinScreen::HandleCardClicked(UDHCardButton* Card)
{
	if (Card->CardId == TEXT("Network"))
	{
		bOnline = !bOnline;
		UpdateNetworkText();
		Search();
	}
	else if (Card->CardId == TEXT("Refresh"))
	{
		Search();
	}
	else if (Results.IsValidIndex(Card->CardSlot))
	{
		StatusText->SetText(NSLOCTEXT("DowntownHunting", "JoinJoining", "Joining..."));
		GetGameInstance()->GetSubsystem<UCommonSessionSubsystem>()->JoinSession(GetOwningPlayer(), Results[Card->CardSlot]);
	}
}

void UDHJoinScreen::Search()
{
	// Drop the old rows (keep the two switches).
	List->ClearChildren();
	Buttons.SetNum(2);
	Results.Reset();
	StatusText->SetText(NSLOCTEXT("DowntownHunting", "JoinSearching", "Searching..."));

	if (bOnline)
	{
		// Online search needs the online login first (Steam etc.).
		UCommonUserSubsystem* Users = GetGameInstance()->GetSubsystem<UCommonUserSubsystem>();
		Users->OnUserInitializeComplete.AddUniqueDynamic(this, &ThisClass::HandleUserInitialized);
		Users->TryToLoginForOnlinePlay(0);
		return;
	}
	UCommonSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<UCommonSessionSubsystem>();
	Request = Sessions->CreateOnlineSearchSessionRequest();
	Request->OnlineMode = ECommonSessionOnlineMode::LAN;
	Request->OnSearchFinished.AddUObject(this, &ThisClass::HandleSearchFinished);
	Sessions->FindSessions(GetOwningPlayer(), Request);
}

void UDHJoinScreen::HandleUserInitialized(const UCommonUserInfo* UserInfo, bool bSuccess, FText Error, ECommonUserPrivilege RequestedPrivilege, ECommonUserOnlineContext OnlineContext)
{
	GetGameInstance()->GetSubsystem<UCommonUserSubsystem>()->OnUserInitializeComplete.RemoveDynamic(this, &ThisClass::HandleUserInitialized);
	if (!bSuccess)
	{
		StatusText->SetText(FText::Format(NSLOCTEXT("DowntownHunting", "JoinLoginFailed", "Online login failed: {0}"), Error));
		return;
	}
	UCommonSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<UCommonSessionSubsystem>();
	Request = Sessions->CreateOnlineSearchSessionRequest();
	Request->OnlineMode = ECommonSessionOnlineMode::Online;
	Request->OnSearchFinished.AddUObject(this, &ThisClass::HandleSearchFinished);
	Sessions->FindSessions(GetOwningPlayer(), Request);
}

void UDHJoinScreen::HandleSearchFinished(bool bSucceeded, const FText& Error)
{
	Results = Request ? Request->Results : TArray<TObjectPtr<UCommonSession_SearchResult>>();
	StatusText->SetText(Results.IsEmpty()
		? (bSucceeded ? NSLOCTEXT("DowntownHunting", "JoinNone", "No games found. Host one in PLAY, or press REFRESH.")
			: FText::Format(NSLOCTEXT("DowntownHunting", "JoinFailed", "Search failed: {0}"), Error))
		: FText::Format(NSLOCTEXT("DowntownHunting", "JoinFound", "{0} games found"), Results.Num()));

	for (int32 Index = 0; Index < Results.Num(); ++Index)
	{
		const UCommonSession_SearchResult* Result = Results[Index];
		UDHCardButton* Row = WidgetTree->ConstructWidget<UDHCardButton>();
		Row->CardId = TEXT("Session");
		Row->CardSlot = Index;
		Row->OnCardClicked.BindUObject(this, &ThisClass::HandleCardClicked);
		UHorizontalBox* Cells = WidgetTree->ConstructWidget<UHorizontalBox>();
		FString Host;
#if COMMONUSER_OSSV1
		Host = Result->Result.Session.OwningUserName;
#endif
		const int32 Max = Result->GetMaxPublicConnections();
		const FString CellTexts[] = {
			Host.IsEmpty() ? TEXT("-") : Host,
			Pretty(Setting(Result, TEXT("GAMEMODE"))),
			Pretty(Setting(Result, TEXT("MAPNAME"))),
			FString::Printf(TEXT("%d / %d"), Max - Result->GetNumOpenPublicConnections(), Max),
			FString::Printf(TEXT("%d ms"), Result->GetPingInMs()) };
		const float Widths[] = { 300.f, 260.f, 220.f, 120.f, 100.f };
		for (int32 Column = 0; Column < UE_ARRAY_COUNT(CellTexts); ++Column)
		{
			USizeBox* Cell = WidgetTree->ConstructWidget<USizeBox>();
			Cell->SetWidthOverride(Widths[Column]);
			UTextBlock* Text = DHScreenStyle::MakeText(WidgetTree, Column < 2 ? TEXT("Orbitron") : TEXT("NotoSans"), 15, Column == 1 ? DHScreenStyle::Accent : DHScreenStyle::Text);
			Text->SetText(FText::FromString(CellTexts[Column]));
			Text->SetClipping(EWidgetClipping::ClipToBounds);
			Cell->AddChild(Text);
			Cells->AddChildToHorizontalBox(Cell)->SetPadding(FMargin(8, 6));
		}
		Row->AddChild(Cells);
		List->AddChildToVerticalBox(Row)->SetPadding(FMargin(0, 3));
		Buttons.Add(Row);
	}
	if (Buttons.Num() > 2 && (HasAnyUserFocus() || HasFocusedDescendants()))
	{
		Buttons[2]->SetFocus();
	}
}
