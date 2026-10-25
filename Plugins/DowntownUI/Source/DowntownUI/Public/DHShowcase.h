#pragma once

#include "Blueprint/UserWidget.h"

#include "DHShowcase.generated.h"

class UBorder;
class UImage;
class UMediaPlayer;
class USizeBox;
class UTextBlock;
class UTexture2D;

/**
 * Main menu stage on the left, built in code: a comic panel that springs in from the left with a whoosh whenever it
 * shows something new, over a slanted colour slab and a huge outlined word drifting behind it. Shows a hunter (splash
 * art with its looping clip, Content/Movies/Characters) or a wide picture (weapon or controller art, /Game/DH/UI/MenuArt).
 */
UCLASS()
class DOWNTOWNUI_API UDHShowcase : public UUserWidget
{
	GENERATED_BODY()

public:
	/** A hunter by id ("" = the random silhouette). */
	void ShowCharacter(const FString& Id, const FText& Word, const FLinearColor& SlabColour);

	/** A wide picture with a caption. */
	void ShowArt(UTexture2D* Art, const FText& Caption, const FText& Word, const FLinearColor& SlabColour);

	/** Stops the clip (the menu was left). */
	void Stop();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void Present(UTexture2D* Picture, const FString& Clip, const FText& Caption, const FText& Word, const FLinearColor& SlabColour, bool bWide);

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> Panel;

	UPROPERTY(Transient)
	TObjectPtr<UImage> Picture;

	UPROPERTY(Transient)
	TObjectPtr<UImage> Video;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Caption;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> Slab;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Word;

	UPROPERTY(Transient)
	TObjectPtr<UMediaPlayer> Player;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> KeepAlive;

	FString Shown;
	float Offset = -900.f;
	float Velocity = 0.f;
	float VideoFade = 0.f;
	float Time = 0.f;
};
