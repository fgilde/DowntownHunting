#include "DHStrikeMap.h"

#include "Blueprint/WidgetTree.h"
#include "CommonUIExtensions.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "DHKillstreaks.h"
#include "DHLoadoutSettings.h"
#include "DHMinimap.h"
#include "DHPlayerChoicesComponent.h"
#include "DHScreenStyle.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameplayTagContainer.h"
#include "InputCoreTypes.h"
#include "Teams/LyraTeamSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DHStrikeMap)

namespace
{
	constexpr float MapSize = 720.f;
	constexpr float StickSpeed = 0.45f;  // map widths per second at full tilt
	constexpr float MapCrop = 0.1f;      // of the capture on each side

	// White texture with an alpha mask from the distance to the centre (0..1).
	template <typename TAlpha>
	UTexture2D* MakeRound(int32 Size, TAlpha Alpha)
	{
		UTexture2D* Texture = UTexture2D::CreateTransient(Size, Size, PF_B8G8R8A8);
		FColor* Pixels = static_cast<FColor*>(Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE));
		for (int32 Y = 0; Y < Size; ++Y)
		{
			for (int32 X = 0; X < Size; ++X)
			{
				const float Distance = FVector2f(2.f * (X + 0.5f) / Size - 1.f, 2.f * (Y + 0.5f) / Size - 1.f).Size();
				Pixels[Y * Size + X] = FColor(255, 255, 255, uint8(255.f * FMath::Clamp(Alpha(Distance), 0.f, 1.f)));
			}
		}
		Texture->GetPlatformData()->Mips[0].BulkData.Unlock();
		Texture->UpdateResource();
		return Texture;
	}
}

UDHStrikeMap::UDHStrikeMap(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsBackHandler = true;
	SetIsFocusable(true);
}

void UDHStrikeMap::Open(APlayerController* Player, const FString& StreakId)
{
	UDHPlayerChoicesComponent* Choices = Player ? Player->FindComponentByClass<UDHPlayerChoicesComponent>() : nullptr;
	if (!Choices)
	{
		return;
	}
	UTexture2D* Texture = nullptr;
	FVector2D Centre;
	float Extent = 0.f;
	if (!UDHMinimap::GetCapture(Player->GetWorld(), Texture, Centre, Extent))
	{
		// No map (hardcore): where the player aims.
		FVector Eye;
		FRotator View;
		Player->GetPlayerViewPoint(Eye, View);
		FHitResult Hit;
		FCollisionQueryParams Params(TEXT("DHStrikeAim"), false, Player->GetPawn());
		const FVector End = Eye + View.Vector() * 30000.f;
		Choices->ServerUseStreak(StreakId, Player->GetWorld()->LineTraceSingleByChannel(Hit, Eye, End, ECC_Visibility, Params) ? Hit.ImpactPoint : End);
		return;
	}
	UDHStrikeMap* Map = Cast<UDHStrikeMap>(UCommonUIExtensions::PushContentToLayer_ForPlayer(Player->GetLocalPlayer(),
		FGameplayTag::RequestGameplayTag(TEXT("UI.Layer.Menu")), StaticClass()));
	if (Map)
	{
		// The capture reaches past the play area on every side: shown a little zoomed in.
		Map->StreakId = StreakId;
		Map->MapImage->SetBrushResourceObject(Texture);
		FSlateBrush Brush = Map->MapImage->GetBrush();
		Brush.SetUVRegion(FBox2D(FVector2D(MapCrop), FVector2D(1.f - MapCrop)));
		Map->MapImage->SetBrush(Brush);
		Map->Centre = Centre;
		Map->Extent = Extent * (1.f - 2.f * MapCrop);
		const APawn* Pawn = Player->GetPawn();
		Map->Cursor = Pawn ? Map->ToMap(Pawn->GetActorLocation()) : FVector2D(0.5f, 0.5f);
	}
}

void UDHStrikeMap::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	DotTexture = MakeRound(64, [](float D) { return FMath::SmoothStep(1.f, 0.7f, D); });
	RingTexture = MakeRound(256, [](float D) { return FMath::Max(FMath::SmoothStep(1.f, 0.95f, D) * FMath::SmoothStep(0.86f, 0.91f, D), 0.18f * FMath::SmoothStep(1.f, 0.95f, D)); });

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
	WidgetTree->RootWidget = Root;
	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>();
	Dim->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.7f));
	UOverlaySlot* DimSlot = Root->AddChildToOverlay(Dim);
	DimSlot->SetHorizontalAlignment(HAlign_Fill);
	DimSlot->SetVerticalAlignment(VAlign_Fill);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	UOverlaySlot* ColumnSlot = Root->AddChildToOverlay(Column);
	ColumnSlot->SetHorizontalAlignment(HAlign_Center);
	ColumnSlot->SetVerticalAlignment(VAlign_Center);
	UTextBlock* Title = DHScreenStyle::MakeText(WidgetTree, TEXT("Orbitron"), 34, DHScreenStyle::Accent);
	Title->SetText(NSLOCTEXT("DowntownHunting", "StrikeMapTitle", "PICK THE TARGET"));
	Title->SetRenderTransformAngle(-2.f);
	Column->AddChildToVerticalBox(Title)->SetHorizontalAlignment(HAlign_Center);
	Cast<UVerticalBoxSlot>(Title->Slot)->SetPadding(FMargin(0, 0, 0, 10));

	UBorder* Frame = WidgetTree->ConstructWidget<UBorder>();
	Frame->SetBrushColor(DHScreenStyle::Accent);
	Frame->SetPadding(FMargin(3));
	Column->AddChildToVerticalBox(Frame)->SetHorizontalAlignment(HAlign_Center);
	MapBox = WidgetTree->ConstructWidget<USizeBox>();
	MapBox->SetWidthOverride(MapSize);
	MapBox->SetHeightOverride(MapSize);
	Frame->SetContent(MapBox);
	UOverlay* Stack = WidgetTree->ConstructWidget<UOverlay>();
	Stack->SetClipping(EWidgetClipping::ClipToBounds);
	MapBox->AddChild(Stack);
	MapImage = WidgetTree->ConstructWidget<UImage>();
	UOverlaySlot* ImageSlot = Stack->AddChildToOverlay(MapImage);
	ImageSlot->SetHorizontalAlignment(HAlign_Fill);
	ImageSlot->SetVerticalAlignment(VAlign_Fill);
	Markers = WidgetTree->ConstructWidget<UCanvasPanel>();
	UOverlaySlot* MarkersSlot = Stack->AddChildToOverlay(Markers);
	MarkersSlot->SetHorizontalAlignment(HAlign_Fill);
	MarkersSlot->SetVerticalAlignment(VAlign_Fill);
	SelfDot = AddMarker(DotTexture, 16.f, FLinearColor::White);
	TargetRing = AddMarker(RingTexture, 100.f, FLinearColor(1.f, 0.1f, 0.05f));
	TargetDot = AddMarker(DotTexture, 10.f, FLinearColor(1.f, 0.1f, 0.05f));

	UTextBlock* Hint = DHScreenStyle::MakeText(WidgetTree, TEXT("Orbitron"), 14, DHScreenStyle::Accent);
	Hint->SetText(NSLOCTEXT("DowntownHunting", "StrikeMapHint", "CLICK / A  STRIKE      RIGHT CLICK / ESC / B  CANCEL      LEFT STICK  AIM"));
	Column->AddChildToVerticalBox(Hint)->SetHorizontalAlignment(HAlign_Center);
	Cast<UVerticalBoxSlot>(Hint->Slot)->SetPadding(FMargin(0, 10, 0, 0));
}

UImage* UDHStrikeMap::AddMarker(UTexture2D* Texture, float Size, const FLinearColor& Colour)
{
	UImage* Marker = WidgetTree->ConstructWidget<UImage>();
	Marker->SetBrushFromTexture(Texture);
	Marker->SetColorAndOpacity(Colour);
	Marker->SetVisibility(ESlateVisibility::HitTestInvisible);
	UCanvasPanelSlot* MarkerSlot = Markers->AddChildToCanvas(Marker);
	MarkerSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	MarkerSlot->SetSize(FVector2D(Size));
	return Marker;
}

void UDHStrikeMap::Place(UImage* Marker, const FVector2D& Map, float Size)
{
	if (UCanvasPanelSlot* MarkerSlot = Cast<UCanvasPanelSlot>(Marker->Slot))
	{
		MarkerSlot->SetPosition(Map * MapSize);
		MarkerSlot->SetSize(FVector2D(Size));
	}
}

FVector2D UDHStrikeMap::ToMap(const FVector& World) const
{
	// Like the minimap's capture: image up is +X, image right is +Y.
	return FVector2D(0.5f + (World.Y - Centre.Y) / Extent, 0.5f - (World.X - Centre.X) / Extent);
}

FVector UDHStrikeMap::ToWorld(const FVector2D& Map) const
{
	return FVector(Centre.X + (0.5f - Map.Y) * Extent, Centre.Y + (Map.X - 0.5f) * Extent, 0.f);
}

void UDHStrikeMap::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Age += InDeltaTime;
	// Stick with a dead zone; up on the stick is up on the map.
	const FVector2D Move(FMath::Abs(Stick.X) > 0.15f ? Stick.X : 0.f, FMath::Abs(Stick.Y) > 0.15f ? -Stick.Y : 0.f);
	Cursor = (Cursor + Move * StickSpeed * InDeltaTime).ClampAxes(0.f, 1.f);

	const float Ring = FMath::Max(48.f, 2.f * DHKillstreaks::StrikeSpread(StreakId) / Extent * MapSize);
	Place(TargetRing, Cursor, Ring * (1.f + 0.04f * FMath::Sin(Age * 6.f)));
	Place(TargetDot, Cursor, 10.f);
	TargetRing->SetRenderTransformAngle(Age * 40.f);

	const APlayerController* PC = GetOwningPlayer();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	SelfDot->SetVisibility(Pawn ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (Pawn)
	{
		Place(SelfDot, ToMap(Pawn->GetActorLocation()), 16.f);
	}
	// Teammates, so nobody bombs their own.
	int32 Used = 0;
	const ULyraTeamSubsystem* Teams = GetWorld()->GetSubsystem<ULyraTeamSubsystem>();
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	if (PC && Teams && GameState)
	{
		const int32 OwnTeam = Teams->FindTeamFromObject(PC->PlayerState);
		for (const APlayerState* Other : GameState->PlayerArray)
		{
			const APawn* OtherPawn = Other->GetPawn();
			if (!OtherPawn || Other == PC->PlayerState || OwnTeam == INDEX_NONE || Teams->FindTeamFromObject(Other) != OwnTeam)
			{
				continue;
			}
			if (MateDots.Num() <= Used)
			{
				MateDots.Add(AddMarker(DotTexture, 12.f, FLinearColor(0.2f, 0.6f, 1.f)));
			}
			MateDots[Used]->SetVisibility(ESlateVisibility::HitTestInvisible);
			Place(MateDots[Used++], ToMap(OtherPawn->GetActorLocation()), 12.f);
		}
	}
	for (int32 Index = Used; Index < MateDots.Num(); ++Index)
	{
		MateDots[Index]->SetVisibility(ESlateVisibility::Collapsed);
	}
	// Dead in the meantime: nothing to call in.
	if (!Pawn && IsActivated())
	{
		DeactivateWidget();
	}
}

TOptional<FUIInputConfig> UDHStrikeMap::GetDesiredInputConfig() const
{
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

UWidget* UDHStrikeMap::NativeGetDesiredFocusTarget() const
{
	return const_cast<UDHStrikeMap*>(this);
}

FReply UDHStrikeMap::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FGeometry& Box = MapBox->GetCachedGeometry();
	Cursor = (Box.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition()) / Box.GetLocalSize()).ClampAxes(0.f, 1.f);
	return FReply::Handled();
}

FReply UDHStrikeMap::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		DeactivateWidget();
	}
	else if (MapBox->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition()))
	{
		Confirm();
	}
	return FReply::Handled();
}

FReply UDHStrikeMap::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Gamepad_FaceButton_Bottom || Key == EKeys::Enter || Key == EKeys::SpaceBar)
	{
		Confirm();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UDHStrikeMap::NativeOnAnalogValueChanged(const FGeometry& InGeometry, const FAnalogInputEvent& InAnalogEvent)
{
	if (InAnalogEvent.GetKey() == EKeys::Gamepad_LeftX)
	{
		Stick.X = InAnalogEvent.GetAnalogValue();
		return FReply::Handled();
	}
	if (InAnalogEvent.GetKey() == EKeys::Gamepad_LeftY)
	{
		Stick.Y = InAnalogEvent.GetAnalogValue();
		return FReply::Handled();
	}
	return Super::NativeOnAnalogValueChanged(InGeometry, InAnalogEvent);
}

void UDHStrikeMap::Confirm()
{
	APlayerController* PC = GetOwningPlayer();
	if (UDHPlayerChoicesComponent* Choices = PC ? PC->FindComponentByClass<UDHPlayerChoicesComponent>() : nullptr)
	{
		Choices->ServerUseStreak(StreakId, ToWorld(Cursor));
		if (USoundBase* Sound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/Sounds/UI/sfx_UI_MainMenu_Select_nl_meta_Preset.sfx_UI_MainMenu_Select_nl_meta_Preset")))
		{
			UGameplayStatics::PlaySound2D(this, Sound, 0.8f, 0.7f);
		}
	}
	DeactivateWidget();
}
