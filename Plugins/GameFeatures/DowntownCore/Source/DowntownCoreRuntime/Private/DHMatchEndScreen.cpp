#include "DHMatchEndScreen.h"

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
#include "DHFreeForAllScoring.h"
#include "DHScreenStyle.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Teams/LyraTeamDisplayAsset.h"
#include "Teams/LyraTeamSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DHMatchEndScreen)

namespace
{
	UTextBlock* MakeText(UWidgetTree* Tree, const TCHAR* Font, int32 Size, const FLinearColor& Colour)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>();
		Text->SetFont(DHScreenStyle::Font(Font, Size));
		Text->SetColorAndOpacity(Colour);
		Text->SetShadowOffset(FVector2D(2, 2));
		Text->SetJustification(ETextJustify::Center);
		return Text;
	}
}

void UDHMatchEndScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	SetRenderOpacity(0.f);

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
	WidgetTree->RootWidget = Root;
	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>();
	Dim->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.55f));
	UOverlaySlot* DimSlot = Root->AddChildToOverlay(Dim);
	DimSlot->SetHorizontalAlignment(HAlign_Fill);
	DimSlot->SetVerticalAlignment(VAlign_Fill);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	UOverlaySlot* ColumnSlot = Root->AddChildToOverlay(Column);
	ColumnSlot->SetHorizontalAlignment(HAlign_Center);
	ColumnSlot->SetVerticalAlignment(VAlign_Center);

	ResultText = MakeText(WidgetTree, TEXT("Orbitron"), 72, DHScreenStyle::Accent);
	Column->AddChildToVerticalBox(ResultText)->SetHorizontalAlignment(HAlign_Center);
	WinnerText = MakeText(WidgetTree, TEXT("Orbitron"), 22, DHScreenStyle::Text);
	UVerticalBoxSlot* WinnerSlot = Column->AddChildToVerticalBox(WinnerText);
	WinnerSlot->SetHorizontalAlignment(HAlign_Center);
	WinnerSlot->SetPadding(FMargin(0, 4, 0, 28));

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetBrushColor(DHScreenStyle::Card);
	Panel->SetPadding(FMargin(24, 14));
	Column->AddChildToVerticalBox(Panel)->SetHorizontalAlignment(HAlign_Center);
	Table = WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->SetContent(Table);
}

void UDHMatchEndScreen::Fill()
{
	const UDHFreeForAllScoring* Scoring = UDHFreeForAllScoring::Find(this);
	const ULyraTeamSubsystem* Teams = GetWorld()->GetSubsystem<ULyraTeamSubsystem>();
	if (bLiveScoreboard)
	{
		ResultText->SetText(NSLOCTEXT("DowntownHunting", "BoardTitle", "SCOREBOARD"));
		ResultText->SetFont(DHScreenStyle::Font(TEXT("Orbitron"), 40));
		WinnerText->SetText(Scoring ? FText::FromString(Scoring->GetAnnouncement()) : FText::GetEmpty());
		FillTable();
		return;
	}
	if (!Scoring || !Teams || Scoring->GetWinnerName().IsEmpty())
	{
		return;
	}
	bFilled = true;

	const APlayerState* Own = GetOwningPlayerState();
	const bool bWon = Scoring->IsTeamScoring()
		? Scoring->GetWinnerTeam() != INDEX_NONE && Teams->FindTeamFromObject(Own) == Scoring->GetWinnerTeam()
		: Scoring->GetWinnerPlayer() == Own;
	ResultText->SetText(bWon ? NSLOCTEXT("DowntownHunting", "EndVictory", "VICTORY") : NSLOCTEXT("DowntownHunting", "EndDefeat", "DEFEAT"));
	ResultText->SetColorAndOpacity(bWon ? DHScreenStyle::Accent : FLinearColor(0.85f, 0.15f, 0.1f));
	WinnerText->SetText(FText::Format(NSLOCTEXT("DowntownHunting", "EndWinner", "WINNER: {0}"), FText::FromString(Scoring->GetWinnerName())));
	FillTable();
}

void UDHMatchEndScreen::FillTable()
{
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	ULyraTeamSubsystem* Teams = GetWorld()->GetSubsystem<ULyraTeamSubsystem>();
	if (!GameState || !Teams)
	{
		return;
	}
	const APlayerState* Own = GetOwningPlayerState();
	const UDHFreeForAllScoring* Scoring = UDHFreeForAllScoring::Find(this);
	const bool bByTeam = !Scoring || Scoring->IsTeamScoring();  // Lyra's own modes are team modes too

	// By team (team modes), then kills, fewer deaths first on a tie.
	TArray<const APlayerState*> Players;
	for (const APlayerState* PlayerState : GameState->PlayerArray)
	{
		Players.Add(PlayerState);
	}
	Players.Sort([Teams, bByTeam](const APlayerState& A, const APlayerState& B)
	{
		const int32 TeamA = bByTeam ? Teams->FindTeamFromObject(&A) : 0, TeamB = bByTeam ? Teams->FindTeamFromObject(&B) : 0;
		const int32 KillsA = UDHFreeForAllScoring::GetKills(&A), KillsB = UDHFreeForAllScoring::GetKills(&B);
		return TeamA != TeamB ? TeamA < TeamB : KillsA != KillsB ? KillsA > KillsB : UDHFreeForAllScoring::GetDeaths(&A) < UDHFreeForAllScoring::GetDeaths(&B);
	});
	const int32 Rows = FMath::Min(bLiveScoreboard ? 16 : 8, Players.Num()) + 1;  // + header

	// Rows are built once (and when players join or leave), refreshes only change the texts.
	if (Cells.Num() != Rows * 4)
	{
		Table->ClearChildren();
		Cells.Reset();
		const float Widths[] = { 280.f, 100.f, 100.f, 90.f };
		for (int32 Row = 0; Row < Rows; ++Row)
		{
			UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>();
			for (int32 Column = 0; Column < 4; ++Column)
			{
				USizeBox* Cell = WidgetTree->ConstructWidget<USizeBox>();
				Cell->SetWidthOverride(Widths[Column]);
				UTextBlock* Text = MakeText(WidgetTree, Column == 0 ? TEXT("Orbitron") : TEXT("NotoSans"), 17, DHScreenStyle::Text);
				Text->SetJustification(Column == 0 ? ETextJustify::Left : ETextJustify::Right);
				Text->SetClipping(EWidgetClipping::ClipToBounds);
				Cell->AddChild(Text);
				Line->AddChildToHorizontalBox(Cell);
				Cells.Add(Text);
			}
			Table->AddChildToVerticalBox(Line)->SetPadding(FMargin(0, 3));
		}
	}
	auto SetRow = [this](int32 Row, const FString& Name, const FString& Kills, const FString& Deaths, const FString& Ratio, const FLinearColor& Colour)
	{
		const FString Texts[] = { Name, Kills, Deaths, Ratio };
		for (int32 Column = 0; Column < 4; ++Column)
		{
			Cells[Row * 4 + Column]->SetText(FText::FromString(Texts[Column]));
			Cells[Row * 4 + Column]->SetColorAndOpacity(Colour);
		}
	};
	SetRow(0, TEXT("PLAYER"), TEXT("KILLS"), TEXT("DEATHS"), TEXT("K/D"), DHScreenStyle::Accent);
	for (int32 Row = 1; Row < Rows; ++Row)
	{
		const APlayerState* Player = Players[Row - 1];
		const int32 Kills = UDHFreeForAllScoring::GetKills(Player), Deaths = UDHFreeForAllScoring::GetDeaths(Player);
		FLinearColor Colour = DHScreenStyle::Text;
		if (bByTeam)
		{
			const ULyraTeamDisplayAsset* Display = Teams->GetTeamDisplayAsset(Teams->FindTeamFromObject(Player), INDEX_NONE);
			const FLinearColor* TeamColour = Display ? Display->ColorParameters.Find(TEXT("TeamColor")) : nullptr;
			Colour = TeamColour ? FMath::Lerp(*TeamColour, FLinearColor::White, 0.35f) : Colour;
		}
		SetRow(Row, Player == Own ? FString::Printf(TEXT("> %s"), *Player->GetPlayerName()) : Player->GetPlayerName(),
			FString::FromInt(Kills), FString::FromInt(Deaths), FString::Printf(TEXT("%.2f"), Kills / float(FMath::Max(1, Deaths))),
			Player == Own && !bByTeam ? DHScreenStyle::Accent : Colour);
	}
}

void UDHMatchEndScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Age += InDeltaTime;
	if (bLiveScoreboard)
	{
		// Live table: refreshed twice a second, appears fast.
		if ((RefreshTimer -= InDeltaTime) <= 0.f)
		{
			RefreshTimer = 0.5f;
			Fill();
		}
		SetRenderOpacity(FMath::Clamp(Age / 0.15f, 0.f, 1.f));
		return;
	}
	if (!bFilled)
	{
		Fill();  // the winner replicates a moment after the end
	}
	SetRenderOpacity(FMath::Clamp(Age / 0.6f, 0.f, 1.f));
	SetRenderScale(FVector2D(1.f + 0.08f * FMath::Clamp(1.f - Age / 0.6f, 0.f, 1.f)));
}
