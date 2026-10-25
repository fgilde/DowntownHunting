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

/** A special ability for the loadout (DHAbilities in DowntownCore carries it out); also used for the perks (no times). */
struct FDHAbilityEntry
{
	FString Id;
	FText Name;
	FText Description;
	float Cooldown = 0.f;  // seconds
	float Duration = 0.f;  // seconds the effect lasts, 0 for a single push
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FDHLoadoutChosen, APlayerController* /*Player*/, const TArray<FString>& /*Loadout*/);

/**
 * Weapons for the loadout builder (Config/DefaultGame.ini) and the player's loadouts, stored in GameUserSettings.ini:
 * NumLoadouts of them, one active. A loadout is an array of entries (see the E* indices): primary and secondary weapon,
 * each "Weapon" or "Weapon:Attachment" (used in modes whose rule is Loadout=Player), the special ability, a perk
 * for it and the second ability (from level SecondAbility unlocks); empty entries are allowed.
 */
UCLASS(Config = Game)
class DOWNTOWNUI_API UDHLoadoutSettings : public UObject
{
	GENERATED_BODY()

public:
	enum EEntry { Primary, Secondary, Ability, Perk, Ability2, NumEntries };
	static constexpr int32 NumLoadouts = 5;

	// Perks.
	static constexpr float QuickRechargeFactor = 0.75f;  // cooldowns
	static constexpr float LongLastingFactor = 1.5f;     // effect durations

	UFUNCTION(BlueprintPure, Category = "Loadout")
	static TArray<FDHWeaponEntry> GetWeapons();

	/** The active loadout, always NumEntries long. */
	UFUNCTION(BlueprintPure, Category = "Loadout")
	static TArray<FString> GetLoadout() { return GetLoadoutAt(GetActiveLoadout()); }

	static TArray<FString> GetLoadoutAt(int32 Index);
	static int32 GetActiveLoadout();

	/** Makes another loadout the active one and tells a running match. */
	static void SetActiveLoadout(APlayerController* Player, int32 Index);

	static TArray<FDHAbilityEntry> GetAbilities();
	static const FDHAbilityEntry* FindAbility(const FString& Id);

	/** None (empty id) first, then the perks. */
	static TArray<FDHAbilityEntry> GetPerks();

	/** None (empty id) first, then the attachments. */
	static TArray<FDHAttachmentEntry> GetAttachments();

	/** Name for an unlock id (FDHProgression): weapon, attachment, ability, perk or the second ability slot. */
	static FString DisplayName(const FString& Id);

	/** "Weapon:Attachment" -> Weapon, Attachment (empty without one). */
	static void SplitEntry(const FString& Entry, FString& OutWeapon, FString& OutAttachment);

	/** Stores the active loadout and tells a running match (UDHPlayerChoicesComponent listens). */
	UFUNCTION(BlueprintCallable, Category = "Loadout")
	static void ChooseLoadout(APlayerController* Player, const TArray<FString>& Loadout);

	static FDHLoadoutChosen OnLoadoutChosen;

private:
	UPROPERTY(Config)
	TArray<FDHWeaponEntry> Weapons;

	UPROPERTY(Config)
	TArray<FString> DefaultLoadout;
};
