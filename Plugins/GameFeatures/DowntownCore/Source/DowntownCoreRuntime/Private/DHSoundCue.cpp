#include "DHSoundCue.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DHSoundCue)

namespace
{
	struct FLayer
	{
		const TCHAR* Sound;
		float Volume = 1.f;
		float Pitch = 1.f;
		float Delay = 0.f;
		const TCHAR* Attenuation = nullptr;  // for plain sound waves; Lyra's MetaSounds bring their own
	};

	const TCHAR* GrenadeAttenuation = TEXT("/Game/Audio/AttenuationPresets/ATT_Grenade.ATT_Grenade");
	const TCHAR* ShotgunAttenuation = TEXT("/Game/Audio/AttenuationPresets/ATT_Shotgun.ATT_Shotgun");

	const TMap<FName, TArray<FLayer>>& Layers()
	{
		static const TMap<FName, TArray<FLayer>> Map = {
			{ TEXT("GameplayCue.DH.Ability.Dash"), { { TEXT("/Game/Audio/MetaSounds/sfx_DashStereo_nl_meta_Preset.sfx_DashStereo_nl_meta_Preset") } } },
			{ TEXT("GameplayCue.DH.Ability.Leap"), {
				{ TEXT("/Game/Audio/MetaSounds/sfx_Interactable_JumpPad_nl_meta.sfx_Interactable_JumpPad_nl_meta") },
				{ TEXT("/Game/Audio/MetaSounds/sfx_DashStereo_nl_meta_Preset.sfx_DashStereo_nl_meta_Preset"), 0.7f, 0.7f } } },
			{ TEXT("GameplayCue.DH.Ability.Adrenaline"), {
				{ TEXT("/Game/Audio/MetaSounds/sfx_Heal_nl_metaPreset.sfx_Heal_nl_metaPreset"), 1.f, 1.3f },
				{ TEXT("/Game/Audio/MetaSounds/sfx_DashStereo_nl_meta_Preset.sfx_DashStereo_nl_meta_Preset"), 0.5f, 1.2f } } },
			{ TEXT("GameplayCue.DH.Ability.Recon"), {
				{ TEXT("/Game/Audio/MetaSounds/sfx_EndOfRound-Tick_nl_meta.sfx_EndOfRound-Tick_nl_meta"), 1.f, 0.6f },
				{ TEXT("/Game/Audio/MetaSounds/sfx_EndOfRound-Tick_nl_meta.sfx_EndOfRound-Tick_nl_meta"), 0.6f, 0.6f, 0.35f } } },
			{ TEXT("GameplayCue.DH.Ability.Cloak"), {
				{ TEXT("/Game/Audio/MetaSounds/sfx_Teleport_nl_meta_Preset.sfx_Teleport_nl_meta_Preset"), 1.f, 0.8f },
				{ TEXT("/Game/Audio/MetaSounds/sfx_MeleeWhoosh_nl_meta_Preset.sfx_MeleeWhoosh_nl_meta_Preset"), 0.8f, 0.5f } } },
			// The sniper's rifle report gets a deep punch and a long roll through the streets.
			{ TEXT("GameplayCue.DH.Sniper"), {
				{ TEXT("/Game/Audio/Sounds/Weapons/Shotgun/Weapons_Shotgun_Punch_01.Weapons_Shotgun_Punch_01"), 0.9f, 0.75f, 0.f, ShotgunAttenuation },
				{ TEXT("/Game/Audio/Sounds/Explosions/Explosions_Grenade_Noise-Exterior-Distant_02.Explosions_Grenade_Noise-Exterior-Distant_02"), 0.45f, 1.5f, 0.12f, GrenadeAttenuation } } },
		};
		return Map;
	}
}

namespace
{
	struct FEffect
	{
		const TCHAR* System;
		bool bAttach;    // follows the pawn
		float Seconds;   // then switched off (some of Lyra's systems loop)
		float Down;      // spawn this far below the pawn's centre (feet)
	};

	// Lyra's Niagara systems, reused for the abilities (seen by everyone).
	const TMap<FName, FEffect>& Effects()
	{
		static const TMap<FName, FEffect> Map = {
			{ TEXT("GameplayCue.DH.Ability.Leap"), { TEXT("/Game/Effects/Particles/Environmental/NS_JumpPad.NS_JumpPad"), false, 1.2f, 90.f } },
			{ TEXT("GameplayCue.DH.Ability.Adrenaline"), { TEXT("/Game/Effects/Particles/Environmental/NS_ElectricMovement.NS_ElectricMovement"), true, 4.f, 0.f } },
			{ TEXT("GameplayCue.DH.Ability.Recon"), { TEXT("/Game/Effects/Particles/Environmental/NS_CharacterSpawnIn.NS_CharacterSpawnIn"), true, 2.f, 90.f } },
		};
		return Map;
	}
}

void UDHSoundCue::Play(APawn* Pawn, const TCHAR* Name)
{
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn);
	if (!ASC || !Pawn->HasAuthority())
	{
		return;
	}
	FGameplayCueParameters Parameters;
	Parameters.Location = Pawn->GetActorLocation();
	ASC->ExecuteGameplayCue(FGameplayTag::RequestGameplayTag(*(FString(TEXT("GameplayCue.DH.")) + Name)), Parameters);
}

void UDHSoundCue::PlayExplosion(const UObject* Context, const FVector& Location, float Volume)
{
	for (const TCHAR* Path : { TEXT("/Game/Audio/MetaSounds/sfx_Weapon_GrenadeExplosion_nl_meta.sfx_Weapon_GrenadeExplosion_nl_meta"),
		TEXT("/Game/Audio/Sounds/Explosions/MSS_Explosions_Grenade.MSS_Explosions_Grenade") })
	{
		UGameplayStatics::PlaySoundAtLocation(Context, LoadObject<USoundBase>(nullptr, Path), Location, Volume);
	}
}

bool UDHSoundCue::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	const TArray<FLayer>* Sounds = Layers().Find(Parameters.OriginalTag.GetTagName());
	UWorld* World = MyTarget ? MyTarget->GetWorld() : nullptr;
	if (!Sounds || !World)
	{
		return false;
	}
	const FVector Location = Parameters.Location.IsZero() ? MyTarget->GetActorLocation() : FVector(Parameters.Location);
	if (const FEffect* Effect = Effects().Find(Parameters.OriginalTag.GetTagName()))
	{
		UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, Effect->System, nullptr, LOAD_NoWarn | LOAD_Quiet);
		UNiagaraComponent* Spawned = !System ? nullptr : Effect->bAttach
			? UNiagaraFunctionLibrary::SpawnSystemAttached(System, MyTarget->GetRootComponent(), NAME_None, FVector(0.f, 0.f, -Effect->Down), FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, true)
			: UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, System, Location - FVector(0.f, 0.f, Effect->Down));
		if (Spawned)
		{
			FTimerHandle Off;
			World->GetTimerManager().SetTimer(Off, FTimerDelegate::CreateWeakLambda(Spawned, [Spawned]() { Spawned->DestroyComponent(); }), Effect->Seconds, false);
		}
	}
	UE_LOG(LogTemp, Verbose, TEXT("DH sound cue %s at %s"), *Parameters.OriginalTag.ToString(), *Location.ToCompactString());
	for (const FLayer& Layer : *Sounds)
	{
		USoundBase* Sound = LoadObject<USoundBase>(nullptr, Layer.Sound);
		USoundAttenuation* Attenuation = Layer.Attenuation ? LoadObject<USoundAttenuation>(nullptr, Layer.Attenuation) : nullptr;
		if (!Sound)
		{
			continue;
		}
		auto PlayLayer = [World, Sound, Attenuation, Location, Volume = Layer.Volume, Pitch = Layer.Pitch]()
		{
			UGameplayStatics::PlaySoundAtLocation(World, Sound, Location, FRotator::ZeroRotator, Volume, Pitch, 0.f, Attenuation);
		};
		if (Layer.Delay > 0.f)
		{
			FTimerHandle Timer;
			World->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(World, PlayLayer), Layer.Delay, false);
		}
		else
		{
			PlayLayer();
		}
	}
	return true;
}
