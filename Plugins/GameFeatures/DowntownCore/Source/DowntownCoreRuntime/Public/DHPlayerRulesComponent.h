#pragma once

#include "Components/PawnComponent.h"
#include "Templates/SubclassOf.h"

#include "DHPlayerRulesComponent.generated.h"

class UInputAction;
class UInputComponent;
class UInputMappingContext;
class ULyraCameraMode;
class UUserWidget;

/**
 * Applies the match rules (UDHGameRulesComponent) to a pawn:
 * - server: gives the mode's loadout and puts it into the quick bar,
 * - locally controlled player: picks the camera mode (first or third person) and, if the mode allows it,
 *   lets the player switch with ToggleAction (V / D-pad up by default).
 * Added to every Lyra character by the DH experiences.
 */
UCLASS(meta = (BlueprintSpawnableComponent))
class UDHPlayerRulesComponent : public UPawnComponent
{
	GENERATED_BODY()

public:
	UDHPlayerRulesComponent(const FObjectInitializer& ObjectInitializer);

	// Server: gives the current loadout again (Gun Game swaps the weapon after a kill).
	void RefreshLoadout() { GiveLoadout(); }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	TSoftClassPtr<ULyraCameraMode> ThirdPersonMode;

	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	TSoftClassPtr<ULyraCameraMode> ThirdPersonAimMode;

	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	TSoftClassPtr<ULyraCameraMode> FirstPersonMode;

	// Used while the pawn has Status.Death (Lyra's death ability would set it, but this component decides the mode).
	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	TSoftClassPtr<ULyraCameraMode> DeathMode;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TSoftObjectPtr<UInputAction> ToggleAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TSoftObjectPtr<UInputMappingContext> InputMapping;

	// Held: live scoreboard (IA_ShowScoreboard of ShooterCore: Tab, gamepad Select).
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TSoftObjectPtr<UInputAction> ScoreboardAction;

private:
	void GiveLoadout();
	void SetUpLocalPlayer();
	void TogglePerspective();
	void ShowScoreboard();
	void HideScoreboard();
	void TrySlide();
	void TryWallJump();
	void ApplyAttachments();
	void EnableHairSkinning();
	bool IsFirstPerson() const;
	TSubclassOf<ULyraCameraMode> DetermineCameraMode() const;

	FTimerHandle RetryTimer;
	FTimerHandle HunterTimer;
	TWeakObjectPtr<UObject> ModifiedWeapon;
	TSet<TWeakObjectPtr<class USkinnedMeshComponent>> HairSkins;
	int32 Retries = 0;
	double LastWallJump = -1.0;
	bool bLoadoutGiven = false;
	bool bMappingAdded = false;
	TWeakObjectPtr<UInputComponent> BoundInputComponent;

	// Mode overlay (Gun Game, Search & Destroy), local player only.
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> GunGameHud;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> Scoreboard;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> Minimap;

	// Last choice of the local player, kept across respawns in "Choice" modes.
	static bool bPreferFirstPerson;
};
