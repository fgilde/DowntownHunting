#pragma once

#include "Components/ControllerComponent.h"
#include "UObject/SoftObjectPath.h"

#include "DHCharacterSelectComponent.generated.h"

/** One selectable character: a Lyra cosmetic part actor (City Sample player or crowd character). */
USTRUCT(BlueprintType)
struct FDHCharacterEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character")
	FString Id;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character", meta = (MetaClass = "/Script/Engine.Actor"))
	FSoftClassPath PartClass;
};

/**
 * Gives a controller its character. Players use the character chosen in the menu (stored in GameUserSettings.ini,
 * overridable with -DHCharacter=<Id> on the command line), sent to the server once; bots get a random one.
 * The character list comes from Config/DefaultGame.ini, written by Sync-Characters.cmd.
 * Works through Lyra's ULyraControllerComponent_CharacterParts on the same controller (added by the experience),
 * called by reflection because that class is not exported from LyraGame.
 */
UCLASS(Config = Game, meta = (BlueprintSpawnableComponent))
class UDHCharacterSelectComponent : public UControllerComponent
{
	GENERATED_BODY()

public:
	UDHCharacterSelectComponent(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintPure, Category = "Character")
	static TArray<FDHCharacterEntry> GetCharacters();

	/** Id chosen in the menu, empty for random. */
	UFUNCTION(BlueprintPure, Category = "Character")
	static FString GetPreferredCharacter();

	UFUNCTION(BlueprintCallable, Category = "Character")
	static void SetPreferredCharacter(const FString& Id);

	/** Switch this player's character; takes effect at the next spawn. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Character")
	void ServerSetCharacter(const FString& Id);

protected:
	virtual void BeginPlay() override;

private:
	void ApplyCharacter(const FString& Id);

	UPROPERTY(Config)
	TArray<FDHCharacterEntry> Characters;

	FString AppliedId;
};
