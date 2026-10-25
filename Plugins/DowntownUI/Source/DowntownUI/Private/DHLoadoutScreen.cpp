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
#include "DHProgression.h"
#include "DHScreenStyle.h"
#include "Engine/Texture2D.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DHLoadoutScreen)

namespace
{
	using FSettings = UDHLoadoutSettings;

	constexpr float LabelWidth = 190.f;
	const TCHAR* SecondAbilityId = TEXT("SecondAbility");

	FString WeaponOf(const FString& Entry)
	{
		FString Weapon, Attachment;
		FSettings::SplitEntry(Entry, Weapon, Attachment);
		return Weapon;
	}

	FString AttachmentOf(const FString& Entry)
	{
		FString Weapon, Attachment;
		FSettings::SplitEntry(Entry, Weapon, Attachment);
		return Attachment;
	}

	FText LockText(const FString& Id)
	{
		return FText::Format(NSLOCTEXT("DowntownHunting", "UnlocksAt", "Unlocks at level {0}"), FDHProgression::UnlockLevel(Id));
	}
}

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

	UTextBlock* Title = DHScreenStyle::MakeText(WidgetTree, TEXT("Orbitron"), 34, DHScreenStyle::Accent);
	Title->SetText(NSLOCTEXT("DowntownHunting", "LoadoutTitle", "LOADOUT"));
	Column->AddChildToVerticalBox(Title)->SetPadding(FMargin(0, 0, 0, 6));
	UHorizontalBox* Tabs = AddRow(Column, NSLOCTEXT("DowntownHunting", "LoadoutTabs", "USE"));
	for (int32 Index = 0; Index < FSettings::NumLoadouts; ++Index)
	{
		UTextBlock* Label = DHScreenStyle::MakeText(WidgetTree, TEXT("Orbitron"), 13, DHScreenStyle::Text);
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>();
		Box->SetWidthOverride(150.f);
		Box->AddChild(Label);
		AddCard(Tabs, EKind::Tab, Index, FString(), Box, false);
		Cards.Last().Label = Label;
	}

	SummaryText = DHScreenStyle::MakeText(WidgetTree, TEXT("NotoSans"), 15, DHScreenStyle::Text);
	Column->AddChildToVerticalBox(SummaryText)->SetPadding(FMargin(LabelWidth + 8, 2, 0, 10));

	AddWeaponRow(Column, NSLOCTEXT("DowntownHunting", "Primary", "PRIMARY"), FSettings::Primary);
	AddAttachmentRow(Column, FSettings::Primary);
	AddWeaponRow(Column, NSLOCTEXT("DowntownHunting", "Secondary", "SECONDARY"), FSettings::Secondary);
	AddAttachmentRow(Column, FSettings::Secondary);
	AddAbilityRow(Column, NSLOCTEXT("DowntownHunting", "AbilityRow", "ABILITY"), FSettings::Ability);
	AddPerkRow(Column);
	AddAbilityRow(Column, NSLOCTEXT("DowntownHunting", "Ability2Row", "2ND ABILITY"), FSettings::Ability2);

	UButton* Back = WidgetTree->ConstructWidget<UButton>();
	Back->SetBackgroundColor(DHScreenStyle::Card);
	UTextBlock* BackText = DHScreenStyle::MakeText(WidgetTree, TEXT("Orbitron"), 20, DHScreenStyle::Text);
	BackText->SetText(NSLOCTEXT("DowntownHunting", "Back", "BACK"));
	Back->AddChild(BackText);
	Back->OnClicked.AddDynamic(this, &ThisClass::HandleBackClicked);
	UVerticalBoxSlot* BackSlot = Column->AddChildToVerticalBox(Back);
	BackSlot->SetHorizontalAlignment(HAlign_Center);
	BackSlot->SetPadding(FMargin(0, 18, 0, 0));
}

UHorizontalBox* UDHLoadoutScreen::AddRow(UVerticalBox* Column, const FText& Title)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	Column->AddChildToVerticalBox(Row)->SetPadding(FMargin(0, 4));
	USizeBox* LabelBox = WidgetTree->ConstructWidget<USizeBox>();
	LabelBox->SetWidthOverride(LabelWidth);
	UTextBlock* Label = DHScreenStyle::MakeText(WidgetTree, TEXT("Orbitron"), 18, DHScreenStyle::Text);
	Label->SetText(Title);
	LabelBox->AddChild(Label);
	Row->AddChildToHorizontalBox(LabelBox)->SetVerticalAlignment(VAlign_Center);
	return Row;
}

UDHCardButton* UDHLoadoutScreen::AddCard(UHorizontalBox* Row, EKind Kind, int32 Entry, const FString& Id, UWidget* Content, bool bLocked)
{
	UDHCardButton* Button = WidgetTree->ConstructWidget<UDHCardButton>();
	Button->SetBackgroundColor(DHScreenStyle::Card);
	Button->CardSlot = Cards.Num();
	Button->OnCardClicked.BindUObject(this, &ThisClass::HandleCardClicked);
	Button->AddChild(Content);
	Content->SetRenderOpacity(bLocked ? 0.35f : 1.f);
	Row->AddChildToHorizontalBox(Button)->SetPadding(FMargin(6, 0));
	Cards.Add({ Button, Kind, Entry, Id, nullptr });
	KeepAlive.Add(Button);
	return Button;
}

void UDHLoadoutScreen::AddWeaponRow(UVerticalBox* Column, const FText& Title, int32 Entry)
{
	UHorizontalBox* Row = AddRow(Column, Title);
	for (const FDHWeaponEntry& Weapon : FSettings::GetWeapons())
	{
		UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
		USizeBox* IconBox = WidgetTree->ConstructWidget<USizeBox>();
		IconBox->SetWidthOverride(150.f);
		IconBox->SetHeightOverride(70.f);
		UImage* Icon = WidgetTree->ConstructWidget<UImage>();
		if (UTexture2D* Texture = Cast<UTexture2D>(Weapon.Icon.TryLoad()))
		{
			Icon->SetBrushFromTexture(Texture);
		}
		IconBox->AddChild(Icon);
		Content->AddChildToVerticalBox(IconBox);

		const bool bLocked = !FDHProgression::IsUnlocked(Weapon.Id);
		UTextBlock* Name = DHScreenStyle::MakeText(WidgetTree, TEXT("Orbitron"), 13, DHScreenStyle::Text);
		Name->SetText(bLocked ? FText::Format(NSLOCTEXT("DowntownHunting", "LockedWeapon", "{0}  ·  LVL {1}"), FText::FromString(Weapon.Id.ToUpper()), FDHProgression::UnlockLevel(Weapon.Id))
			: FText::FromString(Weapon.Id.ToUpper()));
		Content->AddChildToVerticalBox(Name)->SetHorizontalAlignment(HAlign_Center);
		AddCard(Row, EKind::Weapon, Entry, Weapon.Id, Content, bLocked);
	}
}

void UDHLoadoutScreen::AddAttachmentRow(UVerticalBox* Column, int32 Entry)
{
	UHorizontalBox* Row = AddRow(Column, FText::GetEmpty());
	for (const FDHAttachmentEntry& Attachment : FSettings::GetAttachments())
	{
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>();
		Box->SetWidthOverride(170.f);
		UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
		UTextBlock* Name = DHScreenStyle::MakeText(WidgetTree, TEXT("Orbitron"), 12, DHScreenStyle::Text);
		Name->SetText(Attachment.Name);
		Content->AddChildToVerticalBox(Name)->SetHorizontalAlignment(HAlign_Center);
		const bool bLocked = !FDHProgression::IsUnlocked(Attachment.Id);
		UTextBlock* Info = DHScreenStyle::MakeText(WidgetTree, TEXT("NotoSans"), 11, DHScreenStyle::Text);
		Info->SetText(bLocked ? LockText(Attachment.Id) : Attachment.Description);
		Content->AddChildToVerticalBox(Info)->SetHorizontalAlignment(HAlign_Center);
		Box->AddChild(Content);
		AddCard(Row, EKind::Attachment, Entry, Attachment.Id, Box, bLocked);
	}
}

void UDHLoadoutScreen::AddAbilityRow(UVerticalBox* Column, const FText& Title, int32 Entry)
{
	UHorizontalBox* Row = AddRow(Column, Title);
	const bool bSecond = Entry == FSettings::Ability2;
	TArray<FDHAbilityEntry> Abilities = FSettings::GetAbilities();
	if (bSecond)
	{
		Abilities.Insert({ FString(), NSLOCTEXT("DowntownHunting", "AbilityNone", "NONE"), FText::GetEmpty() }, 0);
	}
	for (const FDHAbilityEntry& Ability : Abilities)
	{
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>();
		Box->SetWidthOverride(bSecond && Ability.Id.IsEmpty() ? 110.f : 190.f);
		UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
		UTextBlock* Name = DHScreenStyle::MakeText(WidgetTree, TEXT("Orbitron"), 13, DHScreenStyle::Text);
		Name->SetText(Ability.Name);
		Content->AddChildToVerticalBox(Name)->SetHorizontalAlignment(HAlign_Center);

		// The second slot unlocks late; until then its cards say so.
		const FString LockId = bSecond && !FDHProgression::IsUnlocked(SecondAbilityId) ? FString(SecondAbilityId) : Ability.Id;
		const bool bLocked = !FDHProgression::IsUnlocked(LockId);
		FText Info = Ability.Description;
		if (bLocked)
		{
			Info = LockText(LockId);
		}
		else if (Ability.Cooldown > 0.f)
		{
			Info = Ability.Duration > 0.f
				? FText::Format(NSLOCTEXT("DowntownHunting", "AbilityTimes", "{0}\n{1} s  ·  recharge {2} s"), Info, Ability.Duration, Ability.Cooldown)
				: FText::Format(NSLOCTEXT("DowntownHunting", "AbilityRecharge", "{0}\nrecharge {1} s"), Info, Ability.Cooldown);
		}
		UTextBlock* InfoText = DHScreenStyle::MakeText(WidgetTree, TEXT("NotoSans"), 11, DHScreenStyle::Text);
		InfoText->SetText(Info);
		InfoText->SetAutoWrapText(true);
		InfoText->SetJustification(ETextJustify::Center);
		Content->AddChildToVerticalBox(InfoText)->SetHorizontalAlignment(HAlign_Center);
		Box->AddChild(Content);
		AddCard(Row, EKind::Ability, Entry, Ability.Id, Box, bLocked);
	}
}

void UDHLoadoutScreen::AddPerkRow(UVerticalBox* Column)
{
	UHorizontalBox* Row = AddRow(Column, NSLOCTEXT("DowntownHunting", "PerkRow", "PERK"));
	for (const FDHAbilityEntry& Perk : FSettings::GetPerks())
	{
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>();
		Box->SetWidthOverride(230.f);
		UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
		UTextBlock* Name = DHScreenStyle::MakeText(WidgetTree, TEXT("Orbitron"), 13, DHScreenStyle::Text);
		Name->SetText(Perk.Name);
		Content->AddChildToVerticalBox(Name)->SetHorizontalAlignment(HAlign_Center);
		const bool bLocked = !FDHProgression::IsUnlocked(Perk.Id);
		UTextBlock* Info = DHScreenStyle::MakeText(WidgetTree, TEXT("NotoSans"), 11, DHScreenStyle::Text);
		Info->SetText(bLocked ? LockText(Perk.Id) : Perk.Description);
		Content->AddChildToVerticalBox(Info)->SetHorizontalAlignment(HAlign_Center);
		Box->AddChild(Content);
		AddCard(Row, EKind::Perk, FSettings::Perk, Perk.Id, Box, bLocked);
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
		Card.Button->SetBackgroundColor(IsChosen(Card) ? DHScreenStyle::Selected : bFocused ? DHScreenStyle::Focus : DHScreenStyle::Card);
		Card.Button->Animate(InDeltaTime);
	}
}

bool UDHLoadoutScreen::IsChosen(const FCard& Card) const
{
	const FString& Entry = CurrentLoadout.IsValidIndex(Card.Entry) ? CurrentLoadout[Card.Entry] : FString();
	switch (Card.Kind)
	{
	case EKind::Tab: return Card.Entry == CurrentIndex;
	case EKind::Weapon: return WeaponOf(Entry) == Card.Id;
	case EKind::Attachment: return AttachmentOf(Entry) == Card.Id;
	default: return Entry == Card.Id;
	}
}

void UDHLoadoutScreen::HandleCardClicked(UDHCardButton* Button)
{
	if (Cards.IsValidIndex(Button->CardSlot))
	{
		Pick(Cards[Button->CardSlot]);
	}
}

void UDHLoadoutScreen::HandleBackClicked()
{
	DeactivateWidget();
}

void UDHLoadoutScreen::Pick(const FCard& Card)
{
	if (Card.Kind == EKind::Tab)
	{
		FSettings::SetActiveLoadout(GetOwningPlayer(), Card.Entry);
		Refresh();
		return;
	}
	if (!FDHProgression::IsUnlocked(Card.Id) || (Card.Entry == FSettings::Ability2 && !FDHProgression::IsUnlocked(SecondAbilityId)))
	{
		return;
	}
	TArray<FString> Loadout = FSettings::GetLoadout();
	FString& Entry = Loadout[Card.Entry];
	switch (Card.Kind)
	{
	case EKind::Weapon:
	{
		// Swap instead of carrying the same weapon twice; the attachments stay with their slot.
		FString& Other = Loadout[Card.Entry == FSettings::Primary ? FSettings::Secondary : FSettings::Primary];
		const FString OldWeapon = WeaponOf(Entry);
		const FString EntryAttachment = AttachmentOf(Entry);
		if (WeaponOf(Other) == Card.Id)
		{
			const FString OtherAttachment = AttachmentOf(Other);
			Other = OtherAttachment.IsEmpty() ? OldWeapon : OldWeapon + TEXT(":") + OtherAttachment;
		}
		Entry = EntryAttachment.IsEmpty() ? Card.Id : Card.Id + TEXT(":") + EntryAttachment;
		break;
	}
	case EKind::Attachment:
		Entry = Card.Id.IsEmpty() ? WeaponOf(Entry) : WeaponOf(Entry) + TEXT(":") + Card.Id;
		break;
	case EKind::Ability:
	{
		FString& Other = Loadout[Card.Entry == FSettings::Ability ? FSettings::Ability2 : FSettings::Ability];
		if (Other == Card.Id)
		{
			if (Entry.IsEmpty())
			{
				return;  // the first slot must keep an ability
			}
			Other = Entry;  // swap
		}
		Entry = Card.Id;
		break;
	}
	default:
		Entry = Card.Id;
		break;
	}
	FSettings::ChooseLoadout(GetOwningPlayer(), Loadout);
	Refresh();
}

void UDHLoadoutScreen::Refresh()
{
	CurrentIndex = FSettings::GetActiveLoadout();
	CurrentLoadout = FSettings::GetLoadout();

	// Tabs show their primary weapon.
	for (const FCard& Card : Cards)
	{
		if (Card.Kind == EKind::Tab)
		{
			Card.Label->SetText(FText::Format(NSLOCTEXT("DowntownHunting", "LoadoutTab", "{0}  {1}"), Card.Entry + 1,
				FText::FromString(WeaponOf(FSettings::GetLoadoutAt(Card.Entry)[FSettings::Primary]).ToUpper())));
		}
	}

	auto WeaponText = [](const FString& Entry)
	{
		FText Text = FText::FromString(WeaponOf(Entry).IsEmpty() ? TEXT("-") : WeaponOf(Entry));
		for (const FDHAttachmentEntry& Attachment : FSettings::GetAttachments())
		{
			if (!AttachmentOf(Entry).IsEmpty() && Attachment.Id == AttachmentOf(Entry))
			{
				Text = FText::Format(NSLOCTEXT("DowntownHunting", "WeaponWithAttachment", "{0} ({1})"), Text, Attachment.Name);
			}
		}
		return Text;
	};
	FString Abilities;
	for (const int32 Entry : { int32(FSettings::Ability), int32(FSettings::Ability2) })
	{
		if (const FDHAbilityEntry* Ability = FSettings::FindAbility(CurrentLoadout[Entry]))
		{
			Abilities += (Abilities.IsEmpty() ? TEXT("") : TEXT(" + ")) + Ability->Name.ToString();
		}
	}
	SummaryText->SetText(FText::Format(
		NSLOCTEXT("DowntownHunting", "LoadoutSummary", "{0} + {1}  ·  {2}\nThe highlighted loadout is used. Weapons apply from your next spawn in modes without a fixed loadout, abilities everywhere."),
		WeaponText(CurrentLoadout[FSettings::Primary]), WeaponText(CurrentLoadout[FSettings::Secondary]), FText::FromString(Abilities)));
}
