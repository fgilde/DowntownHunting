#include "DHGunGameHud.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "DHGameRulesComponent.h"
#include "DHScreenStyle.h"
#include "Player/LyraPlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DHGunGameHud)

void UDHGunGameHud::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
	WidgetTree->RootWidget = Root;

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	UOverlaySlot* ColumnSlot = Root->AddChildToOverlay(Column);
	ColumnSlot->SetHorizontalAlignment(HAlign_Left);
	ColumnSlot->SetVerticalAlignment(VAlign_Top);
	ColumnSlot->SetPadding(FMargin(48, 120, 0, 0));

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>();
	Title->SetText(NSLOCTEXT("DowntownHunting", "GunGameTitle", "GUN GAME"));
	Title->SetFont(DHScreenStyle::Font(TEXT("Orbitron"), 14));
	Title->SetColorAndOpacity(DHScreenStyle::Accent);
	Column->AddChildToVerticalBox(Title);

	StepText = WidgetTree->ConstructWidget<UTextBlock>();
	StepText->SetFont(DHScreenStyle::Font(TEXT("Orbitron"), 24));
	StepText->SetColorAndOpacity(DHScreenStyle::Text);
	StepText->SetShadowOffset(FVector2D(1, 1));
	Column->AddChildToVerticalBox(StepText);

	NextText = WidgetTree->ConstructWidget<UTextBlock>();
	NextText->SetFont(DHScreenStyle::Font(TEXT("NotoSans"), 14));
	NextText->SetColorAndOpacity(DHScreenStyle::Text);
	NextText->SetShadowOffset(FVector2D(1, 1));
	Column->AddChildToVerticalBox(NextText);
}

void UDHGunGameHud::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const UDHGameRulesComponent* Rules = UDHGameRulesComponent::Find(this);
	const ALyraPlayerState* PlayerState = GetOwningPlayerState<ALyraPlayerState>();
	if (!Rules || !PlayerState || Rules->GetGunGame().IsEmpty())
	{
		return;
	}
	const TArray<FName>& Weapons = Rules->GetGunGame();
	const int32 Level = FMath::Min(UDHGameRulesComponent::GetGunGameLevel(PlayerState), Weapons.Num() - 1);
	if (Level == ShownLevel)
	{
		return;
	}
	ShownLevel = Level;
	StepText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d  %s"), Level + 1, Weapons.Num(), *Weapons[Level].ToString().ToUpper())));
	NextText->SetText(Weapons.IsValidIndex(Level + 1)
		? FText::Format(NSLOCTEXT("DowntownHunting", "GunGameNext", "Next: {0}"), FText::FromName(Weapons[Level + 1]))
		: NSLOCTEXT("DowntownHunting", "GunGameLast", "Last weapon: one kill wins"));
}
