#include "DHKillstreaks.h"
#include "DHGameRulesComponent.h"

#include "DHSoundCue.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/PointLightComponent.h"
#include "DHLoadoutSettings.h"
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
		constexpr float BlastRadius = 550.f;
		constexpr float BlastDamage = 160.f;

		struct FStrike
		{
			const TCHAR* Id;
			int32 Impacts;
			float Spread;
		};
		const FStrike Strikes[] = { { TEXT("Mortar"), 3, 350.f }, { TEXT("Airstrike"), 6, 900.f }, { TEXT("CarpetBomb"), 16, 1800.f } };

		const FStrike* FindStrike(const FString& Id)
		{
			for (const FStrike& Strike : Strikes)
			{
				if (Id == Strike.Id)
				{
					return &Strike;
				}
			}
			return nullptr;
		}

		FGameplayTag StreakTag() { return FGameplayTag::RequestGameplayTag(TEXT("DH.Streak")); }
		FGameplayTag HunterTag() { return FGameplayTag::RequestGameplayTag(TEXT("DH.Streak.HunterUntil")); }
		FGameplayTag ReadyTag(const FString& Id) { return FGameplayTag::RequestGameplayTag(FName(TEXT("DH.Streak.Ready.") + Id), false); }

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

		// The enemy with the most other enemies around (bots' strikes), none without enemies.
		const APawn* BiggestGroup(const APlayerState* Player)
		{
			const AGameStateBase* GameState = Player->GetWorld()->GetGameState();
			const APawn* Best = nullptr;
			int32 BestCount = -1;
			for (const APlayerState* Target : GameState->PlayerArray)
			{
				const APawn* Pawn = Target->GetPawn();
				if (!IsAlive(Pawn) || !Enemies(Player, Target))
				{
					continue;
				}
				int32 Count = 0;
				for (const APlayerState* Other : GameState->PlayerArray)
				{
					const APawn* OtherPawn = Other->GetPawn();
					Count += IsAlive(OtherPawn) && Enemies(Player, Other) && FVector::Dist(OtherPawn->GetActorLocation(), Pawn->GetActorLocation()) < 1500.f;
				}
				if (Count > BestCount)
				{
					BestCount = Count;
					Best = Pawn;
				}
			}
			return Best;
		}

		void StartHunter(ALyraPlayerState* Player)
		{
			SetStat(Player, HunterTag(), FMath::CeilToInt(ServerTime(Player) + HunterSeconds));
			// Full health (LyraHealthSet is not exported: attributes by name).
			UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Player->GetPawn());
			const UClass* HealthSet = FindObject<UClass>(nullptr, TEXT("/Script/LyraGame.LyraHealthSet"));
			if (ASC && HealthSet)
			{
				const FGameplayAttribute Health(FindFProperty<FProperty>(HealthSet, TEXT("Health")));
				const FGameplayAttribute MaxHealth(FindFProperty<FProperty>(HealthSet, TEXT("MaxHealth")));
				ASC->SetNumericAttributeBase(Health, ASC->GetNumericAttribute(MaxHealth));
			}
		}

		UDHPlayerChoicesComponent* ChoicesOf(const APlayerState* Player)
		{
			const AController* Controller = Player ? Player->GetOwningController() : nullptr;
			return Controller ? Controller->FindComponentByClass<UDHPlayerChoicesComponent>() : nullptr;
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

	TArray<FString> ChosenStreaks(const APlayerState* Player)
	{
		const UDHPlayerChoicesComponent* Choices = ChoicesOf(Player);
		return Choices ? Choices->GetStreaks() : UDHLoadoutSettings::StreaksOf({});
	}

	int32 ReadyCount(const APlayerState* Player, const FString& Id)
	{
		const ALyraPlayerState* LyraPlayer = Cast<ALyraPlayerState>(Player);
		const FGameplayTag Tag = ReadyTag(Id);
		return LyraPlayer && Tag.IsValid() ? LyraPlayer->GetStatTagStackCount(Tag) : 0;
	}

	bool IsStrike(const FString& Id)
	{
		return FindStrike(Id) != nullptr;
	}

	float StrikeSpread(const FString& Id)
	{
		const FStrike* Strike = FindStrike(Id);
		return Strike ? Strike->Spread + BlastRadius * 0.5f : 0.f;
	}

	bool Use(APlayerState* Player, const FString& Id, const FVector& Target, bool bAutoTarget)
	{
		ALyraPlayerState* LyraPlayer = Cast<ALyraPlayerState>(Player);
		const FGameplayTag Tag = ReadyTag(Id);
		if (!LyraPlayer || !Tag.IsValid() || LyraPlayer->GetStatTagStackCount(Tag) <= 0 || !IsAlive(Player->GetPawn()))
		{
			return false;
		}
		FName Event = TEXT("Airstrike");
		if (Id == TEXT("Radar"))
		{
			SetStat(LyraPlayer, RadarTag(), FMath::CeilToInt(ServerTime(Player) + RadarSeconds));
			Event = TEXT("Radar");
		}
		else if (Id == TEXT("Hunter"))
		{
			StartHunter(LyraPlayer);
			Event = TEXT("HunterMode");
		}
		else if (const FStrike* Strike = FindStrike(Id))
		{
			const APawn* Group = bAutoTarget ? BiggestGroup(Player) : nullptr;
			if (bAutoTarget && !Group)
			{
				return false;  // kept for when there are enemies
			}
			// The picked spot comes without height: onto the ground (or roof) there.
			FVector Spot = Group ? Group->GetActorLocation() : Target;
			FHitResult Ground;
			if (!Group && Player->GetWorld()->LineTraceSingleByChannel(Ground, Spot + FVector(0, 0, 20000), Spot - FVector(0, 0, 20000), ECC_Visibility))
			{
				Spot = Ground.ImpactPoint;
			}
			ADHAirstrike* Actor = Player->GetWorld()->SpawnActor<ADHAirstrike>(Spot, FRotator::ZeroRotator);
			if (!Actor)
			{
				return false;
			}
			Actor->Striker = Player;
			Actor->ImpactCount = Strike->Impacts;
			Actor->Spread = Strike->Spread;
		}
		else
		{
			return false;
		}
		LyraPlayer->RemoveStatTagStack(Tag, 1);
		UE_LOG(LogTemp, Log, TEXT("DH killstreak: %s uses %s"), *Player->GetPlayerName(), *Id);
		UDHGameRulesComponent::Announce(Player, Event, INDEX_NONE, Player);
		return true;
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
		FString Earned;
		for (const FString& Id : ChosenStreaks(Killer))
		{
			const FDHStreakEntry* Entry = UDHLoadoutSettings::FindStreak(Id);
			if (Entry && Entry->Kills == Streak && ReadyTag(Id).IsValid())
			{
				LyraKiller->AddStatTagStack(ReadyTag(Id), 1);
				Earned = Id;
			}
		}
		const AController* Controller = Killer->GetOwningController();
		UDHPlayerChoicesComponent* Choices = ChoicesOf(Killer);
		if (Controller && !Controller->IsPlayerController() && !Earned.IsEmpty())
		{
			Use(Killer, Earned, FVector::ZeroVector, /*bAutoTarget=*/ true);  // bots call theirs in at once
		}
		else if (Choices && Controller && (!Earned.IsEmpty() || Streak >= 3))
		{
			Choices->ClientStreak(Streak, Earned);
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
	const float Interval = FMath::Min(0.3f, 2.4f / ImpactCount);
	const int32 Due = FMath::Clamp(FMath::FloorToInt((Age - StrikeDelay) / Interval) + 1, 0, ImpactCount);
	while (Impacts < Due)
	{
		++Impacts;
		// From the sky onto whatever is there (street or roof).
		const FVector Spot = GetActorLocation() + FVector(FMath::RandPointInCircle(Spread), 0.f);
		FHitResult Ground;
		const bool bHit = GetWorld()->LineTraceSingleByChannel(Ground, Spot + FVector(0, 0, 20000), Spot - FVector(0, 0, 20000), ECC_Visibility);
		Impact(bHit ? Ground.ImpactPoint : Spot);
	}
	if (Impacts >= ImpactCount)
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
	UDHSoundCue::PlayExplosion(this, Location, 1.5f);
}
