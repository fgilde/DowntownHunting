#pragma once

#include "Camera/LyraCameraMode.h"

#include "DHCameraMode_FirstPerson.generated.h"

/**
 * First-person camera: the base Lyra mode already puts the view at the pawn's eye height,
 * so this only exposes it to Blueprints and hides the local character's head while active.
 */
UCLASS(Blueprintable)
class UDHCameraMode_FirstPerson : public ULyraCameraMode
{
	GENERATED_BODY()

protected:
	virtual void OnActivation() override;
	virtual void OnDeactivation() override;

	// Bone hidden on the viewed character so the camera does not render the inside of the head.
	UPROPERTY(EditDefaultsOnly, Category = "First Person")
	FName HeadBoneName = TEXT("head");

private:
	void SetHeadHidden(bool bHidden) const;
};
