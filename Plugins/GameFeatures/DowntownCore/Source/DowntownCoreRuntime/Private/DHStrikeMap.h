#pragma once

#include "CommonActivatableWidget.h"

#include "DHStrikeMap.generated.h"

class APlayerController;
class UCanvasPanel;
class UImage;
class USizeBox;
class UTexture2D;

/**
 * Picking the spot of a strike killstreak (Mortar, Airstrike, Carpet Bomb) on the whole map, CoD style: the minimap's
 * capture big in the middle, the own position, teammates, and a target circle as large as the strike. Mouse: point and
 * click (right click cancels); gamepad: left stick moves the circle, A calls it in, B cancels. Then
 * UDHPlayerChoicesComponent::ServerUseStreak. Without a map capture (hardcore has no minimap) the strike goes where
 * the player aims.
 */
UCLASS()
class UDHStrikeMap : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UDHStrikeMap(const FObjectInitializer& ObjectInitializer);

	static void Open(APlayerController* Player, const FString& StreakId);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnAnalogValueChanged(const FGeometry& InGeometry, const FAnalogInputEvent& InAnalogEvent) override;

private:
	void Confirm();
	// World XY <-> position on the map (0..1, top left).
	FVector2D ToMap(const FVector& World) const;
	FVector ToWorld(const FVector2D& Map) const;
	UImage* AddMarker(UTexture2D* Texture, float Size, const FLinearColor& Colour);
	void Place(UImage* Marker, const FVector2D& Map, float Size);

	FString StreakId;
	FVector2D Cursor = FVector2D(0.5f, 0.5f);
	FVector2D Stick = FVector2D::ZeroVector;
	FVector2D Centre = FVector2D::ZeroVector;
	float Extent = 1.f;
	float Age = 0.f;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> MapBox;

	UPROPERTY(Transient)
	TObjectPtr<UImage> MapImage;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> Markers;

	UPROPERTY(Transient)
	TObjectPtr<UImage> TargetRing;

	UPROPERTY(Transient)
	TObjectPtr<UImage> TargetDot;

	UPROPERTY(Transient)
	TObjectPtr<UImage> SelfDot;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> MateDots;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> DotTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> RingTexture;
};
