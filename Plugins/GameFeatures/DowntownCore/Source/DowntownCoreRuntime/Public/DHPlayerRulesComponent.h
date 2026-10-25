#pragma once

#include "Components/PawnComponent.h"
#include "Templates/SubclassOf.h"
#include "DHAbilities.h"

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
 *   lets the player switch with ToggleAction (V / D-pad up by default); the loadout's abilities on Lyra's dash input
 *   and on Ability2Action (DHAbilities), with their HUD,
 * - every machine: speed (sprint, killstreak hunter, infected, adrenaline) and cloak,
 * - developer mode (FDHProgression::DevFlag) on the host's own pawn: god mode and endless ammo.
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

	// Sprint on or off (DHMovement): set by the owning client's input, and on the server through its RPC.
	void SetSprinting(bool bOn);
	bool IsSprinting() const { return bSprinting; }

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

	// First ability: Lyra's dash input (its own dash ability is taken away); second ability (RB by default).
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TSoftObjectPtr<UInputAction> AbilityAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TSoftObjectPtr<UInputAction> Ability2Action;

	// Sprint: held on the keyboard (Shift), a toggle on the gamepad (left stick press) that ends by itself.
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TSoftObjectPtr<UInputAction> SprintAction;

private:
	void GiveLoadout();
	void SetUpLocalPlayer();
	void TogglePerspective();
	void ShowScoreboard();
	void HideScoreboard();
	void TrySlide();
	void TryWallJump();
	void HandleSprintPressed();
	void HandleSprintReleased();
	void SetSprintingLocal(bool bOn);
	void CheckSprint();
	void UpdateMoveSpeed();
	void ApplyDevCheats();
	void UpdateMoveAnims();
	bool IsGamepadInput() const;
	void UseAbility1() { UseAbility(0); }
	void UseAbility2() { UseAbility(1); }
	void UseAbility(int32 Slot);
	void PrepareLyraDash();
	void HandleAbilityActivated(class UGameplayAbility* Ability);
	void ApplyAttachments();
	void EnableHairSkinning();
	bool HasScope() const;
	bool IsFirstPerson() const;
	TSubclassOf<ULyraCameraMode> DetermineCameraMode() const;

	FTimerHandle RetryTimer;
	FTimerHandle HunterTimer;
	FTimerHandle SprintTimer;
	FTimerHandle MoveAnimTimer;
	uint8 MoveAnim = 0;
	float StepDistance = 0.f;  // 0 Lyra's locomotion, 1 sprint, 2 slide
	TWeakObjectPtr<class UAnimMontage> MoveMontage;
	double SlideStartedAt = -100.0;  // owning client
	bool bSprinting = false;
	bool bSprintWanted = false;  // owning client: key held, or the gamepad toggle waiting for the player to move
	double SprintWantedAt = 0.0;
	bool bDevGod = false;
	TWeakObjectPtr<UObject> ModifiedWeapon;
	TWeakObjectPtr<const UObject> LoggedWeapon;
	TSet<TWeakObjectPtr<class USkinnedMeshComponent>> HairSkins;
	FDHCloakState CloakState;
	int32 Retries = 0;
	double LastWallJump = -1.0;
	bool bLoadoutGiven = false;
	bool bMappingAdded = false;
	TWeakObjectPtr<UInputComponent> BoundInputComponent;
	TWeakObjectPtr<class UAbilitySystemComponent> FireSource;  // server: sniper shots get their heavy report
	FDelegateHandle FireHandle;

	// Mode overlay (Gun Game, Search & Destroy), local player only.
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> GunGameHud;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> Scoreboard;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> Minimap;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> ScopeOverlay;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> AbilityHud;

	// Last choice of the local player, kept across respawns in "Choice" modes.
	static bool bPreferFirstPerson;
};
