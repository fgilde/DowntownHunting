#pragma once

#include "LoadingProcessInterface.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "DHWarmupSubsystem.generated.h"

class UWorld;

/**
 * Keeps Lyra's loading screen up after a map has loaded until the GPU is ready: pipeline state (PSO) precaching,
 * shader compilation (only when running from the editor, cooked builds ship their shaders) and texture streaming.
 * Without it characters and buildings show black or blurry for the first seconds of a match.
 * Gives up after DH.Warmup.MaxSeconds so a busy streaming city never blocks the game.
 */
UCLASS()
class UDHWarmupSubsystem : public UGameInstanceSubsystem, public ILoadingProcessInterface
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldShowLoadingScreen(FString& OutReason) const override;

private:
	void HandleMapLoaded(UWorld* World);

	mutable double MapLoadedTime = -1.0;  // reset once ready
	FDelegateHandle MapLoadedHandle;
};
