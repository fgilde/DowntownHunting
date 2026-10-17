#pragma once

#include "UObject/Object.h"
#include "UObject/SoftObjectPath.h"

#include "DHCharacterSettings.generated.h"

class APlayerController;

/** One selectable character: a Lyra cosmetic part actor (City Sample player or crowd character). */
USTRUCT(BlueprintType)
struct DOWNTOWNUI_API FDHCharacterEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character")
	FString Id;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character", meta = (MetaClass = "/Script/Engine.Actor"))
	FSoftClassPath PartClass;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FDHCharacterChosen, APlayerController* /*Player*/, const FString& /*Id*/);

/**
 * Character list (Config/DefaultGame.ini, written by Sync-Characters.cmd) and the player's choice
 * (GameUserSettings.ini, overridable with -DHCharacter=<Id>). Lives outside the DowntownCore game feature
 * because the main menu needs it before any experience is loaded.
 */
UCLASS(Config = Game)
class DOWNTOWNUI_API UDHCharacterSettings : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Character")
	static TArray<FDHCharacterEntry> GetCharacters();

	/** Id chosen in the menu, empty for random. */
	UFUNCTION(BlueprintPure, Category = "Character")
	static FString GetPreferredCharacter();

	/** Stores the choice and tells a running match (UDHCharacterSelectComponent listens). */
	UFUNCTION(BlueprintCallable, Category = "Character")
	static void ChooseCharacter(APlayerController* Player, const FString& Id);

	static FDHCharacterChosen OnCharacterChosen;

private:
	UPROPERTY(Config)
	TArray<FDHCharacterEntry> Characters;
};
