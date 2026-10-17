#pragma once

#include "Components/GameStateComponent.h"

#include "DHGameRulesComponent.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogDHRules, Log, All);

class AController;

UENUM(BlueprintType)
enum class EDHPerspectiveRule : uint8
{
	ThirdPerson,
	FirstPerson,
	// Each player switches between first and third person.
	Choice,
};

/**
 * Match rules of a Downtown Hunting mode, read on the server from the travel URL options that the mode's
 * playlist entry puts there (ExtraArgs), and replicated to clients:
 *   Perspective=TP|FP|Choice   WeaponSwitch=1|0   Loadout=Pistol,Rifle,Shotgun,AK47,MP5,M1911 | Player
 * Loadout=Player: each player brings the loadout from the loadout builder (UDHPlayerChoicesComponent), bots get a random primary + pistol.
 * Added to the game state by every DH experience.
 */
UCLASS(meta = (BlueprintSpawnableComponent))
class UDHGameRulesComponent : public UGameStateComponent
{
	GENERATED_BODY()

public:
	UDHGameRulesComponent(const FObjectInitializer& ObjectInitializer);

	static const UDHGameRulesComponent* Find(const UObject* WorldContextObject);

	EDHPerspectiveRule GetPerspectiveRule() const { return Perspective; }
	bool AllowsWeaponSwitch() const { return bWeaponSwitch; }

	// Server only. Item definition classes to give the controller's pawn, in quick bar order.
	TArray<UClass*> GetLoadoutItems(const AController* Controller) const;

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Loadout names (URL option) to inventory item definitions.
	UPROPERTY(EditDefaultsOnly, Category = "Rules")
	TMap<FName, TSoftClassPtr<UObject>> WeaponItems;

	UPROPERTY(Replicated)
	EDHPerspectiveRule Perspective = EDHPerspectiveRule::ThirdPerson;

	UPROPERTY(Replicated)
	bool bWeaponSwitch = true;

	TArray<FName> Loadout;
	bool bPlayerLoadout = false;
};
