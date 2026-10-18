#include "DHObjectiveHud.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "DHBombSite.h"
#include "DHFreeForAllScoring.h"
#include "DHScreenStyle.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Teams/LyraTeamSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DHObjectiveHud)

namespace
{
	UTextBlock* MakeText(UWidgetTree* Tree, int32 Size, const FLinearColor& Colour)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>();
		Text->SetFont(DHScreenStyle::Font(TEXT("Orbitron"), Size));
		Text->SetColorAndOpacity(Colour);
		Text->SetShadowOffset(FVector2D(1, 1));
		Text->SetJustification(ETextJustify::Center);
		return Text;
	}

	UCanvasPanelSlot* Place(UCanvasPanel* Canvas, UWidget* Widget, const FVector2D& Anchor, const FVector2D& Offset)
	{
		UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Widget);
		Slot->SetAnchors(FAnchors(Anchor.X, Anchor.Y));
		Slot->SetAlignment(FVector2D(0.5f, 0.5f));
		Slot->SetPosition(Offset);
		Slot->SetAutoSize(true);
		return Slot;
	}
}

void UDHObjectiveHud::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::HitTestInvisible);

	Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Canvas;

	TaskText = MakeText(WidgetTree, 15, DHScreenStyle::Text);
	Place(Canvas, TaskText, FVector2D(0.5f, 0.f), FVector2D(0, 105));

	ProgressText = MakeText(WidgetTree, 16, DHScreenStyle::Accent);
	Place(Canvas, ProgressText, FVector2D(0.5f, 0.62f), FVector2D(0, -22));
	ProgressBar = WidgetTree->ConstructWidget<UProgressBar>();
	ProgressBar->SetFillColorAndOpacity(DHScreenStyle::Accent);
	UCanvasPanelSlot* BarSlot = Place(Canvas, ProgressBar, FVector2D(0.5f, 0.62f), FVector2D(0, 0));
	BarSlot->SetAutoSize(false);
	BarSlot->SetSize(FVector2D(320, 10));
}

void UDHObjectiveHud::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const UDHSearchAndDestroyScoring* Scoring = Cast<UDHSearchAndDestroyScoring>(UDHFreeForAllScoring::Find(this));
	APlayerController* Player = GetOwningPlayer();
	const ULyraTeamSubsystem* Teams = GetWorld()->GetSubsystem<ULyraTeamSubsystem>();
	if (!Scoring || !Player || !Teams)
	{
		return;
	}
	const bool bAttacking = Teams->FindTeamFromObject(GetOwningPlayerState()) == Scoring->GetAttackingTeam();
	const ADHBombSite* Planted = Scoring->GetPlantedSite();

	TaskText->SetText(Planted
		? (bAttacking ? FText::Format(NSLOCTEXT("DowntownHunting", "SndGuard", "ATTACK: GUARD THE BOMB AT {0}"), FText::FromString(Planted->Letter))
			: FText::Format(NSLOCTEXT("DowntownHunting", "SndDefuse", "DEFEND: DEFUSE THE BOMB AT {0}"), FText::FromString(Planted->Letter)))
		: (bAttacking ? NSLOCTEXT("DowntownHunting", "SndPlant", "ATTACK: PLANT THE BOMB AT A OR B")
			: NSLOCTEXT("DowntownHunting", "SndProtect", "DEFEND: PROTECT A AND B")));

	const float Progress = Scoring->GetProgress();
	const ESlateVisibility ProgressVisibility = Progress > 0.f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	ProgressBar->SetVisibility(ProgressVisibility);
	ProgressText->SetVisibility(ProgressVisibility);
	ProgressBar->SetPercent(Progress);
	ProgressText->SetText(Planted ? NSLOCTEXT("DowntownHunting", "SndDefusing", "DEFUSING") : NSLOCTEXT("DowntownHunting", "SndPlanting", "PLANTING"));

	// A marker over every site: letter and distance, red once the bomb is there.
	const TArray<TObjectPtr<ADHBombSite>>& Sites = Scoring->GetSites();
	while (Markers.Num() < Sites.Num())
	{
		UTextBlock* Marker = MakeText(WidgetTree, 18, DHScreenStyle::Accent);
		Canvas->AddChildToCanvas(Marker)->SetAutoSize(true);
		Markers.Add(Marker);
	}
	const APawn* Pawn = Player->GetPawn();
	for (int32 Index = 0; Index < Sites.Num(); ++Index)
	{
		const ADHBombSite* Site = Sites[Index];
		UTextBlock* Marker = Markers[Index];
		FVector2D Screen;
		const FVector Location = Site ? Site->GetActorLocation() + FVector(0, 0, 250) : FVector::ZeroVector;
		if (!Site || !UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(Player, Location, Screen, false))
		{
			Marker->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}
		Marker->SetVisibility(ESlateVisibility::HitTestInvisible);
		const int32 Metres = Pawn ? FMath::RoundToInt(FVector::Dist(Pawn->GetActorLocation(), Site->GetActorLocation()) / 100.f) : 0;
		Marker->SetText(FText::FromString(FString::Printf(TEXT("%s\n%d m"), *Site->Letter, Metres)));
		Marker->SetColorAndOpacity(Site == Planted ? FLinearColor(1.f, 0.1f, 0.05f) : DHScreenStyle::Accent);
		if (UCanvasPanelSlot* MarkerSlot = Cast<UCanvasPanelSlot>(Marker->Slot))
		{
			MarkerSlot->SetAlignment(FVector2D(0.5f, 1.f));
			MarkerSlot->SetPosition(Screen);
		}
	}
}
