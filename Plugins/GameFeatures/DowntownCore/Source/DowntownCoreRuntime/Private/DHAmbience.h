#pragma once

#include "UObject/Object.h"

#include "DHAmbience.generated.h"

class UAudioComponent;
class USoundAttenuation;

/**
 * The city around the match, on every client (not on a dedicated server): City Sample's ambience (Tools/CitySample_
 * Ambience.txt, /Game/Audio/SoundWaves/Ambient): a bed of urban air and distant traffic, and every few seconds something
 * passing somewhere around the listener in 3D, 25 to 90 m away: cars, buses, motorbikes, horns, sirens, a helicopter or
 * a jet overhead, a jackhammer. Without the City Sample sounds it stays silent. Started by UDHGameRulesComponent.
 */
UCLASS()
class UDHAmbience : public UObject
{
	GENERATED_BODY()

public:
	void Start(UWorld* World);
	void Stop();

private:
	void PlayOneShot();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UAudioComponent>> Beds;

	UPROPERTY(Transient)
	TObjectPtr<USoundAttenuation> Distance;

	TWeakObjectPtr<UWorld> World;
	FTimerHandle Timer;
};
