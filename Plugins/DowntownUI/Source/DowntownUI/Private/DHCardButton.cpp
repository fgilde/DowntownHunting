#include "DHCardButton.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/ButtonSlot.h"
#include "DHScreenStyle.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DHCardButton)

namespace
{
	constexpr float Slant = -6.f;  // degrees, every card leans forward

	FSlateRenderTransform FaceTransform(float Pop, float PopScale)
	{
		FWidgetTransform Transform;
		Transform.Shear = FVector2D(Slant - 3.f * Pop * (PopScale > 0.f), 0.f);
		Transform.Scale = FVector2D(1.f + PopScale * Pop);
		Transform.Angle = -1.2f * Pop * (PopScale > 0.f);
		return Transform.ToSlateRenderTransform();
	}
}

UDHCardButton::UDHCardButton(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, PlainBrush(FSlateRoundedBoxBrush(FLinearColor::White, 3.f))
	, OutlineBrush(FSlateRoundedBoxBrush(FLinearColor::White, 3.f, DHScreenStyle::Accent, 3.f))
	, PressedBrush(FSlateRoundedBoxBrush(FLinearColor(1.f, 1.f, 1.f, 0.8f), 3.f, DHScreenStyle::Cyan, 3.f))
{
	// The button draws nothing itself; the face does (sharp corners, white fill tinted by the background colour).
	FButtonStyle Style = GetStyle();
	const FSlateNoResource None;
	Style.SetNormal(None).SetHovered(None).SetPressed(None).SetDisabled(None);
	SetStyle(Style);
}

void UDHCardButton::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	OnClicked.AddUniqueDynamic(this, &ThisClass::HandleClicked);
}

TSharedRef<SWidget> UDHCardButton::RebuildWidget()
{
	TSharedRef<SWidget> Result = Super::RebuildWidget();
	WrapContent();
	return Result;
}

void UDHCardButton::OnSlotAdded(UPanelSlot* InSlot)
{
	Super::OnSlotAdded(InSlot);
	WrapContent();
}

void UDHCardButton::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	Face.Reset();
}

void UDHCardButton::WrapContent()
{
	const UButtonSlot* ContentSlot = Cast<UButtonSlot>(GetContentSlot());
	if (!MyButton.IsValid() || !ContentSlot || (Face.IsValid() && MyButton->GetContent() == Face.ToSharedRef()))
	{
		return;
	}
	const TSharedRef<SWidget> Content = MyButton->GetContent();
	TWeakObjectPtr<UDHCardButton> WeakThis(this);
	SAssignNew(Face, SBorder)
		.BorderImage_UObject(this, &ThisClass::FaceBrush)
		.BorderBackgroundColor_Lambda([WeakThis]() { return FSlateColor(WeakThis.IsValid() ? WeakThis->GetBackgroundColor() : FLinearColor::Transparent); })
		.Padding(GetStyle().NormalPadding + ContentSlot->GetPadding())
		.HAlign(ContentSlot->GetHorizontalAlignment())
		.VAlign(ContentSlot->GetVerticalAlignment())
		[
			Content
		];
	Face->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	Face->SetRenderTransform(FaceTransform(Pop, PopScale));
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	MyButton->SetContentPadding(FMargin(0.f));
	MyButton->SetHAlign(HAlign_Fill);
	MyButton->SetVAlign(VAlign_Fill);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	MyButton->SetContent(Face.ToSharedRef());
}

const FSlateBrush* UDHCardButton::FaceBrush() const
{
	if (MyButton.IsValid() && MyButton->IsPressed())
	{
		return &PressedBrush;
	}
	return bOutline && GetBackgroundColor().A > 0.f ? &OutlineBrush : &PlainBrush;
}

void UDHCardButton::Animate(float DeltaTime)
{
	const bool bActive = IsActive();
	if (bActive != bWasActive)
	{
		bWasActive = bActive;
		bOutline = bActive;
		if (bActive)
		{
			PlayMenuSound(this, false);
			PopVelocity += 6.f;  // kick, so it overshoots
		}
	}
	// Underdamped spring: bounces a little past the target.
	const float Step = FMath::Min(DeltaTime, 1.f / 30.f);
	PopVelocity += ((bActive ? 1.f : 0.f) - Pop) * 380.f * Step - PopVelocity * 16.f * Step;
	Pop += PopVelocity * Step;
	if (Face.IsValid())
	{
		Face->SetRenderTransform(FaceTransform(Pop, PopScale));
	}
}

void UDHCardButton::PlayMenuSound(const UObject* Context, bool bClick)
{
	const TCHAR* Path = bClick
		? TEXT("/Game/Audio/Sounds/UI/sfx_UI_MainMenu_Select_nl_meta_Preset.sfx_UI_MainMenu_Select_nl_meta_Preset")
		: TEXT("/Game/Audio/Sounds/UI/sfx_UI_MainMenu_Highlight_nl_meta_Preset.sfx_UI_MainMenu_Highlight_nl_meta_Preset");
	if (USoundBase* Sound = LoadObject<USoundBase>(nullptr, Path))
	{
		UGameplayStatics::PlaySound2D(Context, Sound, bClick ? 1.f : 0.7f, FMath::FRandRange(0.92f, 1.12f));
	}
}

void UDHCardButton::HandleClicked()
{
	PlayMenuSound(this, true);
	PopVelocity -= 10.f;  // squash, then spring back
	OnCardClicked.ExecuteIfBound(this);
}
