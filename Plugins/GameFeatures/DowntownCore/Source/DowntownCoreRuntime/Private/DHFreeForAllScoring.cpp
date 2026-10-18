#include "DHFreeForAllScoring.h"

#include "AbilitySystem/Phases/LyraGamePhaseAbility.h"
#include "AbilitySystem/Phases/LyraGamePhaseSubsystem.h"
#include "Character/LyraHealthComponent.h"
#include "DHGameRulesComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "Messages/LyraVerbMessage.h"
#include "Messages/LyraVerbMessageHelpers.h"
#include "Net/UnrealNetwork.h"
#include "Player/LyraPlayerState.h"
#include "Teams/LyraTeamDisplayAsset.h"
#include "Teams/LyraTeamSubsystem.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DHFreeForAllScoring)

namespace
{
	FGameplayTag EliminationsTag() { return FGameplayTag::RequestGameplayTag(TEXT("ShooterGame.Score.Eliminations")); }
	FGameplayTag DeathsTag() { return FGameplayTag::RequestGameplayTag(TEXT("ShooterGame.Score.Deaths")); }
}

UDHFreeForAllScoring::UDHFreeForAllScoring(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
	WarmupPhase = TSoftClassPtr<ULyraGamePhaseAbility>(FSoftObjectPath(TEXT("/ShooterCore/Experiences/Phases/Phase_Warmup.Phase_Warmup_C")));
	PostGamePhase = TSoftClassPtr<ULyraGamePhaseAbility>(FSoftObjectPath(TEXT("/ShooterCore/Experiences/Phases/Phase_PostGame.Phase_PostGame_C")));
}

UDHTeamScoring::UDHTeamScoring(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTeamScoring = true;
	KillLimit = 30;
}

const UDHFreeForAllScoring* UDHFreeForAllScoring::Find(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	return GameState ? GameState->FindComponentByClass<UDHFreeForAllScoring>() : nullptr;
}

void UDHFreeForAllScoring::BeginPlay()
{
	Super::BeginPlay();
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	EliminationListener = UGameplayMessageSubsystem::Get(this).RegisterListener(
		FGameplayTag::RequestGameplayTag(TEXT("Lyra.Elimination.Message")), this, &ThisClass::OnElimination);
	StartPhase(WarmupPhase, GET_FUNCTION_NAME_CHECKED(ThisClass, OnWarmupEnded));
	// Next tick: the rules component may begin play after this one.
	GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &ThisClass::ReadRules));
}

void UDHFreeForAllScoring::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorld()->GetTimerManager().ClearTimer(ClockTimer);
	if (EliminationListener.IsValid())
	{
		EliminationListener.Unregister();
	}
	Super::EndPlay(EndPlayReason);
}

void UDHFreeForAllScoring::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UDHFreeForAllScoring, KillLimit);
	DOREPLIFETIME(UDHFreeForAllScoring, MatchEndTime);
	DOREPLIFETIME(UDHFreeForAllScoring, WinnerName);
	DOREPLIFETIME(UDHFreeForAllScoring, Announcement);
}

void UDHFreeForAllScoring::StartPhase(const TSoftClassPtr<ULyraGamePhaseAbility>& Phase, FName EndedCallback)
{
	// ULyraGamePhaseSubsystem is not exported from LyraGame: call its Blueprint entry point.
	UWorldSubsystem* Phases = GetWorld()->GetSubsystem<ULyraGamePhaseSubsystem>();
	UFunction* Start = Phases ? Phases->FindFunction(TEXT("K2_StartPhase")) : nullptr;
	if (!Start)
	{
		return;
	}
	struct
	{
		TSubclassOf<ULyraGamePhaseAbility> Phase;
		FLyraGamePhaseDynamicDelegate PhaseEnded;
	} Params{ Phase.LoadSynchronous() };
	if (!EndedCallback.IsNone())
	{
		Params.PhaseEnded.BindUFunction(this, EndedCallback);
	}
	Phases->ProcessEvent(Start, &Params);
}

void UDHFreeForAllScoring::ReadRules()
{
	if (const UDHGameRulesComponent* Rules = UDHGameRulesComponent::Find(this))
	{
		KillLimit = Rules->GetScoreLimit() > 0 ? Rules->GetScoreLimit() : KillLimit;
		MatchMinutes = Rules->GetTimeLimit() > 0 ? Rules->GetTimeLimit() : MatchMinutes;
	}
}

void UDHFreeForAllScoring::OnWarmupEnded(const ULyraGamePhaseAbility* Phase)
{
	StartClock();
	UE_LOG(LogDHRules, Log, TEXT("Match: limit %d, %d minutes"), KillLimit, MatchMinutes);
}

void UDHFreeForAllScoring::StartClock()
{
	SetClock(MatchMinutes * 60.0);
}

void UDHFreeForAllScoring::SetClock(double Seconds)
{
	MatchEndTime = GetGameStateChecked<AGameStateBase>()->GetServerWorldTimeSeconds() + Seconds;
	GetWorld()->GetTimerManager().SetTimer(ClockTimer, this, &ThisClass::OnClockExpired, Seconds, false);
}

float UDHFreeForAllScoring::GetSecondsLeft() const
{
	const AGameStateBase* GameState = GetGameState<AGameStateBase>();
	return MatchEndTime > 0.0 && GameState ? FMath::Max(0.0, MatchEndTime - GameState->GetServerWorldTimeSeconds()) : -1.0f;
}

int32 UDHFreeForAllScoring::GetKills(const APlayerState* PlayerState)
{
	const ALyraPlayerState* LyraPlayerState = Cast<ALyraPlayerState>(PlayerState);
	return LyraPlayerState ? LyraPlayerState->GetStatTagStackCount(EliminationsTag()) : 0;
}

int32 UDHFreeForAllScoring::GetTeamKills(const UObject* WorldContextObject, int32 TeamId)
{
	const ULyraTeamSubsystem* Teams = WorldContextObject ? WorldContextObject->GetWorld()->GetSubsystem<ULyraTeamSubsystem>() : nullptr;
	return Teams ? Teams->GetTeamTagStackCount(TeamId, EliminationsTag()) : 0;
}

const APlayerState* UDHFreeForAllScoring::GetLeader() const
{
	const APlayerState* Leader = nullptr;
	for (const APlayerState* PlayerState : GetGameStateChecked<AGameStateBase>()->PlayerArray)
	{
		if (GetKills(PlayerState) > (Leader ? GetKills(Leader) : 0))
		{
			Leader = PlayerState;
		}
	}
	return Leader;
}

void UDHFreeForAllScoring::OnElimination(FGameplayTag Channel, const FLyraVerbMessage& Message)
{
	ALyraPlayerState* Killer = Cast<ALyraPlayerState>(ULyraVerbMessageHelpers::GetPlayerStateFromObject(Message.Instigator));
	ALyraPlayerState* Victim = Cast<ALyraPlayerState>(ULyraVerbMessageHelpers::GetPlayerStateFromObject(Message.Target));
	if (IsMatchOver() || MatchEndTime <= 0.0)
	{
		return;  // warmup or post game
	}
	if (Victim)
	{
		Victim->AddStatTagStack(DeathsTag(), 1);
		OnDeath(Victim);
	}
	if (!Killer || Killer == Victim)
	{
		return;
	}
	ULyraTeamSubsystem* Teams = GetWorld()->GetSubsystem<ULyraTeamSubsystem>();
	const int32 KillerTeam = Teams ? Teams->FindTeamFromObject(Killer) : INDEX_NONE;
	if (bTeamScoring && KillerTeam != INDEX_NONE && KillerTeam == Teams->FindTeamFromObject(Victim))
	{
		return;  // team kill
	}
	Killer->AddStatTagStack(EliminationsTag(), 1);
	OnKill(Killer, Victim, KillerTeam);
}

void UDHFreeForAllScoring::OnKill(ALyraPlayerState* Killer, ALyraPlayerState* Victim, int32 KillerTeam)
{
	if (ULyraTeamSubsystem* Teams = GetWorld()->GetSubsystem<ULyraTeamSubsystem>())
	{
		Teams->AddTeamTagStack(KillerTeam, EliminationsTag(), 1);  // also Lyra's scoreboard team rows
	}
	if ((bTeamScoring ? GetTeamKills(this, KillerTeam) : GetKills(Killer)) >= KillLimit)
	{
		EndMatch();
	}
}

void UDHFreeForAllScoring::EndMatch()
{
	if (IsMatchOver())
	{
		return;
	}
	GetWorld()->GetTimerManager().ClearTimer(ClockTimer);
	if (bTeamScoring)
	{
		ULyraTeamSubsystem* Teams = GetWorld()->GetSubsystem<ULyraTeamSubsystem>();
		int32 Best = INDEX_NONE;
		for (const int32 TeamId : Teams ? Teams->GetTeamIDs() : TArray<int32>())
		{
			if (Best == INDEX_NONE || GetTeamKills(this, TeamId) > GetTeamKills(this, Best))
			{
				Best = TeamId;
			}
		}
		const ULyraTeamDisplayAsset* Display = Best != INDEX_NONE ? Teams->GetTeamDisplayAsset(Best, INDEX_NONE) : nullptr;
		WinnerName = Display && !Display->TeamShortName.IsEmpty() ? Display->TeamShortName.ToString().ToUpper() + TEXT(" TEAM")
			: FString::Printf(TEXT("TEAM %d"), Best);
	}
	else
	{
		const APlayerState* Leader = GetLeader();
		WinnerName = Leader ? Leader->GetPlayerName() : TEXT("-");
	}
	UE_LOG(LogDHRules, Log, TEXT("Match won by %s"), *WinnerName);
	StartPhase(PostGamePhase, NAME_None);
}

UDHRoundScoring::UDHRoundScoring(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTeamScoring = true;
	KillLimit = 4;  // rounds to win
	MatchMinutes = 2;  // per round
}

void UDHRoundScoring::StartClock()
{
	++Round;
	bRoundOver = false;
	Announcement = FString::Printf(TEXT("ROUND %d"), Round);
	SetClock(MatchMinutes * 60.0);
	GetWorld()->GetTimerManager().SetTimer(RoundTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (!bRoundOver)
		{
			Announcement.Reset();
		}
	}), 3.f, false);
}

TMap<int32, int32> UDHRoundScoring::CountAlive() const
{
	TMap<int32, int32> Alive;
	const ULyraTeamSubsystem* Teams = GetWorld()->GetSubsystem<ULyraTeamSubsystem>();
	for (const APlayerState* PlayerState : GetGameStateChecked<AGameStateBase>()->PlayerArray)
	{
		const int32 Team = Teams ? Teams->FindTeamFromObject(PlayerState) : INDEX_NONE;
		if (Team == INDEX_NONE)
		{
			continue;
		}
		const ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(PlayerState->GetPawn());
		Alive.FindOrAdd(Team) += Health && !Health->IsDeadOrDying() ? 1 : 0;
	}
	return Alive;
}

void UDHRoundScoring::OnDeath(ALyraPlayerState* Victim)
{
	// The victim's health component switches to dying right after this message.
	GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &ThisClass::CheckRoundOver));
}

void UDHRoundScoring::CheckRoundOver()
{
	if (bRoundOver || IsMatchOver())
	{
		return;
	}
	int32 Standing = INDEX_NONE;
	int32 TeamsStanding = 0;
	for (const TPair<int32, int32>& Team : CountAlive())
	{
		if (Team.Value > 0)
		{
			Standing = Team.Key;
			++TeamsStanding;
		}
	}
	if (TeamsStanding <= 1)
	{
		EndRound(Standing);
	}
}

void UDHRoundScoring::OnClockExpired()
{
	// Time is up: more survivors win, a tie gives nobody the round.
	int32 Best = INDEX_NONE, BestAlive = 0;
	bool bTie = false;
	for (const TPair<int32, int32>& Team : CountAlive())
	{
		if (Team.Value > BestAlive)
		{
			Best = Team.Key;
			BestAlive = Team.Value;
			bTie = false;
		}
		else if (Team.Value == BestAlive)
		{
			bTie = true;
		}
	}
	EndRound(bTie ? INDEX_NONE : Best);
}

void UDHRoundScoring::EndRound(int32 WinnerTeam)
{
	bRoundOver = true;
	GetWorld()->GetTimerManager().ClearTimer(ClockTimer);
	ULyraTeamSubsystem* Teams = GetWorld()->GetSubsystem<ULyraTeamSubsystem>();
	if (Teams && WinnerTeam != INDEX_NONE)
	{
		Teams->AddTeamTagStack(WinnerTeam, EliminationsTag(), 1);  // round wins are the team score
		const ULyraTeamDisplayAsset* Display = Teams->GetTeamDisplayAsset(WinnerTeam, INDEX_NONE);
		const FString Name = Display && !Display->TeamShortName.IsEmpty() ? Display->TeamShortName.ToString().ToUpper() : FString::Printf(TEXT("TEAM %d"), WinnerTeam);
		Announcement = FString::Printf(TEXT("%s TEAM WINS ROUND %d"), *Name, Round);
	}
	else
	{
		Announcement = FString::Printf(TEXT("ROUND %d: DRAW"), Round);
	}
	UE_LOG(LogDHRules, Log, TEXT("%s"), *Announcement);

	if (WinnerTeam != INDEX_NONE && GetTeamKills(this, WinnerTeam) >= KillLimit)
	{
		EndMatch();
		return;
	}
	GetWorld()->GetTimerManager().SetTimer(RoundTimer, this, &ThisClass::StartNextRound, 5.f, false);
}

void UDHRoundScoring::StartNextRound()
{
	// Everybody back to a spawn point, then the next round's clock.
	AGameModeBase* GameMode = GetWorld()->GetAuthGameMode();
	UFunction* Restart = GameMode ? GameMode->FindFunction(TEXT("RequestPlayerRestartNextFrame")) : nullptr;  // ALyraGameMode, not exported
	for (FConstControllerIterator It = GetWorld()->GetControllerIterator(); It; ++It)
	{
		AController* Controller = It->Get();
		if (!Controller || !Controller->PlayerState || Controller->PlayerState->IsOnlyASpectator())
		{
			continue;
		}
		if (APawn* Pawn = Controller->GetPawn())
		{
			Pawn->DetachFromControllerPendingDestroy();
			Pawn->Destroy();
		}
		if (Restart)
		{
			struct
			{
				AController* Controller;
				bool bForceReset;
			} Params{ Controller, false };
			GameMode->ProcessEvent(Restart, &Params);
		}
	}
	StartClock();
}
