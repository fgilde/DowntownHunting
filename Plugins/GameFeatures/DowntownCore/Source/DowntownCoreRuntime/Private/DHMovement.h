#pragma once

#include "CoreMinimal.h"

class ACharacter;

/**
 * Extra moves on top of Lyra's character movement, run on the owning client (instant feel) and on the server
 * (UDHPlayerChoicesComponent RPCs) with the same inputs:
 * - Slide: crouching at running speed throws the character forward with little friction for a moment.
 * - Wall jump: jumping in the air next to a wall pushes off it, up and away.
 */
namespace DHMovement
{
	constexpr float SlideMinSpeed = 420.f;
	constexpr float SlideSeconds = 0.9f;
	constexpr float WallJumpCooldown = 0.35f;

	bool CanSlide(const ACharacter* Character);
	void Slide(ACharacter* Character);

	// Wall within reach while falling: its normal, else zero.
	FVector FindWall(const ACharacter* Character);
	void WallJump(ACharacter* Character, const FVector& WallNormal);
}
