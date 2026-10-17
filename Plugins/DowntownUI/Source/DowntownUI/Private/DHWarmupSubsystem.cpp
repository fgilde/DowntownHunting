#include "DHWarmupSubsystem.h"

#include "ContentStreaming.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "LoadingScreenManager.h"
#include "PipelineStateCache.h"
#include "ShaderPipelineCache.h"
#include "UObject/UObjectGlobals.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(DHWarmupSubsystem)

namespace DHWarmup
{
	static float MaxSeconds = 20.f;
	static FAutoConsoleVariableRef CVarMaxSeconds(TEXT("DH.Warmup.MaxSeconds"), MaxSeconds,
		TEXT("Longest time the loading screen waits after a map load for shaders, PSOs and textures (0 = off)."));

	// Texture requests below this count are normal background streaming in the city, not missing textures.
	static int32 StreamingThreshold = 40;
	static FAutoConsoleVariableRef CVarStreaming(TEXT("DH.Warmup.StreamingThreshold"), StreamingThreshold,
		TEXT("Pending texture streaming requests that still count as ready."));
}

void UDHWarmupSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	MapLoadedHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::HandleMapLoaded);
	if (ULoadingScreenManager* LoadingScreens = Collection.InitializeDependency<ULoadingScreenManager>())
	{
		LoadingScreens->RegisterLoadingProcessor(this);
	}
}

void UDHWarmupSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(MapLoadedHandle);
	if (ULoadingScreenManager* LoadingScreens = GetGameInstance()->GetSubsystem<ULoadingScreenManager>())
	{
		LoadingScreens->UnregisterLoadingProcessor(this);
	}
	Super::Deinitialize();
}

void UDHWarmupSubsystem::HandleMapLoaded(UWorld* World)
{
	if (World && World->GetGameInstance() == GetGameInstance())
	{
		MapLoadedTime = FPlatformTime::Seconds();
	}
}

bool UDHWarmupSubsystem::ShouldShowLoadingScreen(FString& OutReason) const
{
	if (MapLoadedTime < 0.0)
	{
		return false;
	}
	const double Waited = FPlatformTime::Seconds() - MapLoadedTime;
	if (Waited > DHWarmup::MaxSeconds)
	{
		UE_LOG(LogTemp, Log, TEXT("DH warmup: gave up after %.1f s"), Waited);
		MapLoadedTime = -1.0;
		return false;
	}

	int32 ShaderJobs = 0;
#if WITH_EDITOR
	ShaderJobs = GShaderCompilingManager ? GShaderCompilingManager->GetNumRemainingJobs() : 0;
#endif
	const uint32 Psos = PipelineStateCache::NumActivePrecacheRequests() + FShaderPipelineCache::NumPrecompilesRemaining();
	const int32 Textures = IStreamingManager::Get().GetNumWantingResources();
	if (ShaderJobs == 0 && Psos == 0 && Textures <= DHWarmup::StreamingThreshold)
	{
		UE_LOG(LogTemp, Log, TEXT("DH warmup: ready %.1f s after the map load"), Waited);
		MapLoadedTime = -1.0;  // ready, stop holding until the next map
		return false;
	}
	OutReason = FString::Printf(TEXT("Warming up: %d shaders, %u PSOs, %d textures"), ShaderJobs, Psos, Textures);
	return true;
}
