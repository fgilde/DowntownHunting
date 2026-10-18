#include "DHFreeForAllScoring.h"

#include "AbilitySystem/Phases/LyraGamePhaseAbility.h"
#include "AbilitySystem/Phases/LyraGamePhaseSubsystem.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "Character/LyraHealthComponent.h"
#include "DHBombSite.h"
#include "DHGameRulesComponent.h"
#include "EngineUtils.h"
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
	DOREPLIFETIME(UDHFreeForAllScoring, WinnerTeam);
	DOREPLIFETIME(UDHFreeForAllScoring, WinnerPlayer);
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

int32 UDHFreeForAllScoring::GetDeaths(const APlayerState* PlayerState)
{
	const ALyraPlayerState* LyraPlayerState = Cast<ALyraPlayerState>(PlayerState);
	return LyraPlayerState ? LyraPlayerState->GetStatTagStackCount(DeathsTag()) : 0;
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
		WinnerTeam = Best;
		const ULyraTeamDisplayAsset* Display = Best != INDEX_NONE ? Teams->GetTeamDisplayAsset(Best, INDEX_NONE) : nullptr;
		WinnerName = Display && !Display->TeamShortName.IsEmpty() ? Display->TeamShortName.ToString().ToUpper() + TEXT(" TEAM")
			: FString::Printf(TEXT("TEAM %d"), Best);
	}
	else
	{
		const APlayerState* Leader = GetLeader();
		WinnerPlayer = const_cast<APlayerState*>(Leader);
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

void UDHRoundScoring::EndRound(int32 RoundWinner, const FString& Reason)
{
	bRoundOver = true;
	GetWorld()->GetTimerManager().ClearTimer(ClockTimer);
	ULyraTeamSubsystem* Teams = GetWorld()->GetSubsystem<ULyraTeamSubsystem>();
	if (Teams && RoundWinner != INDEX_NONE)
	{
		Teams->AddTeamTagStack(RoundWinner, EliminationsTag(), 1);  // round wins are the team score
		const ULyraTeamDisplayAsset* Display = Teams->GetTeamDisplayAsset(RoundWinner, INDEX_NONE);
		const FString Name = Display && !Display->TeamShortName.IsEmpty() ? Display->TeamShortName.ToString().ToUpper() : FString::Printf(TEXT("TEAM %d"), RoundWinner);
		Announcement = Reason.IsEmpty() ? FString::Printf(TEXT("%s TEAM WINS ROUND %d"), *Name, Round)
			: FString::Printf(TEXT("%s - %s TEAM WINS ROUND %d"), *Reason, *Name, Round);
	}
	else
	{
		Announcement = FString::Printf(TEXT("ROUND %d: DRAW"), Round);
	}
	UE_LOG(LogDHRules, Log, TEXT("%s"), *Announcement);

	if (RoundWinner != INDEX_NONE && GetTeamKills(this, RoundWinner) >= KillLimit)
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

UDHSearchAndDestroyScoring::UDHSearchAndDestroyScoring(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	KillLimit = 5;  // rounds to win
	MatchMinutes = 2;  // to plant
}

void UDHSearchAndDestroyScoring::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UDHSearchAndDestroyScoring, AttackingTeam);
	DOREPLIFETIME(UDHSearchAndDestroyScoring, Sites);
	DOREPLIFETIME(UDHSearchAndDestroyScoring, PlantedSite);
	DOREPLIFETIME(UDHSearchAndDestroyScoring, Progress);
	DOREPLIFETIME(UDHSearchAndDestroyScoring, bDefusing);
}

void UDHSearchAndDestroyScoring::BeginPlay()
{
	Super::BeginPlay();
	if (!GetOwner()->HasAuthority())
	{
		return;
	}
	// Sites at the first and last control point of the map (placed for Domination: far apart, reachable by bots).
	TArray<AActor*> Points;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->GetClass()->GetName().Contains(TEXT("ControlPoint")))
		{
			Points.Add(*It);
		}
	}
	Points.Sort([](const AActor& A, const AActor& B) { return A.GetName() < B.GetName(); });
	// Bots have to walk there: drop points the navmesh does not reach from a player start (map edges).
	if (const AActor* Start = UGameplayStatics::GetActorOfClass(this, APlayerStart::StaticClass()))
	{
		TArray<AActor*> Reachable = Points.FilterByPredicate([this, Start](const AActor* Point)
		{
			const UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(this, Start->GetActorLocation(), Point->GetActorLocation());
			return Path && Path->IsValid() && !Path->IsPartial();
		});
		UE_LOG(LogDHRules, Log, TEXT("Search & Destroy: %d of %d control points reachable"), Reachable.Num(), Points.Num());
		if (Reachable.Num() >= 2)
		{
			Points = Reachable;
		}
	}
	if (Points.Num() < 2)
	{
		UE_LOG(LogDHRules, Warning, TEXT("Search & Destroy: the map has %d control points, needs 2 for the bomb sites"), Points.Num());
		return;
	}
	// Lyra's bots roam between control point volumes when they see no enemy (EQS_FindWeapon / EQS_Context_ControlPoints).
	// Only the two at the sites stay, so bots of both teams head for A and B, and attackers alone on a site plant.
	for (int32 Index = 1; Index < Points.Num() - 1; ++Index)
	{
		Points[Index]->Destroy();
	}
	for (const AActor* Point : { Points[0], Points.Last() })
	{
		ADHBombSite* Site = GetWorld()->SpawnActor<ADHBombSite>(Point->GetActorLocation(), FRotator::ZeroRotator);
		Site->SetState(Sites.IsEmpty() ? TEXT("A") : TEXT("B"), false);
		Sites.Add(Site);
		UE_LOG(LogDHRules, Log, TEXT("Search & Destroy: site %s at %s"), *Site->Letter, *Site->GetActorLocation().ToString());
	}
}

int32 UDHSearchAndDestroyScoring::DefendingTeam() const
{
	const ULyraTeamSubsystem* Teams = GetWorld()->GetSubsystem<ULyraTeamSubsystem>();
	for (const int32 Team : Teams ? Teams->GetTeamIDs() : TArray<int32>())
	{
		if (Team != AttackingTeam)
		{
			return Team;
		}
	}
	return INDEX_NONE;
}

void UDHSearchAndDestroyScoring::StartClock()
{
	Super::StartClock();
	// Teams swap attack and defence every round.
	const ULyraTeamSubsystem* Teams = GetWorld()->GetSubsystem<ULyraTeamSubsystem>();
	TArray<int32> TeamIds = Teams ? Teams->GetTeamIDs() : TArray<int32>();
	TeamIds.Sort();
	AttackingTeam = TeamIds.IsEmpty() ? INDEX_NONE : TeamIds[(Round - 1) % TeamIds.Num()];
	PlantedSite = nullptr;
	Progress = 0.f;
	bDefusing = false;
	for (ADHBombSite* Site : Sites)
	{
		Site->SetState(Site->Letter, false);
	}
	GetWorld()->GetTimerManager().SetTimer(ObjectiveTimer, this, &ThisClass::TickObjective, 0.1f, true);
}

void UDHSearchAndDestroyScoring::TickObjective()
{
	if (bRoundOver || IsMatchOver())
	{
		GetWorld()->GetTimerManager().ClearTimer(ObjectiveTimer);
		return;
	}
	const ULyraTeamSubsystem* Teams = GetWorld()->GetSubsystem<ULyraTeamSubsystem>();
	auto CountAt = [this, Teams](const ADHBombSite* Site, int32 Team)
	{
		int32 Count = 0;
		for (const APlayerState* PlayerState : GetGameStateChecked<AGameStateBase>()->PlayerArray)
		{
			const APawn* Pawn = PlayerState->GetPawn();
			const ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(Pawn);
			if (Health && !Health->IsDeadOrDying() && Teams->FindTeamFromObject(PlayerState) == Team && Site->Contains(Pawn->GetActorLocation()))
			{
				++Count;
			}
		}
		return Count;
	};
	const int32 Defenders = DefendingTeam();
	const float Step = 0.1f;

	if (!PlantedSite)
	{
		// Planting: attackers alone on a site.
		ADHBombSite* Busy = nullptr;
		for (ADHBombSite* Site : Sites)
		{
			if (CountAt(Site, AttackingTeam) > 0 && CountAt(Site, Defenders) == 0)
			{
				Busy = Site;
			}
		}
		Progress = Busy ? Progress + Step / PlantSeconds : FMath::Max(0.f, Progress - Step / PlantSeconds);
		if (Busy && Progress >= 1.f)
		{
			PlantedSite = Busy;
			PlantedSite->SetState(PlantedSite->Letter, true);
			Progress = 0.f;
			Announcement = FString::Printf(TEXT("BOMB PLANTED AT %s"), *PlantedSite->Letter);
			SetClock(BombSeconds);  // the round clock now counts down the bomb
			UE_LOG(LogDHRules, Log, TEXT("%s"), *Announcement);
		}
		return;
	}

	// Defusing: defenders alone on the planted site.
	bDefusing = CountAt(PlantedSite, Defenders) > 0 && CountAt(PlantedSite, AttackingTeam) == 0;
	Progress = bDefusing ? Progress + Step / DefuseSeconds : FMath::Max(0.f, Progress - Step / DefuseSeconds);
	if (Progress >= 1.f)
	{
		PlantedSite->SetState(PlantedSite->Letter, false);
		EndRound(Defenders, TEXT("BOMB DEFUSED"));
	}
}

void UDHSearchAndDestroyScoring::OnClockExpired()
{
	if (PlantedSite)
	{
		PlantedSite->Explode();
		EndRound(AttackingTeam, TEXT("BOMB EXPLODED"));
	}
	else
	{
		EndRound(DefendingTeam(), TEXT("TIME UP"));
	}
}

void UDHSearchAndDestroyScoring::CheckRoundOver()
{
	if (bRoundOver || IsMatchOver())
	{
		return;
	}
	const TMap<int32, int32> Alive = CountAlive();
	const int32 Defenders = DefendingTeam();
	if (Alive.FindRef(Defenders) == 0)
	{
		EndRound(AttackingTeam, TEXT("DEFENDERS ELIMINATED"));
	}
	else if (Alive.FindRef(AttackingTeam) == 0 && !PlantedSite)
	{
		EndRound(Defenders, TEXT("ATTACKERS ELIMINATED"));
	}
	// Attackers dead after the plant: the defenders still have to defuse.
}
