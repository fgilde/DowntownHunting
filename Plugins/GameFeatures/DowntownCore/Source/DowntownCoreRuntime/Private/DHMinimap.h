#pragma once

#include "Blueprint/UserWidget.h"

#include "DHMinimap.generated.h"

class UCanvasPanel;
class UImage;
class UTextBlock;
class UTextureRenderTarget2D;

/**
 * Minimap in the top right corner for the local player: a top-down orthographic capture of the map (taken once per map
 * over the area of the player starts, kept for later pawns), north up, centred on the player, with an arrow for the view
 * direction, dots for teammates in their team colour and the Search & Destroy bomb sites. Enemies are not shown.
 * Built in code, added to the viewport by UDHPlayerRulesComponent.
 */
UCLASS()
class UDHMinimap : public UUserWidget
{
	GENERATED_BODY()

public:
	/** This world's map capture (texture, centre and edge length in world units), false before the first one. */
	static bool GetCapture(const UWorld* World, UTexture2D*& OutTexture, FVector2D& OutCentre, float& OutExtent);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void Capture();
	// World XY to a position in the widget (pixels from the top left).
	FVector2D ToWidget(const FVector& World) const;
	UImage* MakeMarker(UTexture2D* Texture, float Size);

	UPROPERTY()
	TObjectPtr<UImage> MapImage;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> Markers;

	UPROPERTY()
	TObjectPtr<UImage> Arrow;

	UPROPERTY()
	TArray<TObjectPtr<UImage>> Dots;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> SiteLabels;

	UPROPERTY()
	TObjectPtr<UTexture2D> DotTexture;

	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> Target;

	UPROPERTY()
	TObjectPtr<UTexture2D> MapTexture;

	// Captured area: centre and edge length (world units), and the region shown now (UV).
	FVector2D Centre = FVector2D::ZeroVector;
	float Extent = 0.f;
	FVector2D ViewOrigin = FVector2D::ZeroVector;
	float CaptureDelay = 3.f;
	int32 Captures = 0;
	float RadarPulse = 0.f;
};
