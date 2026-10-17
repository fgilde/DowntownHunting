#include "DHFreeForAllScoring.h"

#include "AbilitySystem/Phases/LyraGamePhaseAbility.h"
#include "AbilitySystem/Phases/LyraGamePhaseSubsystem.h"
#include "DHGameRulesComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Messages/LyraVerbMessage.h"
#include "Messages/LyraVerbMessageHelpers.h"
#include "Net/UnrealNetwork.h"
#include "Player/LyraPlayerState.h"
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
	MatchEndTime = GetGameStateChecked<AGameStateBase>()->GetServerWorldTimeSeconds() + MatchMinutes * 60.0;
	GetWorld()->GetTimerManager().SetTimer(ClockTimer, this, &ThisClass::EndMatch, MatchMinutes * 60.0f, false);
	UE_LOG(LogDHRules, Log, TEXT("Free-for-all: %d kills or %d minutes"), KillLimit, MatchMinutes);
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
	}
	if (!Killer || Killer == Victim)
	{
		return;
	}
	Killer->AddStatTagStack(EliminationsTag(), 1);
	if (ULyraTeamSubsystem* Teams = GetWorld()->GetSubsystem<ULyraTeamSubsystem>())
	{
		Teams->AddTeamTagStack(Teams->FindTeamFromObject(Killer), EliminationsTag(), 1);  // Lyra's scoreboard team rows
	}
	if (GetKills(Killer) >= KillLimit)
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
	const APlayerState* Leader = GetLeader();
	WinnerName = Leader ? Leader->GetPlayerName() : TEXT("-");
	UE_LOG(LogDHRules, Log, TEXT("Free-for-all won by %s"), *WinnerName);
	StartPhase(PostGamePhase, NAME_None);
}
