#pragma once

#include "UObject/Object.h"
#include "UObject/SoftObjectPath.h"

#include "DHLoadoutSettings.generated.h"

class APlayerController;

/** A weapon the loadout builder offers. Id matches the Loadout rule names (UDHGameRulesComponent). */
USTRUCT(BlueprintType)
struct DOWNTOWNUI_API FDHWeaponEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout")
	FString Id;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loadout", meta = (AllowedClasses = "/Script/Engine.Texture2D"))
	FSoftObjectPath Icon;
};

/** An attachment the loadout builder offers; Id is what the server understands (UDHPlayerChoicesComponent). */
struct FDHAttachmentEntry
{
	FString Id;
	FText Name;
	FText Description;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FDHLoadoutChosen, APlayerController* /*Player*/, const TArray<FString>& /*Loadout*/);

/**
 * Weapons for the loadout builder (Config/DefaultGame.ini) and the player's loadout: primary and secondary weapon,
 * each with an optional attachment, stored in GameUserSettings.ini. Used in modes whose rule is Loadout=Player.
 */
UCLASS(Config = Game)
class DOWNTOWNUI_API UDHLoadoutSettings : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Loadout")
	static TArray<FDHWeaponEntry> GetWeapons();

	/** Primary, secondary; each "Weapon" or "Weapon:Attachment". */
	UFUNCTION(BlueprintPure, Category = "Loadout")
	static TArray<FString> GetLoadout();

	/** None (empty id) first, then the attachments. */
	static TArray<FDHAttachmentEntry> GetAttachments();

	/** "Weapon:Attachment" -> Weapon, Attachment (empty without one). */
	static void SplitEntry(const FString& Entry, FString& OutWeapon, FString& OutAttachment);

	/** Stores the loadout and tells a running match (UDHPlayerChoicesComponent listens). */
	UFUNCTION(BlueprintCallable, Category = "Loadout")
	static void ChooseLoadout(APlayerController* Player, const TArray<FString>& Loadout);

	static FDHLoadoutChosen OnLoadoutChosen;

private:
	UPROPERTY(Config)
	TArray<FDHWeaponEntry> Weapons;

	UPROPERTY(Config)
	TArray<FString> DefaultLoadout;
};
