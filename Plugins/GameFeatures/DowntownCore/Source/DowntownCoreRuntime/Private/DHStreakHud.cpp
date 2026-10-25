#include "DHStreakHud.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "DHControllerScreen.h"
#include "DHKillstreaks.h"
#include "DHLoadoutSettings.h"
#include "DHPlayerChoicesComponent.h"
#include "DHScreenStyle.h"
#include "DHFreeForAllScoring.h"
#include "DHStrikeMap.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Player/LyraPlayerState.h"
#include "PrimaryGameLayout.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "Sound/SoundBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DHStreakHud)

namespace
{
	const TCHAR* SlotActions[3] = {
		TEXT("/DowntownCore/Input/IA_DH_Streak1.IA_DH_Streak1"),
		TEXT("/DowntownCore/Input/IA_DH_Streak2.IA_DH_Streak2"),
		TEXT("/DowntownCore/Input/IA_DH_Streak3.IA_DH_Streak3"),
	};
	const TCHAR* UseAction = TEXT("/DowntownCore/Input/IA_DH_StreakUse.IA_DH_StreakUse");

	UTextBlock* Outlined(UWidgetTree* Tree, const TCHAR* Font, int32 Size, const FLinearColor& Colour)
	{
		UTextBlock* Text = DHScreenStyle::MakeText(Tree, Font, Size, Colour);
		FSlateFontInfo Info = Text->GetFont();
		Info.OutlineSettings.OutlineSize = 2;
		Info.OutlineSettings.OutlineColor = FLinearColor::Black;
		Text->SetFont(Info);
		return Text;
	}

	void PlayUiSound(const UObject* Context, const TCHAR* Path, float Volume, float Pitch)
	{
		if (USoundBase* Sound = LoadObject<USoundBase>(nullptr, Path))
		{
			UGameplayStatics::PlaySound2D(Context, Sound, Volume, Pitch);
		}
	}
}

void UDHStreakHud::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::HitTestInvisible);

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
	WidgetTree->RootWidget = Root;
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	UOverlaySlot* ColumnSlot = Root->AddChildToOverlay(Column);
	ColumnSlot->SetHorizontalAlignment(HAlign_Right);
	ColumnSlot->SetVerticalAlignment(VAlign_Center);
	ColumnSlot->SetPadding(FMargin(0, 120, 24, 0));

	CountdownText = Outlined(WidgetTree, TEXT("Orbitron"), 120, DHScreenStyle::Accent);
	CountdownText->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	UOverlaySlot* CountdownSlot = Root->AddChildToOverlay(CountdownText);
	CountdownSlot->SetHorizontalAlignment(HAlign_Center);
	CountdownSlot->SetVerticalAlignment(VAlign_Center);
	CountdownSlot->SetPadding(FMargin(0, 0, 0, 160));

	StreakText = Outlined(WidgetTree, TEXT("Orbitron"), 14, DHScreenStyle::Accent);
	Column->AddChildToVerticalBox(StreakText)->SetHorizontalAlignment(HAlign_Right);
	Cast<UVerticalBoxSlot>(StreakText->Slot)->SetPadding(FMargin(0, 0, 4, 6));

	for (FTile& Tile : Tiles)
	{
		// [key badge] [frame: kills | name / state]
		UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>();
		Column->AddChildToVerticalBox(Line)->SetHorizontalAlignment(HAlign_Right);
		Cast<UVerticalBoxSlot>(Line->Slot)->SetPadding(FMargin(0, 4));

		Tile.KeyBadge = WidgetTree->ConstructWidget<UBorder>();
		Tile.KeyBadge->SetBrushColor(DHScreenStyle::Accent);
		Tile.KeyBadge->SetPadding(FMargin(7, 1));
		Tile.Key = DHScreenStyle::MakeText(WidgetTree, TEXT("Orbitron"), 13, FLinearColor(0.02f, 0.01f, 0.03f));
		Tile.KeyBadge->SetContent(Tile.Key);
		Tile.KeyBadge->SetRenderTransformAngle(-4.f);
		UHorizontalBoxSlot* BadgeSlot = Line->AddChildToHorizontalBox(Tile.KeyBadge);
		BadgeSlot->SetVerticalAlignment(VAlign_Center);
		BadgeSlot->SetPadding(FMargin(0, 0, 8, 0));

		Tile.Frame = WidgetTree->ConstructWidget<UBorder>();
		Tile.Frame->SetPadding(FMargin(3));
		Tile.Frame->SetRenderTransformPivot(FVector2D(1.f, 0.5f));
		Line->AddChildToHorizontalBox(Tile.Frame);
		UBorder* Card = WidgetTree->ConstructWidget<UBorder>();
		Card->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.8f));
		Card->SetPadding(FMargin(10, 4));
		Tile.Frame->SetContent(Card);
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
		Size->SetWidthOverride(190.f);
		Size->SetHeightOverride(44.f);
		Card->SetContent(Size);
		UHorizontalBox* Content = WidgetTree->ConstructWidget<UHorizontalBox>();
		Size->AddChild(Content);
		Tile.Kills = Outlined(WidgetTree, TEXT("Orbitron"), 26, DHScreenStyle::Accent);
		Content->AddChildToHorizontalBox(Tile.Kills)->SetVerticalAlignment(VAlign_Center);
		UVerticalBox* Texts = WidgetTree->ConstructWidget<UVerticalBox>();
		UHorizontalBoxSlot* TextsSlot = Content->AddChildToHorizontalBox(Texts);
		TextsSlot->SetVerticalAlignment(VAlign_Center);
		TextsSlot->SetPadding(FMargin(10, 0, 0, 0));
		Tile.Name = Outlined(WidgetTree, TEXT("Orbitron"), 13, DHScreenStyle::Text);
		Texts->AddChildToVerticalBox(Tile.Name);
		Tile.State = Outlined(WidgetTree, TEXT("NotoSans"), 11, DHScreenStyle::Text);
		Texts->AddChildToVerticalBox(Tile.State);
	}
}

bool UDHStreakHud::IsMenuOpen(const APlayerController* Player)
{
	UPrimaryGameLayout* Layout = Player ? UPrimaryGameLayout::GetPrimaryGameLayout(const_cast<APlayerController*>(Player)) : nullptr;
	if (!Layout)
	{
		return false;
	}
	for (const TCHAR* Layer : { TEXT("UI.Layer.GameMenu"), TEXT("UI.Layer.Menu"), TEXT("UI.Layer.Modal") })
	{
		const UCommonActivatableWidgetContainerBase* Stack = Layout->GetLayerWidget(FGameplayTag::RequestGameplayTag(Layer));
		if (Stack && Stack->GetActiveWidget())
		{
			return true;
		}
	}
	return false;
}

TArray<FString> UDHStreakHud::CurrentStreaks() const
{
	const APlayerController* PC = GetOwningPlayer();
	return DHKillstreaks::ChosenStreaks(PC ? PC->PlayerState : nullptr);
}

void UDHStreakHud::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const APlayerController* PC = GetOwningPlayer();
	const ALyraPlayerState* Player = PC ? Cast<ALyraPlayerState>(PC->PlayerState) : nullptr;
	SetRenderOpacity(IsMenuOpen(PC) ? 0.f : 1.f);
	if (!Player)
	{
		return;
	}

	// 3, 2, 1, GO: each number pops in with a beep, GO stays a moment.
	const UDHFreeForAllScoring* Scoring = UDHFreeForAllScoring::Find(this);
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	const double Left = Scoring && GameState ? Scoring->GetGoTime() - GameState->GetServerWorldTimeSeconds() : -10.0;
	const int32 Count = Left > 0.0 ? FMath::CeilToInt(Left) : (Left > -0.9 ? 0 : INDEX_NONE);
	if (Count != ShownCount)
	{
		ShownCount = Count;
		CountdownPop = 1.f;
		CountdownText->SetText(Count > 0 ? FText::AsNumber(Count) : NSLOCTEXT("DowntownHunting", "CountdownGo", "GO!"));
		if (Count != INDEX_NONE)
		{
			PlayUiSound(this, TEXT("/Game/Audio/Sounds/UI/sfx_UI_MainMenu_Select_nl_meta_Preset.sfx_UI_MainMenu_Select_nl_meta_Preset"), 1.f, Count > 0 ? 0.7f : 1.4f);
		}
	}
	CountdownPop = FMath::Max(0.f, CountdownPop - InDeltaTime * 3.f);
	CountdownText->SetVisibility(Count == INDEX_NONE ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	CountdownText->SetRenderScale(FVector2D(1.f + 0.6f * CountdownPop * CountdownPop));
	CountdownText->SetColorAndOpacity(Count > 0 ? DHScreenStyle::Accent : DHScreenStyle::Cyan);
	const TArray<FString> Streaks = CurrentStreaks();
	const int32 Streak = Player->GetStatTagStackCount(FGameplayTag::RequestGameplayTag(TEXT("DH.Streak")));
	StreakText->SetText(FText::Format(NSLOCTEXT("DowntownHunting", "StreakHudCount", "STREAK  {0}"), Streak));

	const double Now = GetWorld()->GetTimeSeconds();
	for (int32 Index = 0; Index < 3; ++Index)
	{
		FTile& Tile = Tiles[Index];
		const FDHStreakEntry* Entry = Streaks.IsValidIndex(Index) ? UDHLoadoutSettings::FindStreak(Streaks[Index]) : nullptr;
		UWidget* Line = Tile.Frame->GetParent();
		Line->SetVisibility(Entry ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (!Entry)
		{
			continue;
		}
		const int32 Ready = DHKillstreaks::ReadyCount(Player, Entry->Id);
		if (Tile.LastReady != INDEX_NONE && Ready > Tile.LastReady)
		{
			Selected = Index;  // just earned: D-pad left calls it in
			Tile.PopVelocity += 10.f;
			PlayUiSound(this, TEXT("/Game/Audio/Sounds/UI/sfx_UI_MainMenu_Select_nl_meta_Preset.sfx_UI_MainMenu_Select_nl_meta_Preset"), 0.7f, 0.8f);
		}
		Tile.LastReady = Ready;
		Tile.Kills->SetText(FText::AsNumber(Entry->Kills));
		Tile.Name->SetText(Entry->Name);
		const float Pulse = 0.5f + 0.5f * FMath::Sin(Now * 8.0);
		if (Ready > 0)
		{
			Tile.State->SetText(Ready > 1 ? FText::Format(NSLOCTEXT("DowntownHunting", "StreakHudReadyMany", "READY  x{0}"), Ready)
				: NSLOCTEXT("DowntownHunting", "StreakHudReady", "READY"));
			Tile.State->SetColorAndOpacity(DHScreenStyle::Accent);
			Tile.Frame->SetBrushColor(FMath::Lerp(DHScreenStyle::Accent, FLinearColor::White, Pulse));
		}
		else
		{
			Tile.State->SetText(FText::Format(NSLOCTEXT("DowntownHunting", "StreakHudProgress", "{0} / {1} kills"), FMath::Min(Streak, Entry->Kills), Entry->Kills));
			Tile.State->SetColorAndOpacity(DHScreenStyle::Text);
			Tile.Frame->SetBrushColor(FLinearColor(0.25f, 0.2f, 0.32f, 0.9f));
		}
		Tile.Name->SetColorAndOpacity(Ready > 0 ? DHScreenStyle::Accent : DHScreenStyle::Text);
		// Keyboard: its own key; gamepad: only the selected one, on D-pad left.
		FText Key = UDHControllerScreen::ActionKeyLabel(PC->GetLocalPlayer(), SlotActions[Index], /*bShort=*/ true);
		if (Key.IsEmpty() && Index == Selected)
		{
			Key = UDHControllerScreen::ActionKeyLabel(PC->GetLocalPlayer(), UseAction, /*bShort=*/ true);
		}
		Tile.Key->SetText(Key);
		Tile.KeyBadge->SetVisibility(Key.IsEmpty() ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible);
		Tile.KeyBadge->SetBrushColor(Ready > 0 ? DHScreenStyle::Accent : FLinearColor(0.5f, 0.45f, 0.55f));

		// Ready pop, an underdamped spring like the ability tiles.
		const float Step = FMath::Min(InDeltaTime, 1.f / 30.f);
		Tile.PopVelocity += -Tile.Pop * 300.f * Step - Tile.PopVelocity * 12.f * Step;
		Tile.Pop += Tile.PopVelocity * Step;
		FWidgetTransform Transform;
		Transform.Shear = FVector2D(-8.f, 0.f);
		Transform.Scale = FVector2D(1.f + 0.1f * Tile.Pop);
		Tile.Frame->SetRenderTransform(Transform);
	}
}

void UDHStreakHud::Activate(int32 Which)
{
	APlayerController* PC = GetOwningPlayer();
	const TArray<FString> Streaks = CurrentStreaks();
	const int32 Index = Which == INDEX_NONE ? Selected : Which;
	UDHPlayerChoicesComponent* Choices = PC ? PC->FindComponentByClass<UDHPlayerChoicesComponent>() : nullptr;
	if (!Choices || !Streaks.IsValidIndex(Index))
	{
		return;
	}
	const FString& Id = Streaks[Index];
	if (DHKillstreaks::ReadyCount(PC->PlayerState, Id) <= 0 || !PC->GetPawn())
	{
		Tiles[Index].PopVelocity -= 6.f;  // not yet: a shake, a dull click
		PlayUiSound(this, TEXT("/Game/Audio/Sounds/UI/sfx_UI_MainMenu_Select_nl_meta_Preset.sfx_UI_MainMenu_Select_nl_meta_Preset"), 0.4f, 0.5f);
		return;
	}
	if (DHKillstreaks::IsStrike(Id))
	{
		UDHStrikeMap::Open(PC, Id);
	}
	else
	{
		Choices->ServerUseStreak(Id, FVector::ZeroVector);
	}
}

void UDHStreakHud::SelectNext()
{
	const int32 Count = CurrentStreaks().Num();
	if (Count > 0)
	{
		Selected = (Selected + 1) % Count;
		Tiles[Selected].PopVelocity += 4.f;
		PlayUiSound(this, TEXT("/Game/Audio/Sounds/UI/sfx_UI_MainMenu_Select_nl_meta_Preset.sfx_UI_MainMenu_Select_nl_meta_Preset"), 0.4f, 1.2f);
	}
}
