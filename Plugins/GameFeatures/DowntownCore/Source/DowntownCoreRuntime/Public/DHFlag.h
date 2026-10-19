#pragma once

#include "GameFramework/Actor.h"

#include "DHFlag.generated.h"

class UMaterialInstanceDynamic;
class UPointLightComponent;
class UStaticMeshComponent;

/**
 * Capture the Flag: a team's flag (pole and cloth in the team colour, a light). Spawned and moved by UDHFlagScoring on
 * the server: stands at its base, rides on the back of the carrier (attached, replicated) or lies where the carrier
 * died.
 */
UCLASS(NotPlaceable)
class ADHFlag : public AActor
{
	GENERATED_BODY()

public:
	ADHFlag();

	UPROPERTY(ReplicatedUsing = OnRep_Colour)
	FLinearColor Colour = FLinearColor::White;

	int32 TeamId = INDEX_NONE;
	FVector Home = FVector::ZeroVector;
	TWeakObjectPtr<APawn> Carrier;
	float DroppedSeconds = -1.f;  // < 0: not lying around

	bool IsHome() const { return !Carrier.IsValid() && DroppedSeconds < 0.f; }

	void PickUp(APawn* Pawn);
	void Drop();
	void ReturnHome();

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	void OnRep_Colour();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Pole;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Cloth;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPointLightComponent> Light;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ClothMaterial;
};
