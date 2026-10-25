#include "DHAmbience.h"

#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundWave.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DHAmbience)

namespace
{
	const TCHAR* AmbientFolder = TEXT("/Game/Audio/SoundWaves/Ambient/");

	USoundWave* Wave(const FString& Path)
	{
		const FString Full = FString(AmbientFolder) + Path;
		return LoadObject<USoundWave>(nullptr, *(Full + TEXT(".") + FPaths::GetCleanFilename(Full)), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	struct FOneShot
	{
		const TCHAR* Path;
		float Weight;
		float Height;  // above the listener (aircraft high up)
		float Volume;
	};

	const FOneShot OneShots[] = {
		{ TEXT("Sweetners/sfx_LeaveBehind_amb_Swt_SedanCarBy_nl_01"), 3.f, 0.f, 0.8f },
		{ TEXT("Sweetners/sfx_LeaveBehind_amb_Swt_SedanCarBy_Muffler_nl_01"), 2.f, 0.f, 0.8f },
		{ TEXT("Sweetners/sfx_LeaveBehind_amb_Swt_SedanCarBy_Muffler_nl_02"), 2.f, 0.f, 0.8f },
		{ TEXT("Sweetners/sfx_LeaveBehind_amb_Swt_SedanCarBy_Whooshy_nl_01"), 2.f, 0.f, 0.8f },
		{ TEXT("Sweetners/sfx_LeaveBehind_amb_Swt_SedanCarBy_Brakes_nl_01"), 1.f, 0.f, 0.8f },
		{ TEXT("Sweetners/sfx_LeaveBehind_amb_Swt_SmallCarBy_nl_01"), 2.f, 0.f, 0.8f },
		{ TEXT("Sweetners/sfx_LeaveBehind_amb_Swt_SmallCarBy_nl_02"), 2.f, 0.f, 0.8f },
		{ TEXT("Sweetners/sfx_LeaveBehind_amb_Swt_CityBusBy_nl_01"), 1.5f, 0.f, 0.9f },
		{ TEXT("Sweetners/sfx_LeaveBehind_amb_Swt_CityBusBy_Corner_Brakes_nl_02"), 1.f, 0.f, 0.9f },
		{ TEXT("Sweetners/sfx_LeaveBehind_amb_Swt_DieselTruckBy_nl_01"), 1.f, 0.f, 0.9f },
		{ TEXT("Sweetners/sfx_LeaveBehind_amb_Swt_MotorcycleBy_nl_01"), 1.f, 0.f, 0.9f },
		{ TEXT("Sweetners/sfx_LeaveBehind_amb_Swt_ScooterCarBy_nl_01"), 1.f, 0.f, 0.8f },
		{ TEXT("Sweetners/sfx_LeaveBehind_amb_Swt_HeliBy_nl_01"), 0.8f, 6000.f, 1.f },
		{ TEXT("Sweetners/sfx_LeaveBehind_amb_Swt_HeliBy_nl_02"), 0.8f, 6000.f, 1.f },
		{ TEXT("Sweetners/sfx_LeaveBehind_amb_Swt_SmJetBy_nl_01"), 0.5f, 9000.f, 1.f },
		{ TEXT("Air_Traffic/sfx_LeaveBehind_Amb_DistPassengerJetBy_Stereo_nl_01"), 0.5f, 9000.f, 0.8f },
		{ TEXT("Siren_Horn/Horns/sfx_LeaveBehind_amb_Swt_CarHorn_By_nl_01"), 1.f, 0.f, 0.7f },
		{ TEXT("Siren_Horn/Horns/sfx_LeaveBehind_amb_Swt_Car_Horn_nl_03"), 1.f, 0.f, 0.6f },
		{ TEXT("Siren_Horn/Horns/sfx_LeaveBehind_amb_Swt_Car_Horn_nl_07"), 1.f, 0.f, 0.6f },
		{ TEXT("Siren_Horn/Siren/sfx_LeaveBehind_amb_Swt_Siren_nl_01"), 0.7f, 0.f, 0.8f },
		{ TEXT("Siren_Horn/Siren/sfx_LeaveBehind_amb_Swt_Siren_nl_04"), 0.7f, 0.f, 0.8f },
		{ TEXT("VariousCity/amb_sirens_distant_01"), 0.6f, 0.f, 0.9f },
		{ TEXT("Misc/sfx_LeaveBehind_amb_Swt_Jackhammer_Med_nl_01"), 0.4f, 0.f, 0.6f },
	};
}

void UDHAmbience::Start(UWorld* InWorld)
{
	World = InWorld;
	if (!InWorld || InWorld->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	// Far reaching, softened with distance and behind walls.
	Distance = NewObject<USoundAttenuation>(this);
	Distance->Attenuation.bAttenuate = true;
	Distance->Attenuation.bSpatialize = true;
	Distance->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
	Distance->Attenuation.AttenuationShapeExtents = FVector(1500.f);
	Distance->Attenuation.FalloffDistance = 12000.f;
	Distance->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
	Distance->Attenuation.bAttenuateWithLPF = true;
	Distance->Attenuation.LPFRadiusMin = 2000.f;
	Distance->Attenuation.LPFRadiusMax = 12000.f;
	Distance->Attenuation.bEnableOcclusion = true;

	// The bed: urban air and traffic washing past somewhere.
	for (const TPair<const TCHAR*, float>& Bed : { TPair<const TCHAR*, float>(TEXT("Air_Traffic/sfx_LeaveBehind_amb_Urban_Air_Mid_lp_01"), 0.45f),
		TPair<const TCHAR*, float>(TEXT("Air_Traffic/sfx_LeaveBehind_amb_Traffic_MedDist_Road_lp_01"), 0.3f),
		TPair<const TCHAR*, float>(TEXT("Air_Traffic/sfx_LeaveBehind_amb_Urban_Air_TrafficRumble_lp_01"), 0.25f) })
	{
		if (USoundWave* Sound = Wave(Bed.Key))
		{
			Sound->bLooping = true;
			if (UAudioComponent* Audio = UGameplayStatics::SpawnSound2D(InWorld, Sound, 0.f, 1.f, FMath::FRandRange(0.f, 20.f), nullptr, false, false))
			{
				Audio->FadeIn(4.f, Bed.Value);
				Beds.Add(Audio);
			}
		}
	}
	InWorld->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateUObject(this, &ThisClass::PlayOneShot), 3.f, false);
}

void UDHAmbience::Stop()
{
	if (World.IsValid())
	{
		World->GetTimerManager().ClearTimer(Timer);
	}
	for (UAudioComponent* Bed : Beds)
	{
		if (Bed)
		{
			Bed->FadeOut(1.f, 0.f);
		}
	}
	Beds.Reset();
}

void UDHAmbience::PlayOneShot()
{
	UWorld* InWorld = World.Get();
	const APlayerController* Player = InWorld ? InWorld->GetFirstPlayerController() : nullptr;
	if (!Player)
	{
		return;
	}
	FVector Listener;
	FRotator Facing;
	Player->GetPlayerViewPoint(Listener, Facing);

	float Total = 0.f;
	for (const FOneShot& Shot : OneShots)
	{
		Total += Shot.Weight;
	}
	float Pick = FMath::FRandRange(0.f, Total);
	const FOneShot* Chosen = &OneShots[0];
	for (const FOneShot& Shot : OneShots)
	{
		Pick -= Shot.Weight;
		if (Pick <= 0.f)
		{
			Chosen = &Shot;
			break;
		}
	}
	if (USoundWave* Sound = Wave(Chosen->Path))
	{
		const FVector Offset = FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f).Vector() * FMath::FRandRange(2500.f, 9000.f);
		UGameplayStatics::PlaySoundAtLocation(InWorld, Sound, Listener + Offset + FVector(0.f, 0.f, Chosen->Height + 200.f),
			FRotator::ZeroRotator, Chosen->Volume, FMath::FRandRange(0.94f, 1.06f), 0.f, Distance);
	}
	InWorld->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateUObject(this, &ThisClass::PlayOneShot), FMath::FRandRange(3.f, 9.f), false);
}
