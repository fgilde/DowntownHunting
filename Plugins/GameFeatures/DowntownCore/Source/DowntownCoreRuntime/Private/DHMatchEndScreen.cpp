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
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	const ULyraTeamSubsystem* Teams = GetWorld()->GetSubsystem<ULyraTeamSubsystem>();
	if (!Scoring || !GameState || !Teams || Scoring->GetWinnerName().IsEmpty())
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

	// Best players by kills, fewer deaths first on a tie.
	TArray<const APlayerState*> Players;
	for (const APlayerState* PlayerState : GameState->PlayerArray)
	{
		Players.Add(PlayerState);
	}
	Players.Sort([](const APlayerState& A, const APlayerState& B)
	{
		const int32 KillsA = UDHFreeForAllScoring::GetKills(&A), KillsB = UDHFreeForAllScoring::GetKills(&B);
		return KillsA != KillsB ? KillsA > KillsB : UDHFreeForAllScoring::GetDeaths(&A) < UDHFreeForAllScoring::GetDeaths(&B);
	});
	auto AddRow = [this](const FString& Name, const FString& Kills, const FString& Deaths, const FLinearColor& Colour)
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
		const FString Cells[] = { Name, Kills, Deaths };
		const float Widths[] = { 280.f, 110.f, 110.f };
		for (int32 Index = 0; Index < 3; ++Index)
		{
			USizeBox* Cell = WidgetTree->ConstructWidget<USizeBox>();
			Cell->SetWidthOverride(Widths[Index]);
			UTextBlock* Text = MakeText(WidgetTree, Index == 0 ? TEXT("Orbitron") : TEXT("NotoSans"), 17, Colour);
			Text->SetJustification(Index == 0 ? ETextJustify::Left : ETextJustify::Right);
			Text->SetText(FText::FromString(Cells[Index]));
			Cell->AddChild(Text);
			Row->AddChildToHorizontalBox(Cell);
		}
		Table->AddChildToVerticalBox(Row)->SetPadding(FMargin(0, 3));
	};
	AddRow(TEXT("PLAYER"), TEXT("KILLS"), TEXT("DEATHS"), DHScreenStyle::Accent);
	for (int32 Index = 0; Index < FMath::Min(8, Players.Num()); ++Index)
	{
		const APlayerState* Player = Players[Index];
		AddRow(Player->GetPlayerName(), FString::FromInt(UDHFreeForAllScoring::GetKills(Player)),
			FString::FromInt(UDHFreeForAllScoring::GetDeaths(Player)), Player == Own ? DHScreenStyle::Accent : DHScreenStyle::Text);
	}
}

void UDHMatchEndScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!bFilled)
	{
		Fill();  // the winner replicates a moment after the end
	}
	Age += InDeltaTime;
	SetRenderOpacity(FMath::Clamp(Age / 0.6f, 0.f, 1.f));
	SetRenderScale(FVector2D(1.f + 0.08f * FMath::Clamp(1.f - Age / 0.6f, 0.f, 1.f)));
}
