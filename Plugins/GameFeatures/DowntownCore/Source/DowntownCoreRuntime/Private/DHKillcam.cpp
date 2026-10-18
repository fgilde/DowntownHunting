#include "DHKillcam.h"

#include "Blueprint/WidgetTree.h"
#include "Camera/CameraActor.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "DHScreenStyle.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DHKillcam)

namespace DHKillcamSettings
{
	constexpr float Delay = 1.0f;     // own death first
	constexpr float Duration = 4.5f;
	constexpr float Back = 380.f, Up = 170.f;
}

void UDHKillcam::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	SetRenderOpacity(0.f);

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
	WidgetTree->RootWidget = Root;
	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetBrushColor(DHScreenStyle::Card);
	Panel->SetPadding(FMargin(28, 10));
	UOverlaySlot* PanelSlot = Root->AddChildToOverlay(Panel);
	PanelSlot->SetHorizontalAlignment(HAlign_Center);
	PanelSlot->SetVerticalAlignment(VAlign_Bottom);
	PanelSlot->SetPadding(FMargin(0, 0, 0, 170));

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->SetContent(Column);
	UTextBlock* Title = DHScreenStyle::MakeText(WidgetTree, TEXT("Orbitron"), 14, DHScreenStyle::Text);
	Title->SetText(NSLOCTEXT("DowntownHunting", "KilledBy", "KILLED BY"));
	Column->AddChildToVerticalBox(Title)->SetHorizontalAlignment(HAlign_Center);
	NameText = DHScreenStyle::MakeText(WidgetTree, TEXT("Orbitron"), 30, FLinearColor(0.95f, 0.2f, 0.12f));
	Column->AddChildToVerticalBox(NameText)->SetHorizontalAlignment(HAlign_Center);
	InfoText = DHScreenStyle::MakeText(WidgetTree, TEXT("NotoSans"), 15, DHScreenStyle::Text);
	Column->AddChildToVerticalBox(InfoText)->SetHorizontalAlignment(HAlign_Center);
}

void UDHKillcam::Start(APlayerState* Killer, float KillerHealth, float Distance)
{
	KillerState = Killer;
	DeadPawn = GetOwningPlayerPawn();
	NameText->SetText(FText::FromString(Killer ? Killer->GetPlayerName() : TEXT("?")));
	InfoText->SetText(FText::Format(NSLOCTEXT("DowntownHunting", "KillInfo", "{0} m away  ·  {1} HP left"),
		FMath::RoundToInt(Distance / 100.f), FMath::RoundToInt(KillerHealth)));
}

void UDHKillcam::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	using namespace DHKillcamSettings;
	Age += InDeltaTime;
	APlayerController* Player = GetOwningPlayer();
	const APawn* KillerPawn = KillerState.IsValid() ? KillerState->GetPawn() : nullptr;

	// Respawned or out of time: back to normal.
	const APawn* OwnPawn = Player ? Player->GetPawn() : nullptr;
	if (!Player || Age > Delay + Duration || (OwnPawn && OwnPawn != DeadPawn.Get()))
	{
		Finish();
		return;
	}
	SetRenderOpacity(FMath::Clamp((Age - Delay) / 0.3f, 0.f, 1.f));
	if (Age < Delay || !KillerPawn)
	{
		return;
	}

	// Behind and above the killer, looking at them.
	const FVector Target = KillerPawn->GetActorLocation() + FVector(0, 0, 60);
	const FVector Forward = KillerPawn->GetActorForwardVector().GetSafeNormal2D();
	const FVector Desired = Target - Forward * Back + FVector(0, 0, Up);
	if (!Camera)
	{
		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		Camera = GetWorld()->SpawnActor<ACameraActor>(Desired, (Target - Desired).Rotation(), Params);
		Player->SetViewTargetWithBlend(Camera, 0.6f, VTBlend_EaseInOut, 2.f);
		bFollowing = true;
	}
	const FVector Location = FMath::VInterpTo(Camera->GetActorLocation(), Desired, InDeltaTime, 6.f);
	Camera->SetActorLocationAndRotation(Location, (Target - Location).Rotation());
}

void UDHKillcam::Finish()
{
	APlayerController* Player = GetOwningPlayer();
	if (bFollowing && Player && Player->GetViewTarget() == Camera)
	{
		// Still dead: back to the player's own (death) view.
		Player->SetViewTargetWithBlend(Player->GetPawn() ? static_cast<AActor*>(Player->GetPawn()) : static_cast<AActor*>(Player), 0.4f);
	}
	bFollowing = false;
	RemoveFromParent();
}

void UDHKillcam::NativeDestruct()
{
	if (Camera)
	{
		Camera->Destroy();
		Camera = nullptr;
	}
	Super::NativeDestruct();
}
