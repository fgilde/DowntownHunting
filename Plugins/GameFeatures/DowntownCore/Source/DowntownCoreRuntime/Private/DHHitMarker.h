#pragma once

#include "Blueprint/UserWidget.h"

#include "DHHitMarker.generated.h"

class UTextBlock;
class UWidget;

/**
 * Shooter feedback at the crosshair on top of Lyra's hit markers and sounds: a red X for a kill, "HEADSHOT" for a
 * hit to the head (decided on the server, UDHGameRulesComponent::OnDamage). Kept per player, shown again on each event.
 */
UCLASS()
class UDHHitMarker : public UUserWidget
{
	GENERATED_BODY()

public:
	void Show(bool bKill, bool bHeadshot);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UPROPERTY()
	TObjectPtr<UWidget> Cross;

	UPROPERTY()
	TObjectPtr<UTextBlock> HeadshotText;

	float KillAge = 10.f;
	float HeadshotAge = 10.f;
};
