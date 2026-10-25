#include "DHScopeOverlay.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "DHControllerScreen.h"
#include "DHScreenStyle.h"
#include "DHWeaponAccess.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Texture2D.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DHScopeOverlay)

namespace DHScope
{
	constexpr int32 Size = 1024;

	// Black square with a round hole, a soft dark rim, crosshair lines and a centre dot.
	UTexture2D* MakeLens()
	{
		UTexture2D* Texture = UTexture2D::CreateTransient(Size, Size, PF_B8G8R8A8);
		FColor* Pixels = static_cast<FColor*>(Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE));
		const float Centre = Size / 2.f;
		for (int32 Y = 0; Y < Size; ++Y)
		{
			for (int32 X = 0; X < Size; ++X)
			{
				const float DX = X + 0.5f - Centre, DY = Y + 0.5f - Centre;
				const float R = FMath::Sqrt(DX * DX + DY * DY) / Centre;
				float Alpha = FMath::Clamp((R - 0.80f) / 0.12f, 0.f, 1.f);  // rim darkens towards the edge
				const bool bLine = (FMath::Abs(DX) < 1.6f || FMath::Abs(DY) < 1.6f) && R < 0.9f && R > 0.012f;
				const bool bThick = (FMath::Abs(DX) < 4.f || FMath::Abs(DY) < 4.f) && R > 0.55f && R < 0.9f;
				if (bLine || bThick || R < 0.006f)
				{
					Alpha = FMath::Max(Alpha, 0.9f);
				}
				Pixels[Y * Size + X] = FColor(0, 0, 0, uint8(Alpha * 255.f));
			}
		}
		Texture->GetPlatformData()->Mips[0].BulkData.Unlock();
		Texture->UpdateResource();
		return Texture;
	}
}

void UDHScopeOverlay::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	SetRenderOpacity(0.f);
	LensTexture = DHScope::MakeLens();

	// Lens square in the middle (screen height), black on both sides.
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
	WidgetTree->RootWidget = Root;
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	UOverlaySlot* RowSlot = Root->AddChildToOverlay(Row);
	RowSlot->SetHorizontalAlignment(HAlign_Fill);
	RowSlot->SetVerticalAlignment(VAlign_Fill);
	auto Side = [this, Row]()
	{
		UBorder* Black = WidgetTree->ConstructWidget<UBorder>();
		Black->SetBrushColor(FLinearColor::Black);
		UHorizontalBoxSlot* SideSlot = Row->AddChildToHorizontalBox(Black);
		SideSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	};
	Side();
	UScaleBox* Fit = WidgetTree->ConstructWidget<UScaleBox>();
	Fit->SetStretch(EStretch::ScaleToFit);
	UImage* Lens = WidgetTree->ConstructWidget<UImage>();
	Lens->SetBrushFromTexture(LensTexture, true);
	Fit->AddChild(Lens);
	UHorizontalBoxSlot* LensSlot = Row->AddChildToHorizontalBox(Fit);
	LensSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	Side();

	// How to steady the aim (UDHPlayerRulesComponent: the sprint input holds the breath).
	BreathHint = DHScreenStyle::MakeText(WidgetTree, TEXT("Orbitron"), 15, DHScreenStyle::Accent);
	UOverlaySlot* HintSlot = Root->AddChildToOverlay(BreathHint);
	HintSlot->SetHorizontalAlignment(HAlign_Center);
	HintSlot->SetVerticalAlignment(VAlign_Bottom);
	HintSlot->SetPadding(FMargin(0, 0, 0, 40));
}

void UDHScopeOverlay::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	static const FGameplayTag AimTag = FGameplayTag::RequestGameplayTag(TEXT("Event.Movement.ADS"));
	const APawn* Pawn = GetOwningPlayerPawn();
	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn);
	const UObject* Weapon = DHWeaponAccess::ActiveWeapon(Pawn);
	const bool bAiming = ASC && ASC->HasMatchingGameplayTag(AimTag) && Weapon && Weapon->GetClass()->GetName().Contains(TEXT("Sniper"));
	// The scope comes up once the view has zoomed most of the way.
	AimSeconds = bAiming ? AimSeconds + InDeltaTime : 0.f;
	Opacity = FMath::FInterpConstantTo(Opacity, AimSeconds > 0.12f ? 1.f : 0.f, InDeltaTime, 8.f);
	SetRenderOpacity(Opacity);
	if (bAiming && AimSeconds - InDeltaTime <= 0.f)
	{
		const APlayerController* PC = GetOwningPlayer();
		BreathHint->SetText(FText::Format(NSLOCTEXT("DowntownHunting", "ScopeBreath", "HOLD BREATH  {0}"),
			UDHControllerScreen::ActionKeyLabel(PC ? PC->GetLocalPlayer() : nullptr, TEXT("/DowntownCore/Input/IA_DH_Sprint.IA_DH_Sprint"), /*bShort=*/ true)));
	}
}
