#include "DHFreeForAllHud.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "DHFreeForAllScoring.h"
#include "DHMatchEndScreen.h"
#include "DHScreenStyle.h"
#include "GameFramework/PlayerState.h"
#include "Teams/LyraTeamDisplayAsset.h"
#include "Teams/LyraTeamSubsystem.h"

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

	auto Add = [Row](UWidget* Widget)
	{
		UHorizontalBoxSlot* Slot = Row->AddChildToHorizontalBox(Widget);
		Slot->SetVerticalAlignment(VAlign_Center);
		Slot->SetPadding(FMargin(10, 0));
	};
	OwnText = MakeText(18, DHScreenStyle::Accent, 140.f);
	Add(OwnText);
	TeamRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	Add(TeamRow);
	ClockText = MakeText(26, DHScreenStyle::Text, 110.f);
	Add(ClockText);
	LeaderText = MakeText(14, DHScreenStyle::Text, 220.f);
	Add(LeaderText);
}

UTextBlock* UDHFreeForAllHud::MakeText(int32 Size, const FLinearColor& Colour, float MinWidth) const
{
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>();
	Text->SetFont(DHScreenStyle::Font(TEXT("Orbitron"), Size));
	Text->SetColorAndOpacity(Colour);
	Text->SetJustification(ETextJustify::Center);
	Text->SetMinDesiredWidth(MinWidth);
	return Text;
}

void UDHFreeForAllHud::UpdateTeams()
{
	ULyraTeamSubsystem* Teams = GetWorld()->GetSubsystem<ULyraTeamSubsystem>();
	TArray<int32> TeamIds = Teams ? Teams->GetTeamIDs() : TArray<int32>();
	TeamIds.Sort();
	while (TeamTexts.Num() < TeamIds.Num())
	{
		UTextBlock* Text = MakeText(24, DHScreenStyle::Text, 60.f);
		TeamRow->AddChildToHorizontalBox(Text)->SetPadding(FMargin(6, 0));
		TeamTexts.Add(Text);
	}
	for (int32 Index = 0; Index < TeamIds.Num(); ++Index)
	{
		const ULyraTeamDisplayAsset* Display = Teams->GetTeamDisplayAsset(TeamIds[Index], INDEX_NONE);
		const FLinearColor* Colour = Display ? Display->ColorParameters.Find(TEXT("TeamColor")) : nullptr;
		TeamTexts[Index]->SetColorAndOpacity(Colour ? FSlateColor(*Colour) : FSlateColor(DHScreenStyle::Text));
		TeamTexts[Index]->SetText(FText::AsNumber(UDHFreeForAllScoring::GetTeamKills(this, TeamIds[Index])));
	}
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
	if (Scoring->IsTeamScoring())
	{
		// Infected ends by infection or time: no score limit to show.
		OwnText->SetText(Scoring->IsA<UDHInfectedScoring>() ? FText::GetEmpty()
			: FText::Format(NSLOCTEXT("DowntownHunting", "TeamLimit", "TO {0}"), Scoring->GetKillLimit()));
		UpdateTeams();
	}
	else
	{
		OwnText->SetText(FText::Format(NSLOCTEXT("DowntownHunting", "FfaOwn", "YOU {0} / {1}"),
			UDHFreeForAllScoring::GetKills(Own), Scoring->GetKillLimit()));
	}

	const float Seconds = Scoring->GetSecondsLeft();
	const int32 Whole = FMath::CeilToInt(FMath::Max(Seconds, 0.f));
	ClockText->SetText(Seconds < 0.f ? NSLOCTEXT("DowntownHunting", "FfaWarmup", "WARMUP")
		: FText::FromString(FString::Printf(TEXT("%d:%02d"), Whole / 60, Whole % 60)));

	LeaderText->SetColorAndOpacity(DHScreenStyle::Text);
	if (Scoring->IsMatchOver() && !EndScreen && GetOwningPlayer())
	{
		EndScreen = CreateWidget<UDHMatchEndScreen>(GetOwningPlayer());
		EndScreen->AddToViewport(50);
	}
	if (Scoring->IsMatchOver())
	{
		LeaderText->SetText(FText::Format(NSLOCTEXT("DowntownHunting", "FfaWinner", "WINNER: {0}"), FText::FromString(Scoring->GetWinnerName())));
		LeaderText->SetColorAndOpacity(DHScreenStyle::Accent);
	}
	else if (!Scoring->GetAnnouncement().IsEmpty())
	{
		LeaderText->SetText(FText::FromString(Scoring->GetAnnouncement()));
		LeaderText->SetColorAndOpacity(DHScreenStyle::Accent);
	}
	else if (Scoring->IsTeamScoring())
	{
		LeaderText->SetText(FText::GetEmpty());
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
