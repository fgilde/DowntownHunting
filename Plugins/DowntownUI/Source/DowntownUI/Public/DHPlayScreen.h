#pragma once

#include "CommonActivatableWidget.h"
#include "CommonUserTypes.h"

#include "DHPlayScreen.generated.h"

class UDHCardButton;
class UTextBlock;
class UCommonUserInfo;

/**
 * "Play" screen in the main menu style, built in code: every mode (Lyra user-facing experience shown in the front end,
 * written by Tools/Python/sync_modes.py from Modes.json) as a card with its tile, the focused mode's description,
 * network (offline, LAN, online) and bots switches, and "Join a game" (Lyra's session browser).
 * Picking a card hosts the match. Lyra's types are reached by reflection, this plugin does not link LyraGame.
 */
UCLASS()
class DOWNTOWNUI_API UDHPlayScreen : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UDHPlayScreen(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeOnInitialized() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	struct FMode
	{
		TObjectPtr<UObject> Definition;  // ULyraUserFacingExperienceDefinition
		TObjectPtr<UDHCardButton> Button;
		FText Title;
		FText Subtitle;
		FText Description;
	};

	void HandleCardClicked(UDHCardButton* Card);
	void Host(UObject* Definition);
	void UpdateSwitches();

	UFUNCTION()
	void HandleUserInitialized(const UCommonUserInfo* UserInfo, bool bSuccess, FText Error, ECommonUserPrivilege RequestedPrivilege, ECommonUserOnlineContext OnlineContext);

	TArray<FMode> Modes;
	TArray<TObjectPtr<UDHCardButton>> Switches;  // network, bots, join

	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> KeepAlive;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DescriptionText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NetworkText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BotsText;

	UPROPERTY(Transient)
	TObjectPtr<UObject> PendingOnline;  // mode waiting for the online login

	int32 Network = 0;  // ECommonSessionOnlineMode: offline, LAN, online
	bool bBots = true;
};
