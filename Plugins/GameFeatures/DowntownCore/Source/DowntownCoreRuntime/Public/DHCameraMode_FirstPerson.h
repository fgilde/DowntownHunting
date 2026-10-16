#pragma once

#include "Camera/LyraCameraMode.h"

#include "DHCameraMode_FirstPerson.generated.h"

class ACharacter;
class UAnimSequenceBase;
class UMeshComponent;
class USkeletalMeshComponent;

/**
 * First-person camera: the base Lyra mode already puts the view at the pawn's eye height.
 *
 * Lyra's third-person animations hold the weapon at chest height, below the view. For the local
 * viewer this mode hides the real body and weapon (they still cast shadows and stay visible to
 * everyone else) and draws owner-only copies: the body copy plays a weapon-ready pose, is turned
 * with the view and placed so its head sits at the camera. Weapon copies hang in its hand.
 */
UCLASS(Blueprintable)
class UDHCameraMode_FirstPerson : public ULyraCameraMode
{
	GENERATED_BODY()

public:
	UDHCameraMode_FirstPerson();

protected:
	virtual void OnDeactivation() override;
	virtual void UpdateView(float DeltaTime) override;

	// Camera position relative to the head bone, in view space (X forward, Y right, Z up). Tune per feel.
	UPROPERTY(EditDefaultsOnly, Category = "First Person")
	FVector CameraOffsetFromHead = FVector(4.0, 24.0, 14.0);

	UPROPERTY(EditDefaultsOnly, Category = "First Person")
	FName HeadBone = TEXT("head");

	// Pose for the body copy, picked by a substring of the equipped weapon actor's class name.
	UPROPERTY(EditDefaultsOnly, Category = "First Person")
	TMap<FString, TSoftObjectPtr<UAnimSequenceBase>> WeaponPoses;

	// Used when no entry in WeaponPoses matches.
	UPROPERTY(EditDefaultsOnly, Category = "First Person")
	TSoftObjectPtr<UAnimSequenceBase> DefaultPose;

	// Bones hidden on the body copy (children are hidden too).
	UPROPERTY(EditDefaultsOnly, Category = "First Person")
	TArray<FName> HiddenBones = { TEXT("neck_01"), TEXT("thigh_l"), TEXT("thigh_r") };

private:
	void BuildCopies(ACharacter& Character);
	void DestroyCopies();
	void SetOriginalsHidden(bool bHidden);
	UAnimSequenceBase* FindPose(const TArray<AActor*>& Weapons) const;
	TArray<AActor*> GetAttachedWeapons(const ACharacter& Character) const;

	TArray<TWeakObjectPtr<UMeshComponent>> Originals;
	TArray<TWeakObjectPtr<UMeshComponent>> Copies;
	TWeakObjectPtr<USkeletalMeshComponent> BodyCopy;
	TArray<TWeakObjectPtr<AActor>> CopiedWeapons;
};
