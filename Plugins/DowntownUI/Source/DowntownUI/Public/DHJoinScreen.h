#pragma once

#include "CommonActivatableWidget.h"
#include "CommonUserTypes.h"

#include "DHJoinScreen.generated.h"

class UCommonSession_SearchResult;
class UCommonSession_SearchSessionRequest;
class UCommonUserInfo;
class UDHCardButton;
class UTextBlock;
class UVerticalBox;

/**
 * "Join a game" in the main menu style, built in code: searches LAN or online (Steam) sessions and lists them with host,
 * mode, map, players and ping; picking one joins it. Replaces Lyra's session browser.
 */
UCLASS()
class DOWNTOWNUI_API UDHJoinScreen : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UDHJoinScreen(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void HandleCardClicked(UDHCardButton* Card);
	void Search();
	void HandleSearchFinished(bool bSucceeded, const FText& Error);
	void UpdateNetworkText();

	UFUNCTION()
	void HandleUserInitialized(const UCommonUserInfo* UserInfo, bool bSuccess, FText Error, ECommonUserPrivilege RequestedPrivilege, ECommonUserOnlineContext OnlineContext);

	UPROPERTY(Transient)
	TObjectPtr<UCommonSession_SearchSessionRequest> Request;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UCommonSession_SearchResult>> Results;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UDHCardButton>> Buttons;  // switches first, then one row per result

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> List;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NetworkText;

	bool bOnline = false;
};
