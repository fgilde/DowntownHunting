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
 * everyone else) and draws owner-only copies: the body copy plays a weapon-ready pose and is turned and
 * moved so the weapon copy in its hand points along the view with its grip at a view-space offset.
 */
// How one kind of weapon is held in first person.
USTRUCT(BlueprintType)
struct FDHFirstPersonWeaponView
{
	GENERATED_BODY()

	// Weapon-ready pose played by the body copy.
	UPROPERTY(EditAnywhere, Category = "First Person")
	TSoftObjectPtr<UAnimSequenceBase> Pose;

	// Weapon grip position in view space (X forward, Y right, Z up), from the hip and while aiming. Tune per feel.
	UPROPERTY(EditAnywhere, Category = "First Person")
	FVector HipOffset = FVector(22.0, 9.0, -11.0);

	UPROPERTY(EditAnywhere, Category = "First Person")
	FVector ADSOffset = FVector(30.0, 0.0, -10.0);

	// Off for one-handed weapons: the left arm vanishes with the torso (it would hang right in front of the view).
	UPROPERTY(EditAnywhere, Category = "First Person")
	bool bLeftArm = true;
};

UCLASS(Blueprintable)
class UDHCameraMode_FirstPerson : public ULyraCameraMode
{
	GENERATED_BODY()

public:
	UDHCameraMode_FirstPerson();

protected:
	virtual void OnDeactivation() override;
	virtual void UpdateView(float DeltaTime) override;

	// Weapon socket at the barrel end; grip-to-muzzle is turned to point along the view.
	UPROPERTY(EditDefaultsOnly, Category = "First Person")
	FName MuzzleSocket = TEXT("Muzzle");

	// Per weapon kind, picked by a substring of the equipped weapon actor's class name.
	UPROPERTY(EditDefaultsOnly, Category = "First Person")
	TMap<FString, FDHFirstPersonWeaponView> WeaponViews;

	// Used when no entry in WeaponViews matches.
	UPROPERTY(EditDefaultsOnly, Category = "First Person")
	FDHFirstPersonWeaponView DefaultView;

	// First-person arms: the torso bone is shrunk away, the arm bones are kept where the pose puts them.
	UPROPERTY(EditDefaultsOnly, Category = "First Person")
	FName TorsoBone = TEXT("spine_02");

	UPROPERTY(EditDefaultsOnly, Category = "First Person")
	TArray<FName> ArmBones = { TEXT("upperarm_l"), TEXT("upperarm_r") };

	// Bones hidden on the body copy (children are hidden too).
	UPROPERTY(EditDefaultsOnly, Category = "First Person")
	TArray<FName> HiddenBones = { TEXT("neck_01"), TEXT("thigh_l"), TEXT("thigh_r") };

	// Scales first-person copies towards the camera so they do not poke into walls. Looks the same on screen.
	UPROPERTY(EditDefaultsOnly, Category = "First Person")
	float FirstPersonScale = 0.5f;

	// While aiming down sights (tag Event.Movement.ADS on the pawn).
	UPROPERTY(EditDefaultsOnly, Category = "First Person|ADS")
	float ADSFieldOfView = 60.0f;

	UPROPERTY(EditDefaultsOnly, Category = "First Person|ADS")
	float ADSBlendSpeed = 12.0f;

	// Kick applied per shot: body copy moves back (cm) and pitches up (deg), then settles.
	UPROPERTY(EditDefaultsOnly, Category = "First Person|Recoil")
	float RecoilBack = 4.0f;

	UPROPERTY(EditDefaultsOnly, Category = "First Person|Recoil")
	float RecoilPitch = 4.0f;

	UPROPERTY(EditDefaultsOnly, Category = "First Person|Recoil")
	float RecoilRecoverySpeed = 14.0f;

private:
	void UpdateWeaponState(const ACharacter& Character, float DeltaTime);
	void SetFirstPersonRendering(bool bEnabled) const;
	void BuildCopies(ACharacter& Character);
	void DestroyCopies();
	void ShowOnlyArms() const;
	void SetOriginalsHidden(bool bHidden);
	const FDHFirstPersonWeaponView& FindView(const TArray<AActor*>& Weapons) const;
	TArray<AActor*> GetAttachedWeapons(const ACharacter& Character) const;

	TArray<TWeakObjectPtr<UMeshComponent>> Originals;
	// Heads and groom hair: hidden outright while in first person (grooms ignore the first-person primitive type).
	TArray<TWeakObjectPtr<UMeshComponent>> HiddenInFirstPerson;
	TArray<TWeakObjectPtr<UMeshComponent>> Copies;
	TWeakObjectPtr<USkeletalMeshComponent> BodyCopy;
	TWeakObjectPtr<class UPoseableMeshComponent> ArmsCopy;
	TWeakObjectPtr<UMeshComponent> WeaponCopy;
	TArray<TWeakObjectPtr<AActor>> CopiedWeapons;

	FDHFirstPersonWeaponView CurrentView;
	float ADSAlpha = 0.0f;
	float Recoil = 0.0f;
	float LastTimeSinceInteracted = 0.0f;
};
