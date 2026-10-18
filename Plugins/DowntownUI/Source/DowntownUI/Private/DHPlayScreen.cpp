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
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "DHCardButton.h"
#include "DHScreenStyle.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "GameplayTagContainer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DHPlayScreen)

namespace
{
	constexpr int32 Columns = 4;

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

	UTextBlock* MakeText(UWidgetTree* Tree, const TCHAR* Font, int32 Size, const FLinearColor& Colour)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>();
		Text->SetFont(DHScreenStyle::Font(Font, Size));
		Text->SetColorAndOpacity(Colour);
		return Text;
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
	UTextBlock* Title = MakeText(WidgetTree, TEXT("Orbitron"), 40, DHScreenStyle::Accent);
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
		UTextBlock* Label = MakeText(WidgetTree, TEXT("Orbitron"), 15, DHScreenStyle::Text);
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

	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
	UVerticalBoxSlot* ScrollSlot = Column->AddChildToVerticalBox(Scroll);
	ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UUniformGridPanel* Grid = WidgetTree->ConstructWidget<UUniformGridPanel>();
	Grid->SetSlotPadding(FMargin(8));
	Scroll->AddChild(Grid);

	for (int32 Index = 0; Index < Modes.Num(); ++Index)
	{
		FMode& Mode = Modes[Index];
		Mode.Button = WidgetTree->ConstructWidget<UDHCardButton>();
		Mode.Button->SetBackgroundColor(DHScreenStyle::Card);
		Mode.Button->CardSlot = Index;
		Mode.Button->CardId = TEXT("Mode");
		Mode.Button->OnCardClicked.BindUObject(this, &ThisClass::HandleCardClicked);

		UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
		USizeBox* ImageBox = WidgetTree->ConstructWidget<USizeBox>();
		ImageBox->SetWidthOverride(320.f);
		ImageBox->SetHeightOverride(160.f);
		UImage* Tile = WidgetTree->ConstructWidget<UImage>();
		const FObjectProperty* Icon = FindFProperty<FObjectProperty>(Mode.Definition->GetClass(), TEXT("TileIcon"));
		if (UTexture2D* Texture = Icon ? Cast<UTexture2D>(Icon->GetObjectPropertyValue_InContainer(Mode.Definition)) : nullptr)
		{
			Tile->SetBrushFromTexture(Texture);
		}
		ImageBox->AddChild(Tile);
		Content->AddChildToVerticalBox(ImageBox);
		const FString TitleText = Mode.Title.ToString().ToUpper();
		UTextBlock* ModeTitle = MakeText(WidgetTree, TEXT("Orbitron"), TitleText.Len() > 22 ? 12 : 16, DHScreenStyle::Text);
		ModeTitle->SetText(FText::FromString(TitleText));
		ModeTitle->SetClipping(EWidgetClipping::ClipToBounds);
		ModeTitle->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
		Content->AddChildToVerticalBox(ModeTitle)->SetPadding(FMargin(4, 6, 4, 0));
		UTextBlock* ModeMap = MakeText(WidgetTree, TEXT("NotoSans"), 13, DHScreenStyle::Accent);
		ModeMap->SetText(Mode.Subtitle);
		Content->AddChildToVerticalBox(ModeMap)->SetPadding(FMargin(4, 0, 4, 4));
		USizeBox* CardBox = WidgetTree->ConstructWidget<USizeBox>();  // same width for every card
		CardBox->SetWidthOverride(320.f);
		CardBox->AddChild(Content);
		Mode.Button->AddChild(CardBox);

		Grid->AddChildToUniformGrid(Mode.Button, Index / Columns, Index % Columns);
		KeepAlive.Add(Mode.Button);
		KeepAlive.Add(Mode.Definition);
	}

	DescriptionText = MakeText(WidgetTree, TEXT("NotoSans"), 16, DHScreenStyle::Text);
	DescriptionText->SetAutoWrapText(true);
	UVerticalBoxSlot* DescriptionSlot = Column->AddChildToVerticalBox(DescriptionText);
	DescriptionSlot->SetPadding(FMargin(8, 16, 8, 0));
}

UWidget* UDHPlayScreen::NativeGetDesiredFocusTarget() const
{
	return Modes.IsEmpty() ? nullptr : Modes[0].Button.Get();
}

void UDHPlayScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	for (const FMode& Mode : Modes)
	{
		const bool bFocused = Mode.Button->HasAnyUserFocus() || Mode.Button->IsHovered();
		Mode.Button->SetBackgroundColor(bFocused ? DHScreenStyle::Focus : DHScreenStyle::Card);
		Mode.Button->SetRenderScale(FVector2D(bFocused ? 1.04f : 1.f));
		if (Mode.Button->HasAnyUserFocus())
		{
			DescriptionText->SetText(Mode.Description);
			if (UScrollBox* Scroll = Cast<UScrollBox>(Mode.Button->GetParent() ? Mode.Button->GetParent()->GetParent() : nullptr))
			{
				Scroll->ScrollWidgetIntoView(Mode.Button, true, EDescendantScrollDestination::IntoView);
			}
		}
	}
	for (UDHCardButton* Switch : Switches)
	{
		const bool bFocused = Switch->HasAnyUserFocus() || Switch->IsHovered();
		Switch->SetBackgroundColor(bFocused ? DHScreenStyle::Focus : DHScreenStyle::Card);
	}
}

void UDHPlayScreen::UpdateSwitches()
{
	static const FText NetworkNames[] = {
		NSLOCTEXT("DowntownHunting", "NetOffline", "OFFLINE"), NSLOCTEXT("DowntownHunting", "NetLan", "LAN"), NSLOCTEXT("DowntownHunting", "NetOnline", "ONLINE") };
	NetworkText->SetText(FText::Format(NSLOCTEXT("DowntownHunting", "PlayNetwork", "NETWORK: {0}"), NetworkNames[Network]));
	BotsText->SetText(bBots ? NSLOCTEXT("DowntownHunting", "PlayBotsOn", "BOTS: ON") : NSLOCTEXT("DowntownHunting", "PlayBotsOff", "BOTS: OFF"));
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
		bBots = !bBots;
		UpdateSwitches();
	}
	else if (Card->CardId == TEXT("Join"))
	{
		if (UClass* Browser = LoadClass<UCommonActivatableWidget>(nullptr, TEXT("/Game/UI/Menu/Experiences/W_SessionBrowserScreen.W_SessionBrowserScreen_C")))
		{
			UCommonUIExtensions::PushContentToLayer_ForPlayer(GetOwningLocalPlayer(), FGameplayTag::RequestGameplayTag(TEXT("UI.Layer.Menu")), Browser);
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
	if (!bBots)
	{
		Request->ExtraArgs.Add(TEXT("NumBots"), TEXT("0"));
	}
	GetGameInstance()->GetSubsystem<UCommonSessionSubsystem>()->HostSession(GetOwningPlayer(), Request);
}
