#include "DHCharacterSelectScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
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

#include UE_INLINE_GENERATED_CPP_BY_NAME(DHCharacterSelectScreen)

namespace
{
	const int32 Columns = 5;
	const FLinearColor CardColor = DHScreenStyle::Card;

	UTexture2D* Portrait(const FString& Id)
	{
		const FString Path = FString::Printf(TEXT("/Game/DH/UI/Portraits/T_DH_Portrait_%s.T_DH_Portrait_%s"), *Id, *Id);
		return LoadObject<UTexture2D>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
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

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	UOverlaySlot* ColumnSlot = Root->AddChildToOverlay(Column);
	ColumnSlot->SetHorizontalAlignment(HAlign_Center);
	ColumnSlot->SetVerticalAlignment(VAlign_Center);

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>();
	Title->SetText(NSLOCTEXT("DowntownHunting", "ChooseHunter", "CHOOSE YOUR HUNTER"));
	Title->SetFont(DHScreenStyle::Font(TEXT("Orbitron"), 40));
	Title->SetColorAndOpacity(DHScreenStyle::Accent);
	Column->AddChildToVerticalBox(Title)->SetHorizontalAlignment(HAlign_Center);

	CurrentText = WidgetTree->ConstructWidget<UTextBlock>();
	CurrentText->SetFont(DHScreenStyle::Font(TEXT("NotoSans"), 18));
	CurrentText->SetColorAndOpacity(DHScreenStyle::Text);
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

UButton* UDHCharacterSelectScreen::MakeCard(const FString& Id, const FText& Label)
{
	UDHCardButton* Card = WidgetTree->ConstructWidget<UDHCardButton>();
	Card->SetBackgroundColor(CardColor);
	Card->CardId = Id;
	Card->OnCardClicked.BindUObject(this, &ThisClass::HandleCardClicked);

	UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
	USizeBox* PortraitBox = WidgetTree->ConstructWidget<USizeBox>();
	PortraitBox->SetWidthOverride(176.f);
	PortraitBox->SetHeightOverride(220.f);
	UImage* Image = WidgetTree->ConstructWidget<UImage>();
	if (UTexture2D* Texture = Portrait(Id.IsEmpty() ? TEXT("Random") : Id))
	{
		Image->SetBrushFromTexture(Texture);
	}
	else
	{
		Image->SetColorAndOpacity(FLinearColor(0.12f, 0.11f, 0.1f));
	}
	PortraitBox->AddChild(Image);
	Content->AddChildToVerticalBox(PortraitBox);

	UTextBlock* Name = WidgetTree->ConstructWidget<UTextBlock>();
	Name->SetText(Label);
	Name->SetFont(DHScreenStyle::Font(TEXT("Orbitron"), 16));
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
	for (int32 Index = 0; Index < Cards.Num(); ++Index)
	{
		const bool bFocused = Cards[Index]->HasAnyUserFocus() || Cards[Index]->IsHovered();
		Cards[Index]->SetBackgroundColor(CardIds[Index] == CurrentId ? DHScreenStyle::Accent : bFocused ? DHScreenStyle::Focus : CardColor);
		Cards[Index]->SetRenderScale(FVector2D(bFocused ? 1.06f : 1.f));
	}
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
