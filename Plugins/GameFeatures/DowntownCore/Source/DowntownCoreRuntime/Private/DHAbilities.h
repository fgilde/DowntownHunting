#pragma once

#include "CoreMinimal.h"

class ACharacter;
class APawn;
class APlayerState;
class UMaterialInterface;
class UMeshComponent;
class UPrimitiveComponent;

/** What UpdateCloak changed on one pawn (on this machine), to undo it. */
struct FDHCloakState
{
	TArray<TWeakObjectPtr<UPrimitiveComponent>> Hidden;
	TMap<TWeakObjectPtr<UMeshComponent>, TArray<UMaterialInterface*>> Swapped;  // original materials (assets)
	bool bHiddenFromBots = false;
	bool bWasCloaked = false;
};

/**
 * The special abilities of the loadout (catalogue with cooldowns and durations: UDHLoadoutSettings::GetAbilities),
 * used through UDHPlayerChoicesComponent::UseAbility:
 * - Dash, Leap: a push, run on the owning client (instant feel) and on the server with the same direction.
 * - Adrenaline: faster running for a while (applied with the other speed factors by UDHPlayerRulesComponent).
 * - Recon: the killstreak radar for a while.
 * - Cloak: the pawn is hidden from other players and the bots' sight until it runs out or its owner attacks.
 * The timed ones are stored as server times in replicated stat tags on the Lyra player state, like the killstreaks.
 */
namespace DHAbilities
{
	constexpr float AdrenalineSpeed = 1.5f;

	/** Owning client and server: the push of Dash and Leap (nothing for the others). */
	void Push(ACharacter* Character, const FString& Id, const FVector& Direction);

	/** Server: starts a timed ability for Seconds. */
	void Start(APawn* Pawn, const FString& Id, float Seconds);

	/** Server: ends the cloak early (its owner attacked). */
	void EndCloak(APlayerState* Player);

	bool HasAdrenaline(const APlayerState* Player);
	bool IsCloaked(const APlayerState* Player);

	/**
	 * Every machine, every pawn, a few times a second: a cloaked pawn (with its weapon and character parts) is hidden
	 * from enemies; its owner (also the first-person arms) and team see it as a faint cyan shimmer (M_DH_Cloak), so they
	 * know it is cloaked. A dissolve effect where it vanishes and reappears. On the server also hidden from the bots.
	 */
	void UpdateCloak(APawn* Pawn, FDHCloakState& State);
}
