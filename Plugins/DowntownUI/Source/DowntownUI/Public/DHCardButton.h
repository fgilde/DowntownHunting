#pragma once

#include "Components/Button.h"

#include "DHCardButton.generated.h"

class UDHCardButton;

DECLARE_DELEGATE_OneParam(FDHCardClicked, UDHCardButton* /*Card*/);

/**
 * Button of the menu screens' cards that says which card was clicked (UButton's OnClicked has no sender,
 * and guessing via hover picked the card under the mouse when a gamepad pressed A).
 * Arcade look: slanted, a neon outline and a springy pop when focused or hovered (Animate, called by the screen's
 * tick), a blip on focus and a punch on click.
 */
UCLASS()
class UDHCardButton : public UButton
{
	GENERATED_BODY()

public:
	UDHCardButton(const FObjectInitializer& ObjectInitializer);

	FString CardId;
	int32 CardSlot = 0;
	bool bAttachment = false;
	FDHCardClicked OnCardClicked;

	/** Pop size when active; 0 keeps the button still (it then only outlines and sounds). */
	float PopScale = 0.07f;

	/** Springs towards the focused/hovered state; plays the focus sound when it becomes active. */
	void Animate(float DeltaTime);

	bool IsActive() const { return HasAnyUserFocus() || IsHovered(); }

	/** Lyra's menu blip or click, a little randomly pitched. */
	static void PlayMenuSound(const UObject* Context, bool bClick);

	virtual void SynchronizeProperties() override;

private:
	UFUNCTION()
	void HandleClicked();

	void SetOutline(bool bOn);

	float Pop = 0.f;
	float PopVelocity = 0.f;
	bool bWasActive = false;
};
