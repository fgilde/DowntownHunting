#include "DHStreakBanner.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "DHScreenStyle.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DHStreakBanner)

void UDHStreakBanner::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
	WidgetTree->RootWidget = Root;
	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetBrushColor(DHScreenStyle::Card);
	Panel->SetPadding(FMargin(26, 8));
	UOverlaySlot* PanelSlot = Root->AddChildToOverlay(Panel);
	PanelSlot->SetHorizontalAlignment(HAlign_Center);
	PanelSlot->SetVerticalAlignment(VAlign_Top);
	PanelSlot->SetPadding(FMargin(0, 120, 0, 0));
	Label = DHScreenStyle::MakeText(WidgetTree, TEXT("Orbitron"), 22, DHScreenStyle::Text);
	Panel->SetContent(Label);
}

void UDHStreakBanner::Show(const FText& Text, bool bReward)
{
	Label->SetText(Text);
	Label->SetColorAndOpacity(bReward ? DHScreenStyle::Accent : DHScreenStyle::Text);
	Seconds = bReward ? 3.5f : 2.f;
	Age = 0.f;
}

void UDHStreakBanner::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Age += InDeltaTime;
	SetRenderOpacity(FMath::Clamp(FMath::Min(Age / 0.2f, (Seconds - Age) / 0.5f), 0.f, 1.f));
	SetRenderScale(FVector2D(1.f + 0.15f * FMath::Max(0.f, 1.f - Age / 0.25f)));
	if (Age > Seconds)
	{
		RemoveFromParent();
	}
}
