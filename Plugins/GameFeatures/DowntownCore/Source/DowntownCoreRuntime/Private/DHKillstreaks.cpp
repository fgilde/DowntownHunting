#include "DHKillstreaks.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/PointLightComponent.h"
#include "DHPlayerChoicesComponent.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "LyraGameplayTags.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Player/LyraPlayerState.h"
#include "Sound/SoundBase.h"
#include "System/LyraGameData.h"
#include "Teams/LyraTeamSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DHKillstreaks)

namespace DHKillstreaks
{
	namespace
	{
		constexpr float StrikeDelay = 3.f;
		constexpr float StrikeSpread = 900.f;
		constexpr float BlastRadius = 550.f;
		constexpr float BlastDamage = 160.f;
		constexpr int32 StrikeImpacts = 6;

		FGameplayTag StreakTag() { return FGameplayTag::RequestGameplayTag(TEXT("DH.Streak")); }
		FGameplayTag HunterTag() { return FGameplayTag::RequestGameplayTag(TEXT("DH.Streak.HunterUntil")); }

		double ServerTime(const UObject* Context)
		{
			const AGameStateBase* GameState = Context && Context->GetWorld() ? Context->GetWorld()->GetGameState() : nullptr;
			return GameState ? GameState->GetServerWorldTimeSeconds() : 0.0;
		}

		void SetStat(ALyraPlayerState* Player, FGameplayTag Tag, int32 Value)
		{
			Player->RemoveStatTagStack(Tag, Player->GetStatTagStackCount(Tag));
			if (Value > 0)
			{
				Player->AddStatTagStack(Tag, Value);
			}
		}

		bool Active(const APlayerState* Player, FGameplayTag Tag)
		{
			const ALyraPlayerState* LyraPlayer = Cast<ALyraPlayerState>(Player);
			return LyraPlayer && LyraPlayer->GetStatTagStackCount(Tag) > ServerTime(Player);
		}

		bool Enemies(const APlayerState* A, const APlayerState* B)
		{
			const ULyraTeamSubsystem* Teams = A->GetWorld()->GetSubsystem<ULyraTeamSubsystem>();
			const int32 TeamA = Teams ? Teams->FindTeamFromObject(A) : INDEX_NONE;
			return A != B && (TeamA == INDEX_NONE || TeamA != Teams->FindTeamFromObject(B));
		}

		bool IsAlive(const APawn* Pawn)
		{
			static const FGameplayTag DeathTag = FGameplayTag::RequestGameplayTag(TEXT("Status.Death"));
			const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn);
			return Pawn && !(ASC && ASC->HasMatchingGameplayTag(DeathTag));
		}

		void CallAirstrike(APlayerState* Killer)
		{
			// On the enemy with the most other enemies around.
			const AGameStateBase* GameState = Killer->GetWorld()->GetGameState();
			const APawn* Best = nullptr;
			int32 BestCount = -1;
			for (const APlayerState* Target : GameState->PlayerArray)
			{
				const APawn* Pawn = Target->GetPawn();
				if (!IsAlive(Pawn) || !Enemies(Killer, Target))
				{
					continue;
				}
				int32 Count = 0;
				for (const APlayerState* Other : GameState->PlayerArray)
				{
					const APawn* OtherPawn = Other->GetPawn();
					Count += IsAlive(OtherPawn) && Enemies(Killer, Other) && FVector::Dist(OtherPawn->GetActorLocation(), Pawn->GetActorLocation()) < 1500.f;
				}
				if (Count > BestCount)
				{
					BestCount = Count;
					Best = Pawn;
				}
			}
			if (Best)
			{
				UE_LOG(LogTemp, Log, TEXT("DH killstreak: airstrike of %s on %s (%d enemies close)"), *Killer->GetPlayerName(), *GetNameSafe(Best), BestCount);
				ADHAirstrike* Strike = Killer->GetWorld()->SpawnActor<ADHAirstrike>(Best->GetActorLocation(), FRotator::ZeroRotator);
				Strike->Striker = Killer;
			}
		}

		void StartHunter(ALyraPlayerState* Killer)
		{
			SetStat(Killer, HunterTag(), FMath::CeilToInt(ServerTime(Killer) + HunterSeconds));
			// Full health (LyraHealthSet is not exported: attributes by name).
			UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Killer->GetPawn());
			const UClass* HealthSet = FindObject<UClass>(nullptr, TEXT("/Script/LyraGame.LyraHealthSet"));
			if (ASC && HealthSet)
			{
				const FGameplayAttribute Health(FindFProperty<FProperty>(HealthSet, TEXT("Health")));
				const FGameplayAttribute MaxHealth(FindFProperty<FProperty>(HealthSet, TEXT("MaxHealth")));
				ASC->SetNumericAttributeBase(Health, ASC->GetNumericAttribute(MaxHealth));
			}
		}
	}

	FGameplayTag RadarTag() { return FGameplayTag::RequestGameplayTag(TEXT("DH.Streak.RadarUntil")); }

	void SetUntil(APlayerState* Player, FGameplayTag Tag, float Seconds)
	{
		if (ALyraPlayerState* LyraPlayer = Cast<ALyraPlayerState>(Player))
		{
			SetStat(LyraPlayer, Tag, Seconds > 0.f ? FMath::CeilToInt(ServerTime(Player) + Seconds) : 0);
		}
	}

	bool IsActive(const APlayerState* Player, FGameplayTag Tag)
	{
		return Active(Player, Tag);
	}

	void OnElimination(APlayerState* Killer, APlayerState* Victim, bool bCountsAsKill)
	{
		if (ALyraPlayerState* LyraVictim = Cast<ALyraPlayerState>(Victim))
		{
			SetStat(LyraVictim, StreakTag(), 0);
			SetStat(LyraVictim, HunterTag(), 0);
		}
		ALyraPlayerState* LyraKiller = Cast<ALyraPlayerState>(Killer);
		if (!LyraKiller || !bCountsAsKill)
		{
			return;
		}
		LyraKiller->AddStatTagStack(StreakTag(), 1);
		const int32 Streak = LyraKiller->GetStatTagStackCount(StreakTag());
		EReward Reward = EReward::None;
		switch (Streak)
		{
		case 3:
			Reward = EReward::Radar;
			SetStat(LyraKiller, RadarTag(), FMath::CeilToInt(ServerTime(Killer) + RadarSeconds));
			break;
		case 5:
			Reward = EReward::Airstrike;
			CallAirstrike(Killer);
			break;
		case 7:
			Reward = EReward::Hunter;
			StartHunter(LyraKiller);
			break;
		default:
			break;
		}
		const AController* Controller = Killer->GetOwningController();
		UDHPlayerChoicesComponent* Choices = Controller ? Controller->FindComponentByClass<UDHPlayerChoicesComponent>() : nullptr;
		if (Choices && Controller->IsPlayerController() && (Reward != EReward::None || Streak >= 3))
		{
			Choices->ClientStreak(Streak, static_cast<uint8>(Reward));
		}
	}

	bool HasRadar(const APlayerState* Viewer)
	{
		const AGameStateBase* GameState = Viewer && Viewer->GetWorld() ? Viewer->GetWorld()->GetGameState() : nullptr;
		if (!GameState)
		{
			return false;
		}
		for (const APlayerState* Player : GameState->PlayerArray)
		{
			if ((Player == Viewer || !Enemies(Viewer, Player)) && Active(Player, RadarTag()))
			{
				return true;
			}
		}
		return false;
	}

	bool IsHunting(const APlayerState* Player)
	{
		return Active(Player, HunterTag());
	}

	// Testing: credit the first local player with kills (offline / listen server).
	FAutoConsoleCommandWithWorldAndArgs StreakCommand(TEXT("DH.Streak"), TEXT("Count N kills for the first player: DH.Streak [N]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
			const int32 Kills = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 1;
			for (int32 Index = 0; Player && Player->PlayerState && Player->HasAuthority() && Index < Kills; ++Index)
			{
				OnElimination(Player->PlayerState, nullptr, true);
			}
		}));

	FText RewardText(int32 Streak, EReward Reward)
	{
		switch (Reward)
		{
		case EReward::Radar: return NSLOCTEXT("DowntownHunting", "StreakRadar", "RADAR  ·  enemies on the map");
		case EReward::Airstrike: return NSLOCTEXT("DowntownHunting", "StreakAirstrike", "AIRSTRIKE  ·  inbound on the enemy");
		case EReward::Hunter: return NSLOCTEXT("DowntownHunting", "StreakHunter", "HUNTER MODE  ·  full health, faster");
		default: return FText::Format(NSLOCTEXT("DowntownHunting", "StreakCount", "{0} KILL STREAK"), Streak);
		}
	}
}

ADHAirstrike::ADHAirstrike()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(false);
	Flare = CreateDefaultSubobject<UPointLightComponent>(TEXT("Flare"));
	Flare->SetLightColor(FLinearColor(1.f, 0.08f, 0.02f));
	Flare->SetAttenuationRadius(1600.f);
	Flare->SetCastShadows(false);
	RootComponent = Flare;
}

void ADHAirstrike::BeginPlay()
{
	Super::BeginPlay();
	ExplosionEffect = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/Effects/Particles/Explosion/NS_Grenade_Explosion.NS_Grenade_Explosion"));
	ExplosionSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/MetaSounds/sfx_Weapon_GrenadeExplosion_nl_meta.sfx_Weapon_GrenadeExplosion_nl_meta"));
	SetActorLocation(GetActorLocation() + FVector(0, 0, 120));
}

void ADHAirstrike::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	using namespace DHKillstreaks;
	Age += DeltaSeconds;
	// Blinking faster and faster until the strike.
	const float Rate = 2.f + Age * 3.f;
	Flare->SetIntensity(FMath::Frac(Age * Rate) < 0.5f && Age < StrikeDelay ? 60000.f : 0.f);
	if (!HasAuthority())
	{
		return;
	}
	const int32 Due = FMath::Clamp(FMath::FloorToInt((Age - StrikeDelay) / 0.3f) + 1, 0, StrikeImpacts);
	while (Impacts < Due)
	{
		++Impacts;
		const FVector2D Offset = FMath::RandPointInCircle(StrikeSpread);
		Impact(GetActorLocation() + FVector(Offset, 0.f));
	}
	if (Impacts >= StrikeImpacts)
	{
		SetLifeSpan(1.f);
		SetActorTickEnabled(false);
	}
}

void ADHAirstrike::Impact(const FVector& Location)
{
	using namespace DHKillstreaks;
	MulticastImpact(Location);
	APlayerState* Killer = Striker.Get();
	APawn* KillerPawn = Killer ? Killer->GetPawn() : nullptr;
	UAbilitySystemComponent* SourceASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Killer);
	const TSubclassOf<UGameplayEffect> DamageEffect = ULyraGameData::Get().DamageGameplayEffect_SetByCaller.LoadSynchronous();
	if (!Killer || !SourceASC || !DamageEffect)
	{
		return;
	}
	for (const APlayerState* Target : GetWorld()->GetGameState()->PlayerArray)
	{
		APawn* Pawn = Target->GetPawn();
		const float Distance = Pawn ? FVector::Dist(Pawn->GetActorLocation(), Location) : BIG_NUMBER;
		if (Distance > BlastRadius || !IsAlive(Pawn) || !Enemies(Killer, Target))
		{
			continue;
		}
		// From the sky: whoever has a roof over the head is safe.
		FHitResult Hit;
		FCollisionQueryParams Params(TEXT("DHAirstrike"), false, Pawn);
		if (GetWorld()->LineTraceSingleByChannel(Hit, Pawn->GetActorLocation() + FVector(0, 0, 3000), Pawn->GetActorLocation(), ECC_Visibility, Params))
		{
			continue;
		}
		UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn);
		if (!TargetASC)
		{
			continue;
		}
		// The killer is instigator and causer: the kill is theirs, team rules apply.
		FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
		Context.AddInstigator(KillerPawn ? static_cast<AActor*>(KillerPawn) : Killer, KillerPawn ? static_cast<AActor*>(KillerPawn) : this);
		Context.AddOrigin(Location);
		FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(DamageEffect, 1.f, Context);
		Spec.Data->SetSetByCallerMagnitude(LyraGameplayTags::SetByCaller_Damage, BlastDamage * (1.f - 0.6f * Distance / BlastRadius));
		SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data, TargetASC);
	}
}

void ADHAirstrike::MulticastImpact_Implementation(FVector_NetQuantize Location)
{
	if (ExplosionEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, ExplosionEffect, Location, FRotator::ZeroRotator, FVector(2.5f));
	}
	if (ExplosionSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ExplosionSound, Location, 1.5f);
	}
}
