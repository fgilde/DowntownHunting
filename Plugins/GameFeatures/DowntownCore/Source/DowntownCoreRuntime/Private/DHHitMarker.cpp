#include "DHHitMarker.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "DHScreenStyle.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DHHitMarker)

namespace DHHitMarkerSettings
{
	constexpr float KillSeconds = 0.6f;
	constexpr float HeadshotSeconds = 1.2f;
	const FLinearColor KillColour(1.f, 0.12f, 0.08f);
}

void UDHHitMarker::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
	WidgetTree->RootWidget = Root;

	// X from two thin bars, rotated.
	UOverlay* X = WidgetTree->ConstructWidget<UOverlay>();
	for (const float Angle : { 45.f, -45.f })
	{
		USizeBox* Bar = WidgetTree->ConstructWidget<USizeBox>();
		Bar->SetWidthOverride(44.f);
		Bar->SetHeightOverride(4.f);
		UBorder* Fill = WidgetTree->ConstructWidget<UBorder>();
		Fill->SetBrushColor(DHHitMarkerSettings::KillColour);
		Bar->SetContent(Fill);
		Bar->SetRenderTransformAngle(Angle);
		UOverlaySlot* BarSlot = X->AddChildToOverlay(Bar);
		BarSlot->SetHorizontalAlignment(HAlign_Center);
		BarSlot->SetVerticalAlignment(VAlign_Center);
	}
	UOverlaySlot* XSlot = Root->AddChildToOverlay(X);
	XSlot->SetHorizontalAlignment(HAlign_Center);
	XSlot->SetVerticalAlignment(VAlign_Center);
	Cross = X;

	HeadshotText = DHScreenStyle::MakeText(WidgetTree, TEXT("Orbitron"), 16, DHScreenStyle::Accent);
	HeadshotText->SetText(NSLOCTEXT("DowntownHunting", "Headshot", "HEADSHOT"));
	HeadshotText->SetShadowOffset(FVector2D(1, 1));
	UOverlaySlot* TextSlot = Root->AddChildToOverlay(HeadshotText);
	TextSlot->SetHorizontalAlignment(HAlign_Center);
	TextSlot->SetVerticalAlignment(VAlign_Center);
	TextSlot->SetPadding(FMargin(0, 90, 0, 0));
}

void UDHHitMarker::Show(bool bKill, bool bHeadshot)
{
	if (bKill)
	{
		KillAge = 0.f;
	}
	if (bHeadshot)
	{
		HeadshotAge = 0.f;
	}
}

void UDHHitMarker::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	using namespace DHHitMarkerSettings;
	KillAge += InDeltaTime;
	HeadshotAge += InDeltaTime;
	// Pops in big, shrinks to size, fades.
	const float Kill = FMath::Clamp(1.f - KillAge / KillSeconds, 0.f, 1.f);
	Cross->SetRenderOpacity(Kill);
	Cross->SetRenderScale(FVector2D(1.f + 0.6f * FMath::Max(0.f, 1.f - KillAge / 0.12f)));
	HeadshotText->SetRenderOpacity(FMath::Clamp((HeadshotSeconds - HeadshotAge) / 0.3f, 0.f, 1.f));
}
