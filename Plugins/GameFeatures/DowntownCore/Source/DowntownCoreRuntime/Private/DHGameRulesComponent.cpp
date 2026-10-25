#include "DHGameRulesComponent.h"

#include "AbilitySystemGlobals.h"
#include "GameplayCueManager.h"
#include "DHFreeForAllScoring.h"
#include "DHKillstreaks.h"
#include "DHPlayerChoicesComponent.h"
#include "Character/LyraHealthComponent.h"
#include "Curves/CurveFloat.h"
#include "DHPlayerRulesComponent.h"
#include "DHWeaponAccess.h"
#include "DHXP.h"
#include "DHAmbience.h"
#include "DHBoundaries.h"
#include "DHAnnouncer.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Messages/LyraVerbMessage.h"
#include "Messages/LyraVerbMessageHelpers.h"
#include "Player/LyraPlayerState.h"
#include "Teams/LyraTeamSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DHGameRulesComponent)

DEFINE_LOG_CATEGORY(LogDHRules);

namespace
{
	FGameplayTag GunGameLevelTag()
	{
		return FGameplayTag::RequestGameplayTag(TEXT("DH.GunGame.Level"));
	}
}

int32 UDHGameRulesComponent::GetGunGameLevel(const APlayerState* PlayerState)
{
	const ALyraPlayerState* LyraPlayerState = Cast<ALyraPlayerState>(PlayerState);
	return LyraPlayerState ? LyraPlayerState->GetStatTagStackCount(GunGameLevelTag()) : 0;
}

UDHGameRulesComponent::UDHGameRulesComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);

	WeaponItems.Add(TEXT("Pistol"), TSoftClassPtr<UObject>(FSoftObjectPath(TEXT("/ShooterCore/Weapons/Pistol/ID_Pistol.ID_Pistol_C"))));
	WeaponItems.Add(TEXT("Rifle"), TSoftClassPtr<UObject>(FSoftObjectPath(TEXT("/ShooterCore/Weapons/Rifle/ID_Rifle.ID_Rifle_C"))));
	WeaponItems.Add(TEXT("Shotgun"), TSoftClassPtr<UObject>(FSoftObjectPath(TEXT("/ShooterCore/Weapons/Shotgun/ID_Shotgun.ID_Shotgun_C"))));
	for (const TCHAR* Name : { TEXT("AK47"), TEXT("MP5"), TEXT("M1911"), TEXT("Sniper") })  // Tools/Python/make_weapons.py
	{
		WeaponItems.Add(Name, TSoftClassPtr<UObject>(FSoftObjectPath(FString::Printf(TEXT("/DowntownCore/Weapons/%s/ID_%s.ID_%s_C"), Name, Name, Name))));
	}
}

const UDHGameRulesComponent* UDHGameRulesComponent::Find(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	return GameState ? GameState->FindComponentByClass<UDHGameRulesComponent>() : nullptr;
}

void UDHGameRulesComponent::BeginPlay()
{
	Super::BeginPlay();

	// DowntownCore's gameplay cue notifies (weapon fire, UDHSoundCue), on every machine. Like Lyra's AddGameplayCuePath
	// action, which this plugin's feature data does not have; once per run (the path is then known).
	if (UGameplayCueManager* Cues = UAbilitySystemGlobals::Get().GetGameplayCueManager())
	{
		const FString CuePath = TEXT("/DowntownCore/GameplayCues");
		if (!UAbilitySystemGlobals::Get().GetGameplayCueNotifyPaths().Contains(CuePath))
		{
			Cues->AddGameplayCueNotifyPath(CuePath);
		}
	}

	if (GetNetMode() != NM_DedicatedServer)
	{
		Announcer = NewObject<UDHAnnouncer>(this);
		Announcer->Start(GetWorld());
		Ambience = NewObject<UDHAmbience>(this);
		Ambience->Start(GetWorld());
		DHBoundaries::Show(GetWorld());
		// Team colours depend on the local player's team (DowntownCoreRuntimeModule): repaint when it changes.
		GetWorld()->GetTimerManager().SetTimer(ViewerTeamTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			ULyraTeamSubsystem* Teams = GetWorld()->GetSubsystem<ULyraTeamSubsystem>();
			const APlayerController* Viewer = GetWorld()->GetFirstPlayerController();
			const int32 Team = Teams && Viewer ? Teams->FindTeamFromObject(Viewer) : INDEX_NONE;
			if (Teams && Team != ViewerTeam)
			{
				ViewerTeam = Team;
				Teams->NotifyTeamDisplayAssetModified(nullptr);
			}
		}), 0.5f, true);
	}

	const AGameModeBase* GameMode = GetWorld()->GetAuthGameMode();
	if (!GameMode)
	{
		return;
	}
	GetWorld()->GetTimerManager().SetTimer(WatchTimer, this, &ThisClass::WatchMatch, 0.5f, true);
	const FString& Options = GameMode->OptionsString;

	const FString PerspectiveOption = UGameplayStatics::ParseOption(Options, TEXT("Perspective"));
	if (PerspectiveOption.Equals(TEXT("FP"), ESearchCase::IgnoreCase))
	{
		Perspective = EDHPerspectiveRule::FirstPerson;
	}
	else if (PerspectiveOption.Equals(TEXT("Choice"), ESearchCase::IgnoreCase))
	{
		Perspective = EDHPerspectiveRule::Choice;
	}

	bWeaponSwitch = UGameplayStatics::GetIntOption(Options, TEXT("WeaponSwitch"), 1) != 0;
	bHardcore = UGameplayStatics::GetIntOption(Options, TEXT("Hardcore"), 0) != 0;
	BotSkill = FMath::Clamp(UGameplayStatics::GetIntOption(Options, TEXT("BotSkill"), 1), 0, 3);

	TArray<FString> Names;
	UGameplayStatics::ParseOption(Options, TEXT("Loadout")).ParseIntoArray(Names, TEXT(","));
	bPlayerLoadout = Names.Num() == 1 && Names[0].TrimStartAndEnd().Equals(TEXT("Player"), ESearchCase::IgnoreCase);
	for (const FString& Name : Names)
	{
		Loadout.Add(FName(*Name.TrimStartAndEnd()));
	}
	if (!bWeaponSwitch && Loadout.Num() > 1)
	{
		Loadout.SetNum(1);
	}

	TArray<FString> GunGameNames;
	UGameplayStatics::ParseOption(Options, TEXT("GunGame")).ParseIntoArray(GunGameNames, TEXT(","));
	for (const FString& Name : GunGameNames)
	{
		GunGame.Add(FName(*Name.TrimStartAndEnd()));
	}
	if (!GunGame.IsEmpty())
	{
		bWeaponSwitch = false;
	}
	// Gun Game progress and the victims' killcam.
	EliminationListener = UGameplayMessageSubsystem::Get(this).RegisterListener(
		FGameplayTag::RequestGameplayTag(TEXT("Lyra.Elimination.Message")), this, &ThisClass::OnElimination);
	DamageListener = UGameplayMessageSubsystem::Get(this).RegisterListener(
		FGameplayTag::RequestGameplayTag(TEXT("Lyra.Damage.Message")), this, &ThisClass::OnDamage);

	ScoreLimit = UGameplayStatics::GetIntOption(Options, TEXT("ScoreLimit"), 0);
	TimeLimit = UGameplayStatics::GetIntOption(Options, TEXT("TimeLimit"), 0);
	if (ScoreLimit > 0 || TimeLimit > 0)
	{
		GetWorld()->GetTimerManager().SetTimer(LimitsTimer, this, &ThisClass::ApplyLimits, 1.0f, true);
	}

	UE_LOG(LogDHRules, Log, TEXT("Rules from '%s': perspective %d, weapon switch %d, loadout %d items, score limit %d, time limit %d min"),
		*Options, static_cast<int32>(Perspective), bWeaponSwitch ? 1 : 0, Loadout.Num(), ScoreLimit, TimeLimit);
}

void UDHGameRulesComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Modes without a match end screen: the local player's XP when the map is left.
	DHXP::FResult Result;
	DHXP::Award(GetWorld(), false, Result);
	GetWorld()->GetTimerManager().ClearTimer(LimitsTimer);
	GetWorld()->GetTimerManager().ClearTimer(WatchTimer);
	GetWorld()->GetTimerManager().ClearTimer(ViewerTeamTimer);
	if (Announcer)
	{
		Announcer->Stop();
	}
	if (Ambience)
	{
		Ambience->Stop();
	}
	if (EliminationListener.IsValid())
	{
		EliminationListener.Unregister();
	}
	if (DamageListener.IsValid())
	{
		DamageListener.Unregister();
	}
	Super::EndPlay(EndPlayReason);
}

void UDHGameRulesComponent::ApplyLimits()
{
	// Lyra's scoring blueprints (B_TeamDeathMatchScoring, B_ControlPointScoring) sit on the game state; their
	// TargetScore is recomputed from the player count, so it is enforced every second. CountDownTime is the match clock.
	for (UActorComponent* Component : GetOwner()->GetComponents())
	{
		FIntProperty* Target = FindFProperty<FIntProperty>(Component->GetClass(), TEXT("TargetScore"));
		if (ScoreLimit > 0 && Target && Target->GetPropertyValue_InContainer(Component) != ScoreLimit)
		{
			UE_LOG(LogDHRules, Log, TEXT("%s: target score %d -> %d"), *Component->GetName(), Target->GetPropertyValue_InContainer(Component), ScoreLimit);
			Target->SetPropertyValue_InContainer(Component, ScoreLimit);
			if (UFunction* OnRep = Component->FindFunction(TEXT("OnRep_TargetScore")))
			{
				Component->ProcessEvent(OnRep, nullptr);
			}
		}
		FNumericProperty* Clock = FindFProperty<FNumericProperty>(Component->GetClass(), TEXT("CountDownTime"));
		if (!Clock)
		{
			continue;
		}
		void* ClockValue = Clock->ContainerPtrToValuePtr<void>(Component);
		const double Seconds = Clock->IsFloatingPoint() ? Clock->GetFloatingPointPropertyValue(ClockValue) : Clock->GetSignedIntPropertyValue(ClockValue);
		if (TimeLimit > 0 && !bTimeLimitApplied && Seconds > 60.0)
		{
			UE_LOG(LogDHRules, Log, TEXT("%s: match clock %.0f -> %d s"), *Component->GetName(), Seconds, TimeLimit * 60);
			if (Clock->IsFloatingPoint())
			{
				Clock->SetFloatingPointPropertyValue(ClockValue, TimeLimit * 60.0);
			}
			else
			{
				Clock->SetIntPropertyValue(ClockValue, static_cast<int64>(TimeLimit * 60));
			}
			if (UFunction* OnRep = Component->FindFunction(TEXT("OnRep_CountDownTime")))
			{
				Component->ProcessEvent(OnRep, nullptr);
			}
			bTimeLimitApplied = true;
		}
	}
}

void UDHGameRulesComponent::Announce(const UObject* WorldContextObject, FName Event, int32 Team, APlayerState* Subject)
{
	UDHGameRulesComponent* Rules = const_cast<UDHGameRulesComponent*>(Find(WorldContextObject));
	if (Rules && Rules->GetOwner()->HasAuthority())
	{
		UE_LOG(LogDHRules, Log, TEXT("Announce %s (team %d, %s)"), *Event.ToString(), Team, *GetNameSafe(Subject));
		Rules->MulticastAnnounce(Event, Team, Subject);
	}
}

void UDHGameRulesComponent::MulticastAnnounce_Implementation(FName Event, int32 Team, APlayerState* Subject)
{
	if (Announcer)
	{
		Announcer->Hear(GetWorld(), Event, Team, Subject);
	}
}

void UDHGameRulesComponent::WatchMatch()
{
	// The match as Lyra's scoring blueprints and DowntownCore's scorings run it, read twice a second: the clock (start of
	// play, last minute, time up), the leading team, the owners of Lyra's control points.
	double Clock = -1.0;
	int32 TargetScore = 0;
	for (UActorComponent* Component : GetOwner()->GetComponents())
	{
		if (const FNumericProperty* Property = FindFProperty<FNumericProperty>(Component->GetClass(), TEXT("CountDownTime")))
		{
			const void* Value = Property->ContainerPtrToValuePtr<void>(Component);
			Clock = Property->IsFloatingPoint() ? Property->GetFloatingPointPropertyValue(Value) : Property->GetSignedIntPropertyValue(Value);
		}
		if (const FIntProperty* Property = FindFProperty<FIntProperty>(Component->GetClass(), TEXT("TargetScore")))
		{
			TargetScore = Property->GetPropertyValue_InContainer(Component);
		}
	}
	const UDHFreeForAllScoring* Scoring = UDHFreeForAllScoring::Find(this);
	if (Scoring)
	{
		Clock = Scoring->GetSecondsLeft();
	}
	// The clock jumps up when warmup gives way to the match.
	if (!bMatchStarted && LastClock >= 0.0 && Clock > LastClock + 30.0)
	{
		bMatchStarted = true;
		Announce(this, TEXT("MatchStart"));
	}
	if (bMatchStarted && !bOneMinuteSaid && LastClock > 60.0 && Clock <= 60.0 && Clock > 0.0)
	{
		bOneMinuteSaid = true;
		Announce(this, TEXT("OneMinute"));
	}

	// Team scores (eliminations; DowntownCore's modes count their points there too).
	const ULyraTeamSubsystem* Teams = GetWorld()->GetSubsystem<ULyraTeamSubsystem>();
	static const FGameplayTag ScoreTag = FGameplayTag::RequestGameplayTag(TEXT("ShooterGame.Score.Eliminations"));
	int32 Best = INDEX_NONE, BestScore = 0;
	bool bTie = false;
	for (const int32 Team : Teams ? Teams->GetTeamIDs() : TArray<int32>())
	{
		const int32 Score = Teams->GetTeamTagStackCount(Team, ScoreTag);
		if (Score > BestScore)
		{
			Best = Team;
			BestScore = Score;
			bTie = false;
		}
		else if (Score == BestScore && Score > 0)
		{
			bTie = true;
		}
	}
	if (bMatchStarted && !bTie && Best != INDEX_NONE && Best != Leader && !bMatchEndSaid)
	{
		Leader = Best;
		Announce(this, TEXT("Lead"), Best);
	}
	// Match over: DowntownCore's scorings know the winner; Lyra's modes end at the target score or when time is up.
	if (!bMatchEndSaid)
	{
		const bool bOver = Scoring ? Scoring->IsMatchOver()
			: bMatchStarted && ((TargetScore > 0 && BestScore >= TargetScore) || (LastClock > 0.0 && Clock <= 0.0));
		if (bOver)
		{
			bMatchEndSaid = true;
			Announce(this, TEXT("MatchEnd"), Scoring ? Scoring->GetWinnerTeam() : bTie ? INDEX_NONE : Best);
		}
	}
	LastClock = Clock;

	// Lyra's control points (Domination): A, B, C in the order of their names.
	TArray<AActor*> Points;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->GetClass()->GetName().Contains(TEXT("ControlPointVolume")))
		{
			Points.Add(*It);
		}
	}
	Points.Sort([](const AActor& A, const AActor& B) { return A.GetFName().LexicalLess(B.GetFName()); });
	for (int32 Index = 0; Index < Points.Num() && Index < 3; ++Index)
	{
		const FIntProperty* Property = FindFProperty<FIntProperty>(Points[Index]->GetClass(), TEXT("OwnerTeamID"));
		const int32 Owner = Property ? Property->GetPropertyValue_InContainer(Points[Index]) : INDEX_NONE;
		int32& Known = PointOwners.FindOrAdd(Points[Index], Owner);
		if (Owner != Known)
		{
			Known = Owner;
			if (Owner >= 0)
			{
				const TCHAR* Letters[] = { TEXT("Capture_A"), TEXT("Capture_B"), TEXT("Capture_C") };
				Announce(this, Letters[Index], Owner);
			}
		}
	}
}

TArray<UClass*> UDHGameRulesComponent::GetLoadoutItems(const AController* Controller) const
{
	TArray<FName> Names = Loadout;
	if (!GunGame.IsEmpty())
	{
		const int32 Level = Controller ? GetGunGameLevel(Controller->PlayerState) : 0;
		Names = { GunGame[FMath::Min(Level, GunGame.Num() - 1)] };
	}
	else if (const UDHInfectedScoring* Infected = Cast<UDHInfectedScoring>(UDHFreeForAllScoring::Find(this)); Infected && Infected->IsInfected(Controller))
	{
		Names = { TEXT("Shotgun") };
	}
	else if (bPlayerLoadout)
	{
		const UDHPlayerChoicesComponent* Choices = Controller ? Controller->FindComponentByClass<UDHPlayerChoicesComponent>() : nullptr;
		Names = Choices ? Choices->GetLoadout() : TArray<FName>();
		if (Names.IsEmpty())
		{
			static const FName Primaries[] = { TEXT("Rifle"), TEXT("Shotgun"), TEXT("AK47"), TEXT("MP5"), TEXT("Sniper") };
			static const FName Secondaries[] = { TEXT("Pistol"), TEXT("M1911") };
			Names = { Primaries[FMath::RandHelper(UE_ARRAY_COUNT(Primaries))], Secondaries[FMath::RandHelper(UE_ARRAY_COUNT(Secondaries))] };
		}
		if (!bWeaponSwitch)
		{
			Names.SetNum(1);
		}
	}

	TArray<UClass*> Items;
	for (const FName& Name : Names)
	{
		if (const TSoftClassPtr<UObject>* Item = WeaponItems.Find(Name))
		{
			if (UClass* Class = Item->LoadSynchronous())
			{
				Items.Add(Class);
			}
		}
	}
	return Items;
}

FName UDHGameRulesComponent::GetWeaponName(const UClass* ItemClass) const
{
	for (const TPair<FName, TSoftClassPtr<UObject>>& Item : WeaponItems)
	{
		if (Item.Value.Get() == ItemClass)
		{
			return Item.Key;
		}
	}
	return NAME_None;
}

void UDHGameRulesComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UDHGameRulesComponent, Perspective);
	DOREPLIFETIME(UDHGameRulesComponent, bWeaponSwitch);
	DOREPLIFETIME(UDHGameRulesComponent, bHardcore);
	DOREPLIFETIME(UDHGameRulesComponent, GunGame);
}

void UDHGameRulesComponent::ShowKiller(APlayerState* Killer, APlayerState* Victim) const
{
	const AController* VictimController = Victim ? Victim->GetOwningController() : nullptr;
	UDHPlayerChoicesComponent* Choices = VictimController ? VictimController->FindComponentByClass<UDHPlayerChoicesComponent>() : nullptr;
	const APawn* KillerPawn = Killer ? Killer->GetPawn() : nullptr;
	if (!Choices || !VictimController->IsPlayerController() || !KillerPawn || Killer == Victim)
	{
		return;
	}
	const ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(KillerPawn);
	const APawn* VictimPawn = Victim->GetPawn();
	Choices->ClientShowKiller(Killer, Health ? Health->GetHealth() : 0.f,
		VictimPawn ? FVector::Dist(VictimPawn->GetActorLocation(), KillerPawn->GetActorLocation()) : 0.f);
}

void UDHGameRulesComponent::OnElimination(FGameplayTag Channel, const FLyraVerbMessage& Message)
{
	APlayerState* Killer = ULyraVerbMessageHelpers::GetPlayerStateFromObject(Message.Instigator);
	APlayerState* Victim = ULyraVerbMessageHelpers::GetPlayerStateFromObject(Message.Target);
	ShowKiller(Killer, Victim);
	const ULyraTeamSubsystem* Teams = GetWorld()->GetSubsystem<ULyraTeamSubsystem>();
	const int32 KillerTeam = Teams ? Teams->FindTeamFromObject(Killer) : INDEX_NONE;
	const bool bTeamKill = KillerTeam != INDEX_NONE && KillerTeam == Teams->FindTeamFromObject(Victim);
	if (GetOwner()->HasAuthority())  // clients get the message again for the kill feed
	{
		const bool bCountsAsKill = Killer && Killer != Victim && !bTeamKill;
		DHKillstreaks::OnElimination(Killer, Victim, bCountsAsKill);
		bool bHeadshot = false;
		LastHitHeadshot.RemoveAndCopyValue(Victim, bHeadshot);
		if (bCountsAsKill)
		{
			if (ALyraPlayerState* LyraKiller = Cast<ALyraPlayerState>(Killer); LyraKiller && bHeadshot)
			{
				LyraKiller->AddStatTagStack(FGameplayTag::RequestGameplayTag(TEXT("DH.Headshots")), 1);
			}
			const AController* KillerController = Killer->GetOwningController();
			if (UDHPlayerChoicesComponent* Choices = KillerController ? KillerController->FindComponentByClass<UDHPlayerChoicesComponent>() : nullptr)
			{
				Choices->ClientHitFeedback(true, bHeadshot);
			}
			// Announcer: first blood, kills in quick succession (within 4 s of each other), headshots.
			const double Now = GetWorld()->GetTimeSeconds();
			TPair<double, int32>& Recent = RecentKills.FindOrAdd(Killer, TPair<double, int32>(-100.0, 0));
			Recent.Value = Now - Recent.Key < 4.0 ? Recent.Value + 1 : 1;
			Recent.Key = Now;
			if (++KillsInMatch == 1)
			{
				Announce(this, TEXT("FirstBlood"), INDEX_NONE, Killer);
			}
			else if (Recent.Value >= 2)
			{
				Announce(this, Recent.Value == 2 ? TEXT("DoubleKill") : Recent.Value == 3 ? TEXT("TripleKill") : TEXT("MultiKill"), INDEX_NONE, Killer);
			}
			else if (bHeadshot)
			{
				Announce(this, TEXT("Headshot"), INDEX_NONE, Killer);
			}
		}
	}
	if (GunGame.IsEmpty())
	{
		return;
	}
	if (bGunGameWon || !Killer || Killer == Victim || bTeamKill)
	{
		return;
	}

	ALyraPlayerState* LyraKiller = Cast<ALyraPlayerState>(Killer);
	if (!LyraKiller)
	{
		return;
	}
	LyraKiller->AddStatTagStack(GunGameLevelTag(), 1);
	const int32 Level = GetGunGameLevel(Killer);
	UE_LOG(LogDHRules, Log, TEXT("Gun Game: %s level %d/%d"), *Killer->GetPlayerName(), Level, GunGame.Num());
	if (Level >= GunGame.Num())
	{
		bGunGameWon = true;
		// After Lyra's scoring has counted this kill.
		GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, Message]() { WinGunGame(Message); }));
		return;
	}
	if (Level == GunGame.Num() - 1)
	{
		Announce(this, TEXT("FinalWeapon"), INDEX_NONE, Killer);
	}
	// Next tick: the kill shot's fire ability is still running on the old weapon.
	TWeakObjectPtr<APlayerState> WeakKiller = Killer;
	GetWorld()->GetTimerManager().SetTimerForNextTick([WeakKiller]()
	{
		APawn* Pawn = WeakKiller.IsValid() ? WeakKiller->GetPawn() : nullptr;
		if (UDHPlayerRulesComponent* PlayerRules = Pawn ? Pawn->FindComponentByClass<UDHPlayerRulesComponent>() : nullptr)
		{
			PlayerRules->RefreshLoadout();
		}
	});
}

namespace DHHitFeedback
{
	/** Damage of a hit without the weapon's distance falloff. */
	float WithoutFalloff(const UObject* Weapon, float Damage, float Distance)
	{
		const FStructProperty* Property = FindFProperty<FStructProperty>(Weapon->GetClass(), TEXT("DistanceDamageFalloff"));
		const FRuntimeFloatCurve* Curve = Property ? Property->ContainerPtrToValuePtr<FRuntimeFloatCurve>(Weapon) : nullptr;
		const float Falloff = Curve && Curve->GetRichCurveConst()->GetNumKeys() > 0 ? Curve->GetRichCurveConst()->Eval(Distance) : 1.f;
		return Falloff > 0.01f ? Damage / Falloff : Damage;
	}
}

int32 UDHGameRulesComponent::GetBotSkill(const APlayerState* Bot) const
{
	// Mixed: fixed per bot (from its id), so a bot keeps its skill through respawns.
	return BotSkill < 3 ? BotSkill : (Bot ? int32(GetTypeHash(Bot->GetPlayerId()) % 3) : 1);
}

void UDHGameRulesComponent::OnDamage(FGameplayTag Channel, const FLyraVerbMessage& Message)
{
	APlayerState* Shooter = ULyraVerbMessageHelpers::GetPlayerStateFromObject(Message.Instigator);
	const APlayerState* Target = ULyraVerbMessageHelpers::GetPlayerStateFromObject(Message.Target);
	const APawn* ShooterPawn = Shooter ? Shooter->GetPawn() : nullptr;
	const APawn* TargetPawn = Target ? Target->GetPawn() : nullptr;
	UObject* Weapon = DHWeaponAccess::ActiveWeapon(ShooterPawn);
	if (!Weapon || !TargetPawn || Shooter == Target)
	{
		return;
	}
	// Lyra multiplies damage on the head (weak spot physical material), but its damage message has no hit result:
	// a hit is a headshot when it does clearly more than the weapon's body damage (the lowest seen so far).
	const float Damage = DHHitFeedback::WithoutFalloff(Weapon, Message.Magnitude, FVector::Dist(ShooterPawn->GetActorLocation(), TargetPawn->GetActorLocation()));
	float& BodyDamage = WeaponBodyDamage.FindOrAdd(Weapon, Damage);
	BodyDamage = FMath::Min(BodyDamage, Damage);
	const bool bHeadshot = Damage > BodyDamage * 1.45f;
	LastHitHeadshot.Add(Target, bHeadshot);
	UE_LOG(LogDHRules, Verbose, TEXT("Hit %s -> %s: %.1f (body %.1f), headshot %d"), *Shooter->GetPlayerName(), *Target->GetPlayerName(), Damage, BodyDamage, bHeadshot);
	const AController* Controller = Shooter->GetOwningController();
	UDHPlayerChoicesComponent* Choices = Controller ? Controller->FindComponentByClass<UDHPlayerChoicesComponent>() : nullptr;
	const ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(TargetPawn);
	if (Choices && bHeadshot && Health && Health->GetHealth() > 0.f)  // kills get their marker on elimination
	{
		Choices->ClientHitFeedback(false, true);
	}
}

void UDHGameRulesComponent::WinGunGame(const FLyraVerbMessage& Message)
{
	// Lyra's scoring decides the winner: lower its target to one more than the winner's team score and replay the kill.
	// ponytail: the replayed kill counts twice in the stats; a direct victory call into the scoring blueprint would avoid that.
	const ULyraTeamSubsystem* Teams = GetWorld()->GetSubsystem<ULyraTeamSubsystem>();
	const int32 Team = Teams ? Teams->FindTeamFromObject(ULyraVerbMessageHelpers::GetPlayerStateFromObject(Message.Instigator)) : INDEX_NONE;
	if (Team == INDEX_NONE)
	{
		return;
	}
	ScoreLimit = Teams->GetTeamTagStackCount(Team, FGameplayTag::RequestGameplayTag(TEXT("ShooterGame.Score.Eliminations"))) + 1;
	ApplyLimits();
	UGameplayMessageSubsystem::Get(this).BroadcastMessage(Message.Verb, Message);
	UE_LOG(LogDHRules, Log, TEXT("Gun Game won by team %d"), Team);
}
