#include "DHFreeForAllHud.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "DHFreeForAllScoring.h"
#include "DHScreenStyle.h"
#include "GameFramework/PlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DHFreeForAllHud)

void UDHFreeForAllHud::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetBrushColor(DHScreenStyle::Card);
	Panel->SetPadding(FMargin(18, 6));
	Panel->SetHorizontalAlignment(HAlign_Center);
	WidgetTree->RootWidget = Panel;

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	Panel->SetContent(Row);

	auto AddText = [this, Row](int32 Size, const FLinearColor& Colour, float MinWidth)
	{
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>();
		Text->SetFont(DHScreenStyle::Font(TEXT("Orbitron"), Size));
		Text->SetColorAndOpacity(Colour);
		Text->SetJustification(ETextJustify::Center);
		Text->SetMinDesiredWidth(MinWidth);
		UHorizontalBoxSlot* Slot = Row->AddChildToHorizontalBox(Text);
		Slot->SetVerticalAlignment(VAlign_Center);
		Slot->SetPadding(FMargin(10, 0));
		return Text;
	};
	OwnText = AddText(18, DHScreenStyle::Accent, 140.f);
	ClockText = AddText(26, DHScreenStyle::Text, 110.f);
	LeaderText = AddText(14, DHScreenStyle::Text, 220.f);
}

void UDHFreeForAllHud::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const UDHFreeForAllScoring* Scoring = UDHFreeForAllScoring::Find(this);
	if (!Scoring)
	{
		return;
	}
	const APlayerState* Own = GetOwningPlayerState();
	OwnText->SetText(FText::Format(NSLOCTEXT("DowntownHunting", "FfaOwn", "YOU {0} / {1}"),
		UDHFreeForAllScoring::GetKills(Own), Scoring->GetKillLimit()));

	const float Seconds = Scoring->GetSecondsLeft();
	const int32 Whole = FMath::CeilToInt(FMath::Max(Seconds, 0.f));
	ClockText->SetText(Seconds < 0.f ? NSLOCTEXT("DowntownHunting", "FfaWarmup", "WARMUP")
		: FText::FromString(FString::Printf(TEXT("%d:%02d"), Whole / 60, Whole % 60)));

	if (Scoring->IsMatchOver())
	{
		LeaderText->SetText(FText::Format(NSLOCTEXT("DowntownHunting", "FfaWinner", "WINNER: {0}"), FText::FromString(Scoring->GetWinnerName())));
		LeaderText->SetColorAndOpacity(DHScreenStyle::Accent);
	}
	else if (const APlayerState* Leader = Scoring->GetLeader())
	{
		LeaderText->SetText(Leader == Own
			? FText::Format(NSLOCTEXT("DowntownHunting", "FfaYouLead", "YOU LEAD ({0})"), UDHFreeForAllScoring::GetKills(Leader))
			: FText::Format(NSLOCTEXT("DowntownHunting", "FfaLeader", "LEADER {0} ({1})"), FText::FromString(Leader->GetPlayerName()), UDHFreeForAllScoring::GetKills(Leader)));
	}
	else
	{
		LeaderText->SetText(NSLOCTEXT("DowntownHunting", "FfaNoLeader", "FREE FOR ALL"));
	}
}
