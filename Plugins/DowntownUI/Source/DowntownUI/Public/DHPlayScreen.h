#pragma once

#include "CommonActivatableWidget.h"
#include "CommonUserTypes.h"

#include "DHPlayScreen.generated.h"

class UDHCardButton;
class UTextBlock;
class UCommonUserInfo;

/**
 * "Play" screen in the main menu style, built in code. The entries (Lyra user-facing experiences shown in the front end,
 * written by Tools/Python/sync_modes.py from Modes.json: one per mode and map) are offered in two rows: the modes
 * (each once), and below the maps that have the selected mode. Picking a mode selects it, picking a map hosts the
 * match. The focused card's description, network (offline, LAN, online) and bots switches, "Join a game" (UDHJoinScreen). Lyra's types are reached by reflection, this plugin does not link LyraGame.
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
	UDHCardButton* MakeCard(const FMode& Entry, const FText& Caption, float Width, const FString& Id, int32 CardIndex);
	void SelectMode(int32 Kind);
	void Host(UObject* Definition);
	void UpdateSwitches();

	UFUNCTION()
	void HandleUserInitialized(const UCommonUserInfo* UserInfo, bool bSuccess, FText Error, ECommonUserPrivilege RequestedPrivilege, ECommonUserOnlineContext OnlineContext);

	TArray<FMode> Modes;           // every entry (mode on a map), Modes.json order
	TArray<FString> Kinds;         // mode titles, each once
	TArray<TObjectPtr<UDHCardButton>> KindButtons;
	TArray<TObjectPtr<UDHCardButton>> MapButtons;
	int32 SelectedKind = 0;

	UPROPERTY(Transient)
	TObjectPtr<class UHorizontalBox> MapRow;
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
