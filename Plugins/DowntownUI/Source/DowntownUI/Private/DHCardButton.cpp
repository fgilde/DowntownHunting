#include "DHCardButton.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "DHScreenStyle.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DHCardButton)

namespace
{
	constexpr float Slant = -6.f;  // degrees, every card leans forward
}

UDHCardButton::UDHCardButton(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetOutline(false);
	SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	FWidgetTransform Transform;
	Transform.Shear = FVector2D(Slant, 0.f);
	SetRenderTransform(Transform);
}

void UDHCardButton::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	OnClicked.AddUniqueDynamic(this, &ThisClass::HandleClicked);
}

void UDHCardButton::SetOutline(bool bOn)
{
	// Sharp corners, white fill tinted by the background colour; the outline shows focus for gamepad and mouse alike.
	FButtonStyle Style = GetStyle();
	const FSlateRoundedBoxBrush Brush(FLinearColor::White, 3.f, DHScreenStyle::Accent, bOn ? 3.f : 0.f);
	Style.SetNormal(Brush).SetHovered(Brush).SetPressed(FSlateRoundedBoxBrush(FLinearColor(1.f, 1.f, 1.f, 0.8f), 3.f, DHScreenStyle::Cyan, 3.f));
	SetStyle(Style);
}

void UDHCardButton::Animate(float DeltaTime)
{
	const bool bActive = IsActive();
	if (bActive != bWasActive)
	{
		bWasActive = bActive;
		SetOutline(bActive && GetBackgroundColor().A > 0.f);
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
	FWidgetTransform Transform = GetRenderTransform();  // keeps a screen's own translation
	Transform.Shear = FVector2D(Slant - 3.f * Pop * (PopScale > 0.f), 0.f);
	Transform.Scale = FVector2D(1.f + PopScale * Pop);
	Transform.Angle = -1.2f * Pop * (PopScale > 0.f);
	SetRenderTransform(Transform);
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
