#pragma once

#include "UObject/Object.h"

#include "DHAnnouncer.generated.h"

class APlayerState;
class UAudioComponent;

/**
 * The match announcer on a client (Call of Duty style): the server names an event, the team it is about and maybe a
 * player (UDHGameRulesComponent::Announce); each client turns that into a line from its own side ("We captured Alpha!"
 * or "We've lost Alpha!") and plays it, one after another, never two at once. Lines: /Game/DH/UI/Announcer/DH_Ann_<Line>
 * (Tools/Branding/make_announcer.py). Also the match music: warmup until the match starts, a chase track for the last
 * minute, a victory or defeat sting at the end.
 *
 * Events: MatchStart, OneMinute, MatchEnd (Team won), Lead (Team leads now), Capture_A/B/C (Team took it), BombPlanted
 * (Team = attackers), BombDefused, BombExploded, RoundEnd (Team won), NewRound, FlagTaken (Team = whose flag), FlagCaptured
 * (Team scored), FlagReturned, HillMoved, HillControl (Team holds it), Airstrike (Subject called it), and for the Subject
 * only: Radar, HunterMode, DoubleKill, TripleKill, MultiKill, Headshot, Infected, LastSurvivor, LastAlive, FinalWeapon;
 * FirstBlood for everyone.
 */
UCLASS()
class UDHAnnouncer : public UObject
{
	GENERATED_BODY()

public:
	/** Joined a match: the warmup music (/Game/DH/UI/Music, quieter than in the menu). */
	void Start(UWorld* World);
	void Stop();

	void Hear(UWorld* World, FName Event, int32 Team, const APlayerState* Subject);

private:
	FName LineFor(UWorld* World, FName Event, int32 Team, const APlayerState* Subject);
	void PlayMusic(const TCHAR* Track, float Volume);

	UFUNCTION()
	void PlayNext();

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Playing;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Music;

	TWeakObjectPtr<UWorld> World;
	FTimerHandle SilenceTimer;
	TArray<FName> Queue;
	int32 LeaderTeam = INDEX_NONE;
	double LastLineAt = -100.0;
	FName LastLine;
};
