#include "DHAbilityHud.h"

#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "DHLoadoutSettings.h"
#include "DHPlayerChoicesComponent.h"
#include "DHScreenStyle.h"
#include "GameFramework/PlayerController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DHAbilityHud)

void UDHAbilityHud::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
	WidgetTree->RootWidget = Root;
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	UOverlaySlot* RowSlot = Root->AddChildToOverlay(Row);
	RowSlot->SetHorizontalAlignment(HAlign_Center);
	RowSlot->SetVerticalAlignment(VAlign_Bottom);
	RowSlot->SetPadding(FMargin(0, 0, 0, 96));  // above Lyra's health bar

	for (int32 Index = 0; Index < 2; ++Index)
	{
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>();
		Box->SetWidthOverride(170.f);
		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
		Box->AddChild(Column);
		Row->AddChildToHorizontalBox(Box)->SetPadding(FMargin(10, 0));

		Names[Index] = DHScreenStyle::MakeText(WidgetTree, TEXT("Orbitron"), 13, DHScreenStyle::Text);
		Names[Index]->SetShadowOffset(FVector2D(1, 1));
		Column->AddChildToVerticalBox(Names[Index]);
		Bars[Index] = WidgetTree->ConstructWidget<UProgressBar>();
		Bars[Index]->SetFillColorAndOpacity(DHScreenStyle::Accent);
		USizeBox* BarBox = WidgetTree->ConstructWidget<USizeBox>();
		BarBox->SetHeightOverride(6.f);
		BarBox->AddChild(Bars[Index]);
		Column->AddChildToVerticalBox(BarBox)->SetPadding(FMargin(0, 2));
		States[Index] = DHScreenStyle::MakeText(WidgetTree, TEXT("NotoSans"), 12, DHScreenStyle::Text);
		States[Index]->SetShadowOffset(FVector2D(1, 1));
		Column->AddChildToVerticalBox(States[Index]);
		Boxes[Index] = Box;
	}
}

void UDHAbilityHud::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const APlayerController* PC = GetOwningPlayer();
	const UDHPlayerChoicesComponent* Choices = PC ? PC->FindComponentByClass<UDHPlayerChoicesComponent>() : nullptr;
	for (int32 Index = 0; Index < 2; ++Index)
	{
		const FDHAbilityEntry* Ability = Choices ? UDHLoadoutSettings::FindAbility(Choices->GetAbility(Index)) : nullptr;
		Boxes[Index]->SetVisibility(Ability ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (!Ability)
		{
			continue;
		}
		const float Remaining = Choices->GetRemaining(Index);
		const float Cooldown = FMath::Max(0.1f, Choices->GetCooldown(Index));
		const float Active = Remaining - (Cooldown - Choices->GetDuration(Index));  // effect time left
		Names[Index]->SetText(Ability->Name);
		Bars[Index]->SetPercent(1.f - Remaining / Cooldown);
		Names[Index]->SetColorAndOpacity(Remaining <= 0.f ? DHScreenStyle::Accent : DHScreenStyle::Text);
		States[Index]->SetText(Active > 0.f ? FText::Format(NSLOCTEXT("DowntownHunting", "AbilityActive", "ACTIVE  {0}"), FText::AsNumber(FMath::CeilToInt(Active)))
			: Remaining > 0.f ? FText::Format(NSLOCTEXT("DowntownHunting", "AbilityRecharging", "{0} s"), FText::AsNumber(FMath::CeilToInt(Remaining)))
			: NSLOCTEXT("DowntownHunting", "AbilityReady", "READY"));
	}
}
