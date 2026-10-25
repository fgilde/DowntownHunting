#pragma once

#include "Blueprint/UserWidget.h"

#include "DHScopeOverlay.generated.h"

class UImage;

/**
 * Sniper scope over the screen while the local player aims a scoped weapon (the first-person camera zooms in and
 * hides the weapon, UDHCameraMode_FirstPerson): black around a round lens with a crosshair. Checks every frame, fades.
 */
UCLASS()
class UDHScopeOverlay : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> LensTexture;

	float Opacity = 0.f;
	float AimSeconds = 0.f;
};
