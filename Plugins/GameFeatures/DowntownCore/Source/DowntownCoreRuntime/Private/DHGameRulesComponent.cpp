#include "DHGameRulesComponent.h"

#include "DHKillstreaks.h"
#include "DHPlayerChoicesComponent.h"
#include "Character/LyraHealthComponent.h"
#include "Curves/CurveFloat.h"
#include "DHPlayerRulesComponent.h"
#include "DHWeaponAccess.h"
#include "DHXP.h"
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
	for (const TCHAR* Name : { TEXT("AK47"), TEXT("MP5"), TEXT("M1911") })  // Tools/Python/make_weapons.py
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

	const AGameModeBase* GameMode = GetWorld()->GetAuthGameMode();
	if (!GameMode)
	{
		return;
	}
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

TArray<UClass*> UDHGameRulesComponent::GetLoadoutItems(const AController* Controller) const
{
	TArray<FName> Names = Loadout;
	if (!GunGame.IsEmpty())
	{
		const int32 Level = Controller ? GetGunGameLevel(Controller->PlayerState) : 0;
		Names = { GunGame[FMath::Min(Level, GunGame.Num() - 1)] };
	}
	else if (bPlayerLoadout)
	{
		const UDHPlayerChoicesComponent* Choices = Controller ? Controller->FindComponentByClass<UDHPlayerChoicesComponent>() : nullptr;
		Names = Choices ? Choices->GetLoadout() : TArray<FName>();
		if (Names.IsEmpty())
		{
			static const FName Primaries[] = { TEXT("Rifle"), TEXT("Shotgun"), TEXT("AK47"), TEXT("MP5") };
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
