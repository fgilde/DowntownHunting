#include "DHLoadoutScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
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
#include "DHLoadoutSettings.h"
#include "DHScreenStyle.h"
#include "Engine/Texture2D.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DHLoadoutScreen)

UDHLoadoutScreen::UDHLoadoutScreen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsBackHandler = true;
	bIsModal = true;
}

void UDHLoadoutScreen::NativeOnInitialized()
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

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>();
	Title->SetText(NSLOCTEXT("DowntownHunting", "LoadoutTitle", "LOADOUT"));
	Title->SetFont(DHScreenStyle::Font(TEXT("Orbitron"), 40));
	Title->SetColorAndOpacity(DHScreenStyle::Accent);
	Column->AddChildToVerticalBox(Title)->SetHorizontalAlignment(HAlign_Center);

	SummaryText = WidgetTree->ConstructWidget<UTextBlock>();
	SummaryText->SetFont(DHScreenStyle::Font(TEXT("NotoSans"), 16));
	SummaryText->SetColorAndOpacity(DHScreenStyle::Text);
	SummaryText->SetJustification(ETextJustify::Center);
	UVerticalBoxSlot* SummarySlot = Column->AddChildToVerticalBox(SummaryText);
	SummarySlot->SetHorizontalAlignment(HAlign_Center);
	SummarySlot->SetPadding(FMargin(0, 6, 0, 20));

	AddRow(Column, NSLOCTEXT("DowntownHunting", "Primary", "PRIMARY"), 0);
	AddRow(Column, NSLOCTEXT("DowntownHunting", "Secondary", "SECONDARY"), 1);

	UButton* Back = WidgetTree->ConstructWidget<UButton>();
	Back->SetBackgroundColor(DHScreenStyle::Card);
	UTextBlock* BackText = WidgetTree->ConstructWidget<UTextBlock>();
	BackText->SetText(NSLOCTEXT("DowntownHunting", "Back", "BACK"));
	BackText->SetFont(DHScreenStyle::Font(TEXT("Orbitron"), 20));
	Back->AddChild(BackText);
	Back->OnClicked.AddDynamic(this, &ThisClass::HandleBackClicked);
	UVerticalBoxSlot* BackSlot = Column->AddChildToVerticalBox(Back);
	BackSlot->SetHorizontalAlignment(HAlign_Center);
	BackSlot->SetPadding(FMargin(0, 28, 0, 0));
}

void UDHLoadoutScreen::AddRow(UVerticalBox* Column, const FText& Title, int32 SlotIndex)
{
	UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
	Label->SetText(Title);
	Label->SetFont(DHScreenStyle::Font(TEXT("Orbitron"), 22));
	UVerticalBoxSlot* LabelSlot = Column->AddChildToVerticalBox(Label);
	LabelSlot->SetPadding(FMargin(8, 12, 0, 6));

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	Column->AddChildToVerticalBox(Row);
	for (const FDHWeaponEntry& Weapon : UDHLoadoutSettings::GetWeapons())
	{
		UDHCardButton* Button = WidgetTree->ConstructWidget<UDHCardButton>();
		Button->SetBackgroundColor(DHScreenStyle::Card);
		Button->CardId = Weapon.Id;
		Button->CardSlot = SlotIndex;
		Button->OnCardClicked.BindUObject(this, &ThisClass::HandleCardClicked);

		UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
		USizeBox* IconBox = WidgetTree->ConstructWidget<USizeBox>();
		IconBox->SetWidthOverride(240.f);
		IconBox->SetHeightOverride(120.f);
		UImage* Icon = WidgetTree->ConstructWidget<UImage>();
		if (UTexture2D* Texture = Cast<UTexture2D>(Weapon.Icon.TryLoad()))
		{
			Icon->SetBrushFromTexture(Texture);
		}
		IconBox->AddChild(Icon);
		Content->AddChildToVerticalBox(IconBox);

		UTextBlock* Name = WidgetTree->ConstructWidget<UTextBlock>();
		Name->SetText(FText::FromString(Weapon.Id.ToUpper()));
		Name->SetFont(DHScreenStyle::Font(TEXT("Orbitron"), 16));
		UVerticalBoxSlot* NameSlot = Content->AddChildToVerticalBox(Name);
		NameSlot->SetHorizontalAlignment(HAlign_Center);
		NameSlot->SetPadding(FMargin(0, 4, 0, 2));
		Button->AddChild(Content);

		Row->AddChildToHorizontalBox(Button)->SetPadding(FMargin(8));
		Cards.Add({ Button, SlotIndex, Weapon.Id });
		CardButtons.Add(Button);
	}
}

void UDHLoadoutScreen::NativeOnActivated()
{
	Super::NativeOnActivated();
	Refresh();
}

UWidget* UDHLoadoutScreen::NativeGetDesiredFocusTarget() const
{
	return Cards.IsEmpty() ? nullptr : Cards[0].Button.Get();
}

void UDHLoadoutScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// UButton draws no focus state; show which card the gamepad/keyboard is on.
	for (const FCard& Card : Cards)
	{
		const bool bFocused = Card.Button->HasAnyUserFocus() || Card.Button->IsHovered();
		const bool bChosen = CurrentLoadout.IsValidIndex(Card.SlotIndex) && CurrentLoadout[Card.SlotIndex] == Card.WeaponId;
		Card.Button->SetBackgroundColor(bChosen ? DHScreenStyle::Accent : bFocused ? DHScreenStyle::Focus : DHScreenStyle::Card);
		Card.Button->SetRenderScale(FVector2D(bFocused ? 1.06f : 1.f));
	}
}

void UDHLoadoutScreen::HandleCardClicked(UDHCardButton* Card)
{
	Pick(Card->CardSlot, Card->CardId);
}

void UDHLoadoutScreen::HandleBackClicked()
{
	DeactivateWidget();
}

void UDHLoadoutScreen::Pick(int32 SlotIndex, const FString& WeaponId)
{
	TArray<FString> Loadout = UDHLoadoutSettings::GetLoadout();
	Loadout.SetNum(2);
	const int32 Other = 1 - SlotIndex;
	if (Loadout[Other] == WeaponId)
	{
		Loadout[Other] = Loadout[SlotIndex];  // swap instead of carrying the same weapon twice
	}
	Loadout[SlotIndex] = WeaponId;
	Loadout.Remove(FString());
	UDHLoadoutSettings::ChooseLoadout(GetOwningPlayer(), Loadout);
	Refresh();
}

void UDHLoadoutScreen::Refresh()
{
	TArray<FString> Loadout = UDHLoadoutSettings::GetLoadout();
	Loadout.SetNum(2);
	CurrentLoadout = Loadout;
	SummaryText->SetText(FText::Format(
		NSLOCTEXT("DowntownHunting", "LoadoutSummary", "{0} + {1}\nUsed in modes without a fixed loadout. Applies from your next spawn."),
		FText::FromString(Loadout[0]), FText::FromString(Loadout[1].IsEmpty() ? TEXT("-") : Loadout[1])));
}
