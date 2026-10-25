#include "DHLoadoutScreen.h"

#include "Blueprint/WidgetTree.h"
#include "CommonInputSubsystem.h"
#include "Components/Border.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "DHCardButton.h"
#include "DHLoadoutSettings.h"
#include "DHProgression.h"
#include "DHScreenStyle.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Texture2D.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DHLoadoutScreen)

namespace
{
	using FSettings = UDHLoadoutSettings;

	constexpr float LabelWidth = 170.f;
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

	/** What the details panel says about a weapon; stats 0..1 by feel, for comparing at a glance. */
	struct FWeaponInfo
	{
		const TCHAR* Id;
		FText Kind;
		FText Text;
		float Stats[4];  // damage, fire rate, range, mobility
	};

	const FWeaponInfo* FindWeaponInfo(const FString& Id)
	{
		static const FWeaponInfo Infos[] = {
			{ TEXT("Rifle"), NSLOCTEXT("DowntownHunting", "KindRifle", "ASSAULT RIFLE"), NSLOCTEXT("DowntownHunting", "InfoRifle", "Full auto all-rounder. Good at every distance."), { 0.55f, 0.75f, 0.7f, 0.6f } },
			{ TEXT("Shotgun"), NSLOCTEXT("DowntownHunting", "KindShotgun", "SHOTGUN"), NSLOCTEXT("DowntownHunting", "InfoShotgun", "Clears a room. Useless across the street."), { 1.f, 0.3f, 0.15f, 0.6f } },
			{ TEXT("AK47"), NSLOCTEXT("DowntownHunting", "KindAK47", "ASSAULT RIFLE"), NSLOCTEXT("DowntownHunting", "InfoAK47", "Hits harder than the rifle, kicks harder too."), { 0.7f, 0.6f, 0.75f, 0.5f } },
			{ TEXT("MP5"), NSLOCTEXT("DowntownHunting", "KindMP5", "SUBMACHINE GUN"), NSLOCTEXT("DowntownHunting", "InfoMP5", "Sprays fast, runs fast. Best up close."), { 0.4f, 0.95f, 0.4f, 0.85f } },
			{ TEXT("Sniper"), NSLOCTEXT("DowntownHunting", "KindSniper", "SNIPER RIFLE"), NSLOCTEXT("DowntownHunting", "InfoSniper", "One shot to the head. Aim to look through the scope."), { 1.f, 0.1f, 1.f, 0.3f } },
			{ TEXT("Pistol"), NSLOCTEXT("DowntownHunting", "KindPistol", "PISTOL"), NSLOCTEXT("DowntownHunting", "InfoPistol", "Reliable sidearm, quick to draw."), { 0.35f, 0.5f, 0.45f, 0.95f } },
			{ TEXT("M1911"), NSLOCTEXT("DowntownHunting", "KindM1911", "PISTOL"), NSLOCTEXT("DowntownHunting", "InfoM1911", "Heavy .45: fewer shots, bigger hits."), { 0.5f, 0.35f, 0.45f, 0.95f } },
		};
		for (const FWeaponInfo& Info : Infos)
		{
			if (Id == Info.Id)
			{
				return &Info;
			}
		}
		return nullptr;
	}

	UTexture2D* LoadTexture(const FString& Path)
	{
		return LoadObject<UTexture2D>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	UTexture2D* AbilityIcon(const FString& Id)
	{
		return Id.IsEmpty() ? nullptr : LoadTexture(FString::Printf(TEXT("/Game/DH/UI/Abilities/T_DH_Ability_%s.T_DH_Ability_%s"), *Id, *Id));
	}

	UTexture2D* WeaponIcon(const FString& Id)
	{
		for (const FDHWeaponEntry& Weapon : FSettings::GetWeapons())
		{
			if (Weapon.Id == Id)
			{
				return Cast<UTexture2D>(Weapon.Icon.TryLoad());
			}
		}
		return nullptr;
	}

	// Rolled with the gamepad, where typing is no option.
	const TCHAR* const RandomNames[] = {
		TEXT("NIGHT SHIFT"), TEXT("RUSH HOUR"), TEXT("ROOFTOP KING"), TEXT("ALLEY CAT"), TEXT("NEON GHOST"), TEXT("STREET SWEEPER"),
		TEXT("LAST CALL"), TEXT("BLOCK PARTY"), TEXT("CITY LIGHTS"), TEXT("DOWNTOWN DIVA"), TEXT("TRAFFIC JAM"), TEXT("SKYLINE"),
		TEXT("BACK ALLEY"), TEXT("HOT PURSUIT"), TEXT("GRAVEYARD SHIFT"), TEXT("CORNER STORE"),
	};
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

	UHorizontalBox* Main = WidgetTree->ConstructWidget<UHorizontalBox>();
	UOverlaySlot* MainSlot = Root->AddChildToOverlay(Main);
	MainSlot->SetHorizontalAlignment(HAlign_Center);
	MainSlot->SetVerticalAlignment(VAlign_Center);

	// Left: title and the loadout slots.
	UVerticalBox* Left = WidgetTree->ConstructWidget<UVerticalBox>();
	Main->AddChildToHorizontalBox(Left)->SetPadding(FMargin(0, 0, 36, 0));
	UTextBlock* Title = DHScreenStyle::MakeText(WidgetTree, TEXT("Orbitron"), 40, DHScreenStyle::Accent);
	Title->SetText(NSLOCTEXT("DowntownHunting", "LoadoutsTitle", "LOADOUTS"));
	Title->SetRenderTransformAngle(-2.f);
	Left->AddChildToVerticalBox(Title)->SetPadding(FMargin(0, 0, 0, 4));
	UTextBlock* Explain = DHScreenStyle::MakeText(WidgetTree, TEXT("NotoSans"), 13, DHScreenStyle::Text);
	Explain->SetText(NSLOCTEXT("DowntownHunting", "LoadoutsExplain", "Five sets to switch between. The pink one\nis what you take into the next fight."));
	Left->AddChildToVerticalBox(Explain)->SetPadding(FMargin(0, 0, 0, 12));
	AddSlots(Left);
	RenameButton = MakeTextButton(TEXT("Rename"), NSLOCTEXT("DowntownHunting", "RenameButton", "RENAME"), 16);
	Left->AddChildToVerticalBox(RenameButton)->SetPadding(FMargin(6, 10, 0, 0));
	Cast<UVerticalBoxSlot>(RenameButton->Slot)->SetHorizontalAlignment(HAlign_Left);

	// Right: the loadout in use, its rows, the details panel.
	UVerticalBox* Right = WidgetTree->ConstructWidget<UVerticalBox>();
	Main->AddChildToHorizontalBox(Right);
	HeaderText = DHScreenStyle::MakeText(WidgetTree, TEXT("Orbitron"), 30, DHScreenStyle::Text);
	Right->AddChildToVerticalBox(HeaderText);
	SummaryText = DHScreenStyle::MakeText(WidgetTree, TEXT("NotoSans"), 14, DHScreenStyle::Text);
	Right->AddChildToVerticalBox(SummaryText)->SetPadding(FMargin(0, 2, 0, 10));
	AddWeaponRow(Right, NSLOCTEXT("DowntownHunting", "Primary", "PRIMARY"), FSettings::Primary);
	AddAttachmentRow(Right, FSettings::Primary);
	AddWeaponRow(Right, NSLOCTEXT("DowntownHunting", "Secondary", "SECONDARY"), FSettings::Secondary);
	AddAttachmentRow(Right, FSettings::Secondary);
	AddAbilityRow(Right, NSLOCTEXT("DowntownHunting", "AbilityRow", "ABILITY"), FSettings::Ability);
	AddPerkRow(Right);
	AddAbilityRow(Right, NSLOCTEXT("DowntownHunting", "Ability2Row", "2ND ABILITY"), FSettings::Ability2);
	AddStreakRow(Right);
	Right->AddChildToVerticalBox(MakeDetails())->SetPadding(FMargin(0, 12, 0, 0));

	PromptText = DHScreenStyle::MakeText(WidgetTree, TEXT("Orbitron"), 14, DHScreenStyle::Accent);
	UOverlaySlot* PromptSlot = Root->AddChildToOverlay(PromptText);
	PromptSlot->SetHorizontalAlignment(HAlign_Center);
	PromptSlot->SetVerticalAlignment(VAlign_Bottom);
	PromptSlot->SetPadding(FMargin(0, 0, 0, 22));

	RenameBox = MakeRenameBox();
	UOverlaySlot* RenameSlot = Root->AddChildToOverlay(RenameBox);
	RenameSlot->SetHorizontalAlignment(HAlign_Center);
	RenameSlot->SetVerticalAlignment(VAlign_Center);
	RenameBox->SetVisibility(ESlateVisibility::Collapsed);
	UpdatePrompts();
}

UDHCardButton* UDHLoadoutScreen::MakeTextButton(const FString& Id, const FText& Label, int32 Size)
{
	UDHCardButton* Button = WidgetTree->ConstructWidget<UDHCardButton>();
	Button->SetBackgroundColor(DHScreenStyle::Card);
	Button->CardSlot = Cards.Num();
	Button->OnCardClicked.BindUObject(this, &ThisClass::HandleCardClicked);
	UTextBlock* Text = DHScreenStyle::MakeText(WidgetTree, TEXT("Orbitron"), Size, DHScreenStyle::Text);
	Text->SetText(Label);
	Button->AddChild(Text);
	Cards.Add({ Button, EKind::Button, 0, Id });
	KeepAlive.Add(Button);
	return Button;
}

void UDHLoadoutScreen::AddSlots(UVerticalBox* Column)
{
	for (int32 Index = 0; Index < FSettings::NumLoadouts; ++Index)
	{
		UHorizontalBox* Content = WidgetTree->ConstructWidget<UHorizontalBox>();
		UTextBlock* Number = DHScreenStyle::MakeText(WidgetTree, TEXT("Orbitron"), 40, DHScreenStyle::Accent);
		Number->SetText(FText::AsNumber(Index + 1));
		Content->AddChildToHorizontalBox(Number)->SetVerticalAlignment(VAlign_Center);
		UVerticalBox* Texts = WidgetTree->ConstructWidget<UVerticalBox>();
		UTextBlock* Name = DHScreenStyle::MakeText(WidgetTree, TEXT("Orbitron"), 17, DHScreenStyle::Text);
		UTextBlock* Detail = DHScreenStyle::MakeText(WidgetTree, TEXT("NotoSans"), 12, DHScreenStyle::Text);
		Texts->AddChildToVerticalBox(Name);
		Texts->AddChildToVerticalBox(Detail);
		Content->AddChildToHorizontalBox(Texts)->SetPadding(FMargin(14, 0, 0, 0));
		Cast<UHorizontalBoxSlot>(Texts->Slot)->SetVerticalAlignment(VAlign_Center);
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>();
		Box->SetWidthOverride(300.f);
		Box->SetHeightOverride(78.f);
		Box->AddChild(Content);
		UDHCardButton* Button = WidgetTree->ConstructWidget<UDHCardButton>();
		Button->SetBackgroundColor(DHScreenStyle::Card);
		Button->CardSlot = Cards.Num();
		Button->PopScale = 0.04f;
		Button->OnCardClicked.BindUObject(this, &ThisClass::HandleCardClicked);
		Button->AddChild(Box);
		Column->AddChildToVerticalBox(Button)->SetPadding(FMargin(0, 4));
		FCard Card{ Button, EKind::Slot, Index };
		Card.Title = Name;
		Card.Detail = Detail;
		Cards.Add(Card);
		KeepAlive.Add(Button);
	}
}

UHorizontalBox* UDHLoadoutScreen::AddRow(UVerticalBox* Column, const FText& Title)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	Column->AddChildToVerticalBox(Row)->SetPadding(FMargin(0, 3));
	USizeBox* LabelBox = WidgetTree->ConstructWidget<USizeBox>();
	LabelBox->SetWidthOverride(LabelWidth);
	UTextBlock* Label = DHScreenStyle::MakeText(WidgetTree, TEXT("Orbitron"), 15, Title.IsEmpty() ? DHScreenStyle::Text : DHScreenStyle::Accent);
	Label->SetText(Title.IsEmpty() ? NSLOCTEXT("DowntownHunting", "AttachmentRow", "  + attachment") : Title);
	Label->SetRenderOpacity(Title.IsEmpty() ? 0.6f : 1.f);
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
	Row->AddChildToHorizontalBox(Button)->SetPadding(FMargin(5, 0));
	FCard Card{ Button, Kind, Entry, Id };
	Card.bLocked = bLocked;
	Cards.Add(Card);
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
		IconBox->SetWidthOverride(124.f);
		IconBox->SetHeightOverride(58.f);
		UImage* Icon = WidgetTree->ConstructWidget<UImage>();
		if (UTexture2D* Texture = Cast<UTexture2D>(Weapon.Icon.TryLoad()))
		{
			Icon->SetBrushFromTexture(Texture);
		}
		IconBox->AddChild(Icon);
		Content->AddChildToVerticalBox(IconBox);
		const bool bLocked = !FDHProgression::IsUnlocked(Weapon.Id);
		UTextBlock* Name = DHScreenStyle::MakeText(WidgetTree, TEXT("Orbitron"), 12, DHScreenStyle::Text);
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
		Box->SetWidthOverride(150.f);
		const bool bLocked = !FDHProgression::IsUnlocked(Attachment.Id);
		UTextBlock* Name = DHScreenStyle::MakeText(WidgetTree, TEXT("Orbitron"), 12, DHScreenStyle::Text);
		Name->SetText(Attachment.Name);
		Name->SetJustification(ETextJustify::Center);
		Box->AddChild(Name);
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
		UHorizontalBox* Content = WidgetTree->ConstructWidget<UHorizontalBox>();
		if (UTexture2D* Texture = AbilityIcon(Ability.Id))
		{
			USizeBox* IconBox = WidgetTree->ConstructWidget<USizeBox>();
			IconBox->SetWidthOverride(40.f);
			IconBox->SetHeightOverride(40.f);
			UImage* Icon = WidgetTree->ConstructWidget<UImage>();
			Icon->SetBrushFromTexture(Texture);
			IconBox->AddChild(Icon);
			Content->AddChildToHorizontalBox(IconBox)->SetVerticalAlignment(VAlign_Center);
		}
		// The second slot unlocks late; until then its cards say so.
		const FString LockId = bSecond && !FDHProgression::IsUnlocked(SecondAbilityId) ? FString(SecondAbilityId) : Ability.Id;
		const bool bLocked = !FDHProgression::IsUnlocked(LockId);
		UVerticalBox* Texts = WidgetTree->ConstructWidget<UVerticalBox>();
		UTextBlock* Name = DHScreenStyle::MakeText(WidgetTree, TEXT("Orbitron"), 12, DHScreenStyle::Text);
		Name->SetText(Ability.Name);
		Texts->AddChildToVerticalBox(Name);
		if (Ability.Cooldown > 0.f || bLocked)
		{
			UTextBlock* Info = DHScreenStyle::MakeText(WidgetTree, TEXT("NotoSans"), 11, DHScreenStyle::Text);
			Info->SetText(bLocked ? FText::Format(NSLOCTEXT("DowntownHunting", "LockedLevel", "LVL {0}"), FDHProgression::UnlockLevel(LockId))
				: FText::Format(NSLOCTEXT("DowntownHunting", "AbilityCardRecharge", "{0} s recharge"), Ability.Cooldown));
			Info->SetRenderOpacity(0.75f);
			Texts->AddChildToVerticalBox(Info);
		}
		Content->AddChildToHorizontalBox(Texts)->SetPadding(FMargin(8, 0, 0, 0));
		Cast<UHorizontalBoxSlot>(Texts->Slot)->SetVerticalAlignment(VAlign_Center);
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>();
		Box->SetWidthOverride(Ability.Id.IsEmpty() ? 90.f : 170.f);
		Box->SetHeightOverride(46.f);
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
		Box->SetWidthOverride(190.f);
		const bool bLocked = !FDHProgression::IsUnlocked(Perk.Id);
		UTextBlock* Name = DHScreenStyle::MakeText(WidgetTree, TEXT("Orbitron"), 12, DHScreenStyle::Text);
		Name->SetText(bLocked ? FText::Format(NSLOCTEXT("DowntownHunting", "LockedPerk", "{0}  ·  LVL {1}"), Perk.Name, FDHProgression::UnlockLevel(Perk.Id)) : Perk.Name);
		Name->SetJustification(ETextJustify::Center);
		Box->AddChild(Name);
		AddCard(Row, EKind::Perk, FSettings::Perk, Perk.Id, Box, bLocked);
	}
}

void UDHLoadoutScreen::AddStreakRow(UVerticalBox* Column)
{
	UHorizontalBox* Row = AddRow(Column, NSLOCTEXT("DowntownHunting", "StreakRow", "KILLSTREAKS"));
	for (const FDHStreakEntry& Streak : FSettings::GetStreaks())
	{
		UHorizontalBox* Content = WidgetTree->ConstructWidget<UHorizontalBox>();
		UTextBlock* Kills = DHScreenStyle::MakeText(WidgetTree, TEXT("Orbitron"), 24, DHScreenStyle::Accent);
		Kills->SetText(FText::AsNumber(Streak.Kills));
		Content->AddChildToHorizontalBox(Kills)->SetVerticalAlignment(VAlign_Center);
		UVerticalBox* Texts = WidgetTree->ConstructWidget<UVerticalBox>();
		UTextBlock* Name = DHScreenStyle::MakeText(WidgetTree, TEXT("Orbitron"), 12, DHScreenStyle::Text);
		Name->SetText(Streak.Name);
		Texts->AddChildToVerticalBox(Name);
		UTextBlock* Info = DHScreenStyle::MakeText(WidgetTree, TEXT("NotoSans"), 11, DHScreenStyle::Text);
		Info->SetText(NSLOCTEXT("DowntownHunting", "StreakCardKills", "kills"));
		Info->SetRenderOpacity(0.75f);
		Texts->AddChildToVerticalBox(Info);
		Content->AddChildToHorizontalBox(Texts)->SetPadding(FMargin(8, 0, 0, 0));
		Cast<UHorizontalBoxSlot>(Texts->Slot)->SetVerticalAlignment(VAlign_Center);
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>();
		Box->SetWidthOverride(150.f);
		Box->SetHeightOverride(46.f);
		Box->AddChild(Content);
		AddCard(Row, EKind::Streak, FSettings::Streak1, Streak.Id, Box, false);
	}
}

UWidget* UDHLoadoutScreen::MakeDetails()
{
	// Card on the bottom right: picture, kind, name, what it does, and for weapons four stat bars.
	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.55f));
	Panel->SetPadding(FMargin(16, 12));
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	Panel->SetContent(Row);
	USizeBox* IconBox = WidgetTree->ConstructWidget<USizeBox>();
	IconBox->SetWidthOverride(200.f);
	IconBox->SetHeightOverride(110.f);
	DetailIcon = WidgetTree->ConstructWidget<UImage>();
	IconBox->AddChild(DetailIcon);
	Row->AddChildToHorizontalBox(IconBox)->SetVerticalAlignment(VAlign_Center);

	UVerticalBox* Texts = WidgetTree->ConstructWidget<UVerticalBox>();
	Row->AddChildToHorizontalBox(Texts)->SetPadding(FMargin(20, 0, 0, 0));
	DetailKind = DHScreenStyle::MakeText(WidgetTree, TEXT("Orbitron"), 12, DHScreenStyle::Cyan);
	Texts->AddChildToVerticalBox(DetailKind);
	DetailTitle = DHScreenStyle::MakeText(WidgetTree, TEXT("Orbitron"), 28, DHScreenStyle::Accent);
	Texts->AddChildToVerticalBox(DetailTitle);
	DetailText = DHScreenStyle::MakeText(WidgetTree, TEXT("NotoSans"), 15, DHScreenStyle::Text);
	DetailText->SetAutoWrapText(true);
	USizeBox* TextBox = WidgetTree->ConstructWidget<USizeBox>();
	TextBox->SetWidthOverride(420.f);
	TextBox->AddChild(DetailText);
	Texts->AddChildToVerticalBox(TextBox);

	UVerticalBox* Stats = WidgetTree->ConstructWidget<UVerticalBox>();
	Row->AddChildToHorizontalBox(Stats)->SetPadding(FMargin(24, 0, 0, 0));
	Cast<UHorizontalBoxSlot>(Stats->Slot)->SetVerticalAlignment(VAlign_Center);
	const FText Names[] = { NSLOCTEXT("DowntownHunting", "StatDamage", "DAMAGE"), NSLOCTEXT("DowntownHunting", "StatRate", "FIRE RATE"),
		NSLOCTEXT("DowntownHunting", "StatRange", "RANGE"), NSLOCTEXT("DowntownHunting", "StatMobility", "MOBILITY") };
	for (const FText& Name : Names)
	{
		UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>();
		USizeBox* NameBox = WidgetTree->ConstructWidget<USizeBox>();
		NameBox->SetWidthOverride(110.f);
		UTextBlock* Label = DHScreenStyle::MakeText(WidgetTree, TEXT("Orbitron"), 11, DHScreenStyle::Text);
		Label->SetText(Name);
		NameBox->AddChild(Label);
		Line->AddChildToHorizontalBox(NameBox)->SetVerticalAlignment(VAlign_Center);
		USizeBox* BarBox = WidgetTree->ConstructWidget<USizeBox>();
		BarBox->SetWidthOverride(180.f);
		BarBox->SetHeightOverride(8.f);
		UProgressBar* Bar = WidgetTree->ConstructWidget<UProgressBar>();
		Bar->SetFillColorAndOpacity(DHScreenStyle::Accent);
		BarBox->AddChild(Bar);
		Line->AddChildToHorizontalBox(BarBox)->SetVerticalAlignment(VAlign_Center);
		Stats->AddChildToVerticalBox(Line)->SetPadding(FMargin(0, 3));
		StatBars.Add(Bar);
	}
	StatsBox = Stats;
	return Panel;
}

UWidget* UDHLoadoutScreen::MakeRenameBox()
{
	UBorder* Box = WidgetTree->ConstructWidget<UBorder>();
	Box->SetBrushColor(FLinearColor(0.05f, 0.02f, 0.09f, 0.97f));
	Box->SetPadding(FMargin(28, 22));
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	Box->SetContent(Column);
	UTextBlock* Title = DHScreenStyle::MakeText(WidgetTree, TEXT("Orbitron"), 28, DHScreenStyle::Accent);
	Title->SetText(NSLOCTEXT("DowntownHunting", "RenameTitle", "NAME THIS LOADOUT"));
	Column->AddChildToVerticalBox(Title);
	UTextBlock* Hint = DHScreenStyle::MakeText(WidgetTree, TEXT("NotoSans"), 14, DHScreenStyle::Text);
	Hint->SetText(NSLOCTEXT("DowntownHunting", "RenameHint", "Type a name and press Enter, or roll one with the dice."));
	Column->AddChildToVerticalBox(Hint)->SetPadding(FMargin(0, 2, 0, 12));
	RenameText = WidgetTree->ConstructWidget<UEditableTextBox>();
	FEditableTextBoxStyle Style = RenameText->GetWidgetStyle();
	Style.TextStyle.SetFont(DHScreenStyle::Font(TEXT("Orbitron"), 22));
	RenameText->SetWidgetStyle(Style);
	RenameText->SetMinDesiredWidth(440.f);
	RenameText->SetSelectAllTextWhenFocused(true);
	RenameText->OnTextCommitted.AddDynamic(this, &ThisClass::HandleRenameCommitted);
	Column->AddChildToVerticalBox(RenameText);
	UHorizontalBox* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>();
	Column->AddChildToVerticalBox(Buttons)->SetPadding(FMargin(0, 16, 0, 0));
	for (const TPair<FString, FText>& Entry : { TPair<FString, FText>(TEXT("Roll"), NSLOCTEXT("DowntownHunting", "RenameRoll", "ROLL A NAME")),
		TPair<FString, FText>(TEXT("Save"), NSLOCTEXT("DowntownHunting", "RenameSave", "SAVE")),
		TPair<FString, FText>(TEXT("Cancel"), NSLOCTEXT("DowntownHunting", "RenameCancel", "CANCEL")) })
	{
		Buttons->AddChildToHorizontalBox(MakeTextButton(Entry.Key, Entry.Value, 16))->SetPadding(FMargin(0, 0, 14, 0));
	}
	return Box;
}

void UDHLoadoutScreen::NativeOnActivated()
{
	Super::NativeOnActivated();
	Refresh();
}

UWidget* UDHLoadoutScreen::NativeGetDesiredFocusTarget() const
{
	const FCard* Active = Cards.FindByPredicate([this](const FCard& Card) { return Card.Kind == EKind::Slot && Card.Entry == CurrentIndex; });
	return Active ? Active->Button.Get() : nullptr;
}

void UDHLoadoutScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Focus (gamepad, keyboard) wins over hover for the details.
	int32 Focused = Cards.IndexOfByPredicate([](const FCard& Card) { return Card.Button->HasAnyUserFocus(); });
	if (Focused == INDEX_NONE)
	{
		Focused = Cards.IndexOfByPredicate([](const FCard& Card) { return Card.Button->IsHovered(); });
	}
	for (int32 Index = 0; Index < Cards.Num(); ++Index)
	{
		const FCard& Card = Cards[Index];
		Card.Button->SetBackgroundColor(IsChosen(Card) ? DHScreenStyle::Selected : Index == Focused ? DHScreenStyle::Focus : DHScreenStyle::Card);
		Card.Button->Animate(InDeltaTime);
	}
	if (Focused != INDEX_NONE && Focused != ShownDetails && Cards[Focused].Kind != EKind::Button)
	{
		ShownDetails = Focused;
		ShowDetails(&Cards[Focused]);
	}
	UpdatePrompts();
}

FReply UDHLoadoutScreen::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (RenameBox->GetVisibility() == ESlateVisibility::Collapsed)
	{
		const FKey Key = InKeyEvent.GetKey();
		if (Key == EKeys::Gamepad_FaceButton_Top || Key == EKeys::R)
		{
			OpenRename();
			return FReply::Handled();
		}
		if (Key == EKeys::Gamepad_LeftShoulder || Key == EKeys::Q)
		{
			SwitchLoadout(-1);
			return FReply::Handled();
		}
		if (Key == EKeys::Gamepad_RightShoulder || Key == EKeys::E)
		{
			SwitchLoadout(1);
			return FReply::Handled();
		}
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

bool UDHLoadoutScreen::NativeOnHandleBackAction()
{
	if (RenameBox->GetVisibility() != ESlateVisibility::Collapsed)
	{
		CloseRename(false);
		return true;
	}
	return Super::NativeOnHandleBackAction();
}

bool UDHLoadoutScreen::IsChosen(const FCard& Card) const
{
	const FString& Entry = CurrentLoadout.IsValidIndex(Card.Entry) ? CurrentLoadout[Card.Entry] : FString();
	switch (Card.Kind)
	{
	case EKind::Slot: return Card.Entry == CurrentIndex;
	case EKind::Button: return false;
	case EKind::Weapon: return WeaponOf(Entry) == Card.Id;
	case EKind::Attachment: return AttachmentOf(Entry) == Card.Id;
	case EKind::Streak:
		for (int32 Index = FSettings::Streak1; Index <= FSettings::Streak3; ++Index)
		{
			if (CurrentLoadout.IsValidIndex(Index) && CurrentLoadout[Index] == Card.Id)
			{
				return true;
			}
		}
		return false;
	default: return Entry == Card.Id;
	}
}

void UDHLoadoutScreen::HandleCardClicked(UDHCardButton* Button)
{
	if (!Cards.IsValidIndex(Button->CardSlot))
	{
		return;
	}
	const FCard& Card = Cards[Button->CardSlot];
	if (Card.Kind != EKind::Button)
	{
		Pick(Card);
	}
	else if (Card.Id == TEXT("Rename"))
	{
		OpenRename();
	}
	else if (Card.Id == TEXT("Roll"))
	{
		constexpr int32 Count = UE_ARRAY_COUNT(RandomNames);
		RandomName = (RandomName + 1 + FMath::RandRange(0, Count - 2)) % Count;  // never the same twice in a row
		RenameText->SetText(FText::FromString(RandomNames[RandomName]));
	}
	else
	{
		CloseRename(Card.Id == TEXT("Save"));
	}
}

void UDHLoadoutScreen::HandleRenameCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter)
	{
		CloseRename(true);
	}
}

void UDHLoadoutScreen::SwitchLoadout(int32 Delta)
{
	FSettings::SetActiveLoadout(GetOwningPlayer(), (CurrentIndex + Delta + FSettings::NumLoadouts) % FSettings::NumLoadouts);
	UDHCardButton::PlayMenuSound(this, true);
	Refresh();
}

void UDHLoadoutScreen::OpenRename()
{
	RenameText->SetText(FText::FromString(FSettings::GetLoadoutName(CurrentIndex)));
	RenameBox->SetVisibility(ESlateVisibility::Visible);
	const bool bGamepad = GetOwningLocalPlayer() && UCommonInputSubsystem::Get(GetOwningLocalPlayer())
		&& UCommonInputSubsystem::Get(GetOwningLocalPlayer())->GetCurrentInputType() == ECommonInputType::Gamepad;
	// Typing goes straight into the box; on the gamepad the dice come first.
	const FCard* Roll = Cards.FindByPredicate([](const FCard& Card) { return Card.Kind == EKind::Button && Card.Id == TEXT("Roll"); });
	if (bGamepad && Roll)
	{
		Roll->Button->SetFocus();
	}
	else
	{
		RenameText->SetKeyboardFocus();
	}
}

void UDHLoadoutScreen::CloseRename(bool bSave)
{
	if (bSave)
	{
		FSettings::SetLoadoutName(CurrentIndex, RenameText->GetText().ToString());
	}
	RenameBox->SetVisibility(ESlateVisibility::Collapsed);
	Refresh();
	RenameButton->SetFocus();
}

void UDHLoadoutScreen::Pick(const FCard& Card)
{
	if (Card.Kind == EKind::Slot)
	{
		FSettings::SetActiveLoadout(GetOwningPlayer(), Card.Entry);
		Refresh();
		return;
	}
	if (Card.bLocked || !FDHProgression::IsUnlocked(Card.Id) || (Card.Entry == FSettings::Ability2 && !FDHProgression::IsUnlocked(SecondAbilityId)))
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
	case EKind::Streak:
	{
		// Three at most, kept in the order picked: a chosen one goes, a fourth pushes out the oldest.
		TArray<FString> Streaks;
		for (int32 Index = FSettings::Streak1; Index <= FSettings::Streak3; ++Index)
		{
			if (!Loadout[Index].IsEmpty())
			{
				Streaks.Add(Loadout[Index]);
			}
		}
		if (Streaks.Remove(Card.Id) == 0)
		{
			Streaks.Add(Card.Id);
			if (Streaks.Num() > 3)
			{
				Streaks.RemoveAt(0);
			}
		}
		else if (Streaks.IsEmpty())
		{
			return;  // at least one
		}
		for (int32 Index = 0; Index < 3; ++Index)
		{
			Loadout[FSettings::Streak1 + Index] = Streaks.IsValidIndex(Index) ? Streaks[Index] : FString();
		}
		break;
	}
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

	auto AbilityNames = [](const TArray<FString>& Loadout)
	{
		FString Names;
		for (const int32 Entry : { int32(FSettings::Ability), int32(FSettings::Ability2) })
		{
			if (const FDHAbilityEntry* Ability = FSettings::FindAbility(Loadout[Entry]))
			{
				Names += (Names.IsEmpty() ? TEXT("") : TEXT(" · ")) + Ability->Name.ToString();
			}
		}
		return Names;
	};
	// Slots: name, weapons, abilities.
	for (const FCard& Card : Cards)
	{
		if (Card.Kind == EKind::Slot)
		{
			const TArray<FString> Loadout = FSettings::GetLoadoutAt(Card.Entry);
			Card.Title->SetText(FText::FromString(FSettings::GetLoadoutName(Card.Entry)));
			Card.Detail->SetText(FText::FromString(WeaponOf(Loadout[FSettings::Primary]).ToUpper() + TEXT(" + ")
				+ WeaponOf(Loadout[FSettings::Secondary]).ToUpper() + TEXT("\n") + AbilityNames(Loadout)));
		}
	}
	HeaderText->SetText(FText::FromString(FSettings::GetLoadoutName(CurrentIndex)));
	SummaryText->SetText(NSLOCTEXT("DowntownHunting", "LoadoutSummary2",
		"In use. Weapons change from your next spawn (in modes without a fixed loadout), abilities right away."));
	ShownDetails = INDEX_NONE;
}

void UDHLoadoutScreen::ShowDetails(const FCard* Card)
{
	FText Kind, Title, Text;
	UTexture2D* Icon = nullptr;
	const FWeaponInfo* Weapon = nullptr;
	switch (Card->Kind)
	{
	case EKind::Slot:
	{
		const TArray<FString> Loadout = FSettings::GetLoadoutAt(Card->Entry);
		Kind = FText::Format(NSLOCTEXT("DowntownHunting", "DetailSlot", "LOADOUT {0}"), Card->Entry + 1);
		Title = FText::FromString(FSettings::GetLoadoutName(Card->Entry));
		Text = Card->Entry == CurrentIndex ? NSLOCTEXT("DowntownHunting", "DetailSlotActive", "This one is in use. Change its cards on the right.")
			: NSLOCTEXT("DowntownHunting", "DetailSlotOther", "Pick it to use it (and edit it).");
		Icon = WeaponIcon(WeaponOf(Loadout[FSettings::Primary]));
		break;
	}
	case EKind::Weapon:
		Weapon = FindWeaponInfo(Card->Id);
		Kind = Weapon ? Weapon->Kind : FText::GetEmpty();
		Title = FText::FromString(Card->Id.ToUpper());
		Text = Weapon ? Weapon->Text : FText::GetEmpty();
		Icon = WeaponIcon(Card->Id);
		break;
	case EKind::Attachment:
		for (const FDHAttachmentEntry& Attachment : FSettings::GetAttachments())
		{
			if (Attachment.Id == Card->Id)
			{
				Kind = NSLOCTEXT("DowntownHunting", "DetailAttachment", "ATTACHMENT");
				Title = Attachment.Name;
				Text = Attachment.Description;
			}
		}
		break;
	case EKind::Ability:
		if (const FDHAbilityEntry* Ability = FSettings::FindAbility(Card->Id))
		{
			Kind = Card->Entry == FSettings::Ability2 ? NSLOCTEXT("DowntownHunting", "DetailAbility2", "2ND ABILITY") : NSLOCTEXT("DowntownHunting", "DetailAbility", "ABILITY");
			Title = Ability->Name;
			Text = Ability->Duration > 0.f
				? FText::Format(NSLOCTEXT("DowntownHunting", "DetailAbilityTimes", "{0}.\nLasts {1} s, recharges in {2} s."), Ability->Description, Ability->Duration, Ability->Cooldown)
				: FText::Format(NSLOCTEXT("DowntownHunting", "DetailAbilityRecharge", "{0}.\nRecharges in {1} s."), Ability->Description, Ability->Cooldown);
			Icon = AbilityIcon(Card->Id);
		}
		else
		{
			Kind = NSLOCTEXT("DowntownHunting", "DetailAbility2", "2ND ABILITY");
			Title = NSLOCTEXT("DowntownHunting", "AbilityNone", "NONE");
			Text = NSLOCTEXT("DowntownHunting", "DetailNoAbility", "Only one ability.");
		}
		break;
	case EKind::Streak:
		if (const FDHStreakEntry* Streak = FSettings::FindStreak(Card->Id))
		{
			Kind = FText::Format(NSLOCTEXT("DowntownHunting", "DetailStreak", "KILLSTREAK  ·  {0} KILLS"), Streak->Kills);
			Title = Streak->Name;
			Text = FText::Format(NSLOCTEXT("DowntownHunting", "DetailStreakText", "{0}.\nReady after {1} kills in a row; you call it in yourself (4 / 5 / 6, D-pad left). Take three."),
				Streak->Description, Streak->Kills);
		}
		break;
	case EKind::Perk:
		for (const FDHAbilityEntry& Perk : FSettings::GetPerks())
		{
			if (Perk.Id == Card->Id)
			{
				Kind = NSLOCTEXT("DowntownHunting", "DetailPerk", "PERK");
				Title = Perk.Name;
				Text = Perk.Id.IsEmpty() ? NSLOCTEXT("DowntownHunting", "DetailNoPerk", "No perk.") : Perk.Description;
			}
		}
		break;
	default:
		break;
	}
	if (Card->bLocked)
	{
		const FString LockId = Card->Kind == EKind::Ability && Card->Entry == FSettings::Ability2 && !FDHProgression::IsUnlocked(SecondAbilityId) ? FString(SecondAbilityId) : Card->Id;
		Text = FText::Format(NSLOCTEXT("DowntownHunting", "DetailLocked", "{0}\nLOCKED: {1}."), Text, LockText(LockId));
	}
	DetailKind->SetText(Kind);
	DetailTitle->SetText(Title);
	DetailText->SetText(Text);
	DetailIcon->SetBrushFromTexture(Icon);
	DetailIcon->SetColorAndOpacity(Icon ? FLinearColor::White : FLinearColor::Transparent);
	StatsBox->SetVisibility(Weapon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	for (int32 Index = 0; Weapon && Index < StatBars.Num(); ++Index)
	{
		StatBars[Index]->SetPercent(Weapon->Stats[Index]);
	}
}

void UDHLoadoutScreen::UpdatePrompts()
{
	const ULocalPlayer* Player = GetOwningLocalPlayer();
	const UCommonInputSubsystem* Input = Player ? UCommonInputSubsystem::Get(Player) : nullptr;
	const bool bGamepad = Input && Input->GetCurrentInputType() == ECommonInputType::Gamepad;
	if (bGamepad == bPromptsForGamepad && !PromptText->GetText().IsEmpty())
	{
		return;
	}
	bPromptsForGamepad = bGamepad;
	PromptText->SetText(bGamepad
		? NSLOCTEXT("DowntownHunting", "LoadoutPromptsPad", "A  CHOOSE      LB / RB  SWITCH LOADOUT      Y  RENAME      B  BACK")
		: NSLOCTEXT("DowntownHunting", "LoadoutPromptsKeys", "CLICK / ENTER  CHOOSE      Q / E  SWITCH LOADOUT      R  RENAME      ESC  BACK"));
}
