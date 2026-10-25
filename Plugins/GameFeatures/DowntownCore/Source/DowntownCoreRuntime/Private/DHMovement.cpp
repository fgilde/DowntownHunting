#include "DHMovement.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

namespace DHMovement
{
	bool CanSprint(const ACharacter* Character)
	{
		static const FGameplayTag AimTag = FGameplayTag::RequestGameplayTag(TEXT("Event.Movement.ADS"));
		const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
		const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Character);
		if (!Movement || Movement->IsCrouching() || (ASC && ASC->HasMatchingGameplayTag(AimTag)))
		{
			return false;
		}
		// The owning client knows the stick, the server only the acceleration it results in.
		FVector Input = Character->IsLocallyControlled() ? Character->GetLastMovementInputVector() : Movement->GetCurrentAcceleration();
		Input.Z = 0.f;
		return Input.Size() > (Character->IsLocallyControlled() ? 0.3f : 1.f)
			&& FVector::DotProduct(Input.GetSafeNormal(), Character->GetActorForwardVector()) > 0.5f;
	}

	bool CanSlide(const ACharacter* Character)
	{
		const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
		return Movement && Movement->IsMovingOnGround() && Character->GetVelocity().Size2D() >= SlideMinSpeed;
	}

	void Slide(ACharacter* Character)
	{
		UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
		const UCharacterMovementComponent* Defaults = CastChecked<UCharacterMovementComponent>(Movement->GetArchetype());
		// A push forward, then let it run out on almost no friction; crouching itself is Lyra's (same input).
		const FVector Direction = Character->GetVelocity().GetSafeNormal2D();
		Character->LaunchCharacter(Direction * 1000.f, true, false);
		Movement->GroundFriction = 0.6f;
		Movement->BrakingDecelerationWalking = 250.f;
		Movement->MaxWalkSpeedCrouched = 1000.f;

		FTimerHandle Restore;
		Character->GetWorldTimerManager().SetTimer(Restore, FTimerDelegate::CreateWeakLambda(Character, [Character, Defaults]()
		{
			UCharacterMovementComponent* Restored = Character->GetCharacterMovement();
			Restored->GroundFriction = Defaults->GroundFriction;
			Restored->BrakingDecelerationWalking = Defaults->BrakingDecelerationWalking;
			Restored->MaxWalkSpeedCrouched = Defaults->MaxWalkSpeedCrouched;
		}), SlideSeconds, false);
	}

	FVector FindWall(const ACharacter* Character)
	{
		const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
		if (!Movement || !Movement->IsFalling())
		{
			return FVector::ZeroVector;
		}
		const FVector Start = Character->GetActorLocation();
		const float Reach = Character->GetCapsuleComponent()->GetScaledCapsuleRadius() + 45.f;
		FCollisionQueryParams Params(TEXT("DHWallJump"), false, Character);
		// Look around in eight directions, nearest wall wins.
		FHitResult Best;
		for (int32 Index = 0; Index < 8; ++Index)
		{
			const FVector Direction = FRotator(0.f, Character->GetActorRotation().Yaw + Index * 45.f, 0.f).Vector();
			FHitResult Hit;
			if (Character->GetWorld()->LineTraceSingleByChannel(Hit, Start, Start + Direction * Reach, ECC_Visibility, Params)
				&& FMath::Abs(Hit.ImpactNormal.Z) < 0.3f && (!Best.bBlockingHit || Hit.Distance < Best.Distance))
			{
				Best = Hit;
			}
		}
		return Best.bBlockingHit ? Best.ImpactNormal.GetSafeNormal2D() : FVector::ZeroVector;
	}

	void WallJump(ACharacter* Character, const FVector& WallNormal)
	{
		Character->LaunchCharacter(WallNormal * 550.f + FVector(0.f, 0.f, 780.f), true, true);
	}
}
