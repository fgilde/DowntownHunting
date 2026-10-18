#pragma once

#include "GameFramework/Actor.h"

#include "DHBombSite.generated.h"

class UInstancedStaticMeshComponent;
class UPointLightComponent;
class UTextRenderComponent;
class UMaterialInstanceDynamic;

/**
 * Search & Destroy bomb site: a ring of markers on the ground and its letter above it. Spawned by
 * UDHSearchAndDestroyScoring on the server (at the map's control points) and replicated. Turns red and flashes
 * while armed; Explode() blows it up on every machine and kills everybody close to it.
 */
UCLASS(NotPlaceable)
class ADHBombSite : public AActor
{
	GENERATED_BODY()

public:
	ADHBombSite();

	static constexpr float Radius = 450.f;

	UPROPERTY(ReplicatedUsing = OnRep_State)
	FString Letter;

	UPROPERTY(ReplicatedUsing = OnRep_State)
	bool bArmed = false;

	// True if Location is inside the site (a cylinder, a few metres high).
	bool Contains(const FVector& Location) const;

	void SetState(const FString& InLetter, bool bInArmed);

	static constexpr float BlastRadius = 1200.f;

	// Server: kills the players in the blast radius, then the effect and sound everywhere.
	void Explode();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	void OnRep_State();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastExplosion();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> Ring;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPointLightComponent> Light;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> Label;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> RingMaterial;

	// Loaded at begin play, so the effect is compiled/streamed before the bomb goes off.
	UPROPERTY(Transient)
	TObjectPtr<class UNiagaraSystem> ExplosionEffect;

	UPROPERTY(Transient)
	TObjectPtr<class USoundBase> ExplosionSound;
};
