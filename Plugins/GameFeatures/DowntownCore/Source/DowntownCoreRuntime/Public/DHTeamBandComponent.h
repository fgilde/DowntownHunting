#pragma once

#include "Components/ActorComponent.h"

#include "DHTeamBandComponent.generated.h"

class UMaterialInterface;
class UStaticMesh;

/**
 * Adds team-colored bands to the owner's skeletal mesh (one per socket).
 * The band material uses Lyra's team color parameters, so Lyra's team display assets tint them like any other
 * cosmetic mesh. Needed for character meshes whose own materials have no team color parameters (City Sample).
 */
UCLASS(ClassGroup = (DowntownHunting), meta = (BlueprintSpawnableComponent))
class UDHTeamBandComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDHTeamBandComponent();

protected:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;

	UPROPERTY(EditAnywhere, Category = "Team Band")
	TObjectPtr<UStaticMesh> BandMesh;

	UPROPERTY(EditAnywhere, Category = "Team Band")
	TObjectPtr<UMaterialInterface> BandMaterial;

	// One band per socket/bone, placed with BandTransform relative to it (the bone's X axis runs along the limb).
	UPROPERTY(EditAnywhere, Category = "Team Band")
	TArray<FName> Sockets = { TEXT("upperarm_l"), TEXT("upperarm_r") };

	UPROPERTY(EditAnywhere, Category = "Team Band")
	FTransform BandTransform = FTransform(FRotator(90.0, 0.0, 0.0), FVector(14.0, 0.0, 0.0), FVector(0.13, 0.13, 0.05));

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Bands;
};
