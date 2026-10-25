#pragma once

#include "Components/Button.h"
#include "Styling/SlateBrush.h"

#include "DHCardButton.generated.h"

class SBorder;
class UDHCardButton;

DECLARE_DELEGATE_OneParam(FDHCardClicked, UDHCardButton* /*Card*/);

/**
 * Button of the menu screens' cards that says which card was clicked (UButton's OnClicked has no sender,
 * and guessing via hover picked the card under the mouse when a gamepad pressed A).
 * Arcade look: slanted, a neon outline and a springy pop when focused or hovered (Animate, called by the screen's
 * tick), a blip on focus and a punch on click.
 * The look lives on an inner face (background colour, outline, slant and pop); the button itself stays an upright
 * rectangle, because gamepad navigation picks neighbours by the render bounds and a sheared or popped button reached
 * past its neighbour, so the D-pad skipped a card.
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
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void OnSlotAdded(UPanelSlot* InSlot) override;

private:
	UFUNCTION()
	void HandleClicked();

	/** Puts the button's content into the face and moves the button's paddings onto it. */
	void WrapContent();
	const FSlateBrush* FaceBrush() const;

	TSharedPtr<SBorder> Face;
	FSlateBrush PlainBrush;
	FSlateBrush OutlineBrush;
	FSlateBrush PressedBrush;
	bool bOutline = false;
	float Pop = 0.f;
	float PopVelocity = 0.f;
	bool bWasActive = false;
};
