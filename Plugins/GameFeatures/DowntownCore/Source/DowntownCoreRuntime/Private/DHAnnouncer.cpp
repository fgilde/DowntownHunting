#include "DHAnnouncer.h"

#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Teams/LyraTeamSubsystem.h"
#include "TimerManager.h"
#include "UObject/UObjectIterator.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DHAnnouncer)

void UDHAnnouncer::Start(UWorld* InWorld)
{
	World = InWorld;
	// Lyra's own ambient music (mx_*) gives way to ours, now and whenever its manager starts it again.
	InWorld->GetTimerManager().SetTimer(SilenceTimer, FTimerDelegate::CreateWeakLambda(this, []()
	{
		for (TObjectIterator<UAudioComponent> It; It; ++It)
		{
			if (!It->IsTemplate() && It->Sound && It->Sound->GetName().StartsWith(TEXT("mx_")))
			{
				It->SetVolumeMultiplier(0.f);  // Stop alone does not hold: its manager starts it again
				It->SetPaused(true);
			}
		}
	}), 1.f, true, 0.1f);
	PlayMusic(TEXT("Warmup"), 0.35f);
	Hear(InWorld, TEXT("Warmup"), INDEX_NONE, nullptr);
}

void UDHAnnouncer::Stop()
{
	if (World.IsValid())
	{
		World->GetTimerManager().ClearTimer(SilenceTimer);
	}
	if (Music)
	{
		Music->FadeOut(1.f, 0.f);
		Music = nullptr;
	}
}

void UDHAnnouncer::PlayMusic(const TCHAR* Track, float Volume)
{
	if (Music)
	{
		Music->FadeOut(1.5f, 0.f);
		Music = nullptr;
	}
	UWorld* InWorld = World.Get();
	const FString Path = FString::Printf(TEXT("/Game/DH/UI/Music/DH_Music_%s.DH_Music_%s"), Track, Track);
	if (USoundBase* Sound = InWorld && Track ? LoadObject<USoundBase>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet) : nullptr)
	{
		Music = UGameplayStatics::SpawnSound2D(InWorld, Sound, Volume, 1.f, 0.f, nullptr, false, false);
	}
}

void UDHAnnouncer::Hear(UWorld* InWorld, FName Event, int32 Team, const APlayerState* Subject)
{
	World = InWorld;
	if (Event == TEXT("MatchStart") && Music)
	{
		Music->FadeOut(1.f, 0.f);
		Music = nullptr;
	}
	else if (Event == TEXT("OneMinute"))
	{
		PlayMusic(TEXT("LastMinute"), 0.4f);
	}
	const FName Line = LineFor(InWorld, Event, Team, Subject);
	if (Line == TEXT("Victory") || Line == TEXT("Defeat"))
	{
		PlayMusic(*Line.ToString(), 0.7f);
	}
	if (Line.IsNone())
	{
		return;
	}
	// The same line twice in a row within a few seconds (a hill changing hands back and forth) is said once.
	const double Now = InWorld->GetTimeSeconds();
	if (Line == LastLine && Now - LastLineAt < 6.0)
	{
		return;
	}
	LastLine = Line;
	LastLineAt = Now;
	// Results and kills cut the queue short: old news is not read out late.
	if (Queue.Num() >= 2)
	{
		Queue.RemoveAt(0);
	}
	Queue.Add(Line);
	if (!Playing || !Playing->IsPlaying())
	{
		PlayNext();
	}
}

FName UDHAnnouncer::LineFor(UWorld* InWorld, FName Event, int32 Team, const APlayerState* Subject)
{
	const APlayerController* Player = InWorld ? InWorld->GetFirstPlayerController() : nullptr;
	const APlayerState* Me = Player ? Player->PlayerState : nullptr;
	const ULyraTeamSubsystem* Teams = InWorld ? InWorld->GetSubsystem<ULyraTeamSubsystem>() : nullptr;
	const int32 MyTeam = Teams && Me ? Teams->FindTeamFromObject(Me) : INDEX_NONE;
	const bool bOurs = Team != INDEX_NONE && Team == MyTeam;
	const bool bMe = Subject && Subject == Me;
	const FString Name = Event.ToString();

	if (Name.StartsWith(TEXT("Capture_")))
	{
		return FName((bOurs ? TEXT("OurCapture") : TEXT("EnemyCapture")) + Name.RightChop(8));
	}
	if (Event == TEXT("Lead"))
	{
		const bool bWasOurs = LeaderTeam != INDEX_NONE && LeaderTeam == MyTeam;
		LeaderTeam = Team;
		return bOurs ? FName(TEXT("LeadTaken")) : bWasOurs ? FName(TEXT("LeadLost")) : NAME_None;
	}
	if (Event == TEXT("MatchEnd"))
	{
		return Team == INDEX_NONE ? NAME_None : bOurs ? FName(TEXT("Victory")) : FName(TEXT("Defeat"));
	}
	if (Event == TEXT("RoundEnd"))
	{
		return Team == INDEX_NONE ? NAME_None : bOurs ? FName(TEXT("RoundWon")) : FName(TEXT("RoundLost"));
	}
	if (Event == TEXT("BombPlanted"))
	{
		return bOurs ? FName(TEXT("BombPlantedAttack")) : FName(TEXT("BombPlantedDefend"));
	}
	if (Event == TEXT("FlagTaken"))
	{
		return bOurs ? FName(TEXT("OurFlagTaken")) : FName(TEXT("EnemyFlagTaken"));
	}
	if (Event == TEXT("FlagCaptured"))
	{
		return bOurs ? FName(TEXT("OurFlagCaptured")) : FName(TEXT("EnemyFlagCaptured"));
	}
	if (Event == TEXT("HillControl"))
	{
		return bOurs ? FName(TEXT("HillOurs")) : FName(TEXT("HillEnemy"));
	}
	if (Event == TEXT("Airstrike"))
	{
		const int32 CallerTeam = Teams && Subject ? Teams->FindTeamFromObject(Subject) : INDEX_NONE;
		return bMe ? FName(TEXT("Airstrike")) : CallerTeam != MyTeam ? FName(TEXT("EnemyAirstrike")) : NAME_None;
	}
	static const TSet<FName> Personal = { TEXT("Radar"), TEXT("HunterMode"), TEXT("DoubleKill"), TEXT("TripleKill"), TEXT("MultiKill"),
		TEXT("Headshot"), TEXT("Infected"), TEXT("LastSurvivor"), TEXT("LastAlive"), TEXT("FinalWeapon") };
	if (Personal.Contains(Event))
	{
		return bMe ? Event : NAME_None;
	}
	return Event;  // MatchStart, OneMinute, NewRound, FirstBlood, BombDefused, BombExploded, FlagReturned, HillMoved
}

void UDHAnnouncer::PlayNext()
{
	UWorld* InWorld = World.Get();
	while (InWorld && !Queue.IsEmpty())
	{
		const FString Line = Queue[0].ToString();
		Queue.RemoveAt(0);
		const FString Path = FString::Printf(TEXT("/Game/DH/UI/Announcer/DH_Ann_%s.DH_Ann_%s"), *Line, *Line);
		USoundBase* Sound = LoadObject<USoundBase>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Sound)
		{
			continue;
		}
		Playing = UGameplayStatics::SpawnSound2D(InWorld, Sound, 1.f, 1.f, 0.f, nullptr, false, true);
		if (Playing)
		{
			Playing->OnAudioFinished.AddUniqueDynamic(this, &ThisClass::PlayNext);
		}
		return;
	}
}
