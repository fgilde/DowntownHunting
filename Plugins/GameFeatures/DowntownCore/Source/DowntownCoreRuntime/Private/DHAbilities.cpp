#include "DHAbilities.h"

#include "AIController.h"
#include "Components/PrimitiveComponent.h"
#include "DHKillstreaks.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AIPerceptionSystem.h"
#include "Perception/AISense_Sight.h"

namespace DHAbilities
{
	namespace
	{
		FGameplayTag CloakTag() { return FGameplayTag::RequestGameplayTag(TEXT("DH.Ability.CloakUntil")); }
		FGameplayTag AdrenalineTag() { return FGameplayTag::RequestGameplayTag(TEXT("DH.Ability.AdrenalineUntil")); }
	}

	void Push(ACharacter* Character, const FString& Id, const FVector& Direction)
	{
		if (!Character)
		{
			return;
		}
		const FVector Flat = Direction.GetSafeNormal2D().IsZero() ? Character->GetActorForwardVector().GetSafeNormal2D() : Direction.GetSafeNormal2D();
		if (Id == TEXT("Dash"))
		{
			// A low hop takes the ground friction out of it: about five metres.
			Character->LaunchCharacter(Flat * 1400.f + FVector(0.f, 0.f, 180.f), true, true);
		}
		else if (Id == TEXT("Leap"))
		{
			// About seven metres up, a bit forward.
			Character->LaunchCharacter(Flat * 350.f + FVector(0.f, 0.f, 1200.f), false, true);
		}
	}

	void Start(APawn* Pawn, const FString& Id, float Seconds)
	{
		APlayerState* Player = Pawn ? Pawn->GetPlayerState() : nullptr;
		if (!Player)
		{
			return;
		}
		if (Id == TEXT("Adrenaline"))
		{
			DHKillstreaks::SetUntil(Player, AdrenalineTag(), Seconds);
		}
		else if (Id == TEXT("Recon"))
		{
			DHKillstreaks::SetUntil(Player, DHKillstreaks::RadarTag(), Seconds);
		}
		else if (Id == TEXT("Cloak"))
		{
			DHKillstreaks::SetUntil(Player, CloakTag(), Seconds);
		}
	}

	void EndCloak(APlayerState* Player)
	{
		if (IsCloaked(Player))
		{
			DHKillstreaks::SetUntil(Player, CloakTag(), 0.f);
		}
	}

	bool HasAdrenaline(const APlayerState* Player)
	{
		return DHKillstreaks::IsActive(Player, AdrenalineTag());
	}

	bool IsCloaked(const APlayerState* Player)
	{
		return DHKillstreaks::IsActive(Player, CloakTag());
	}

	void UpdateCloak(APawn* Pawn, TArray<TWeakObjectPtr<UPrimitiveComponent>>& Hidden, bool& bHiddenFromBots)
	{
		const bool bCloaked = Pawn && IsCloaked(Pawn->GetPlayerState());
		const bool bHide = bCloaked && !Pawn->IsLocallyControlled();
		if (bHide)
		{
			// Again each time: weapons equipped meanwhile are new actors.
			TArray<AActor*> Actors;
			Pawn->GetAttachedActors(Actors, /*bResetArray=*/ true, /*bRecursivelyIncludeAttachedActors=*/ true);
			Actors.Add(Pawn);
			for (AActor* Actor : Actors)
			{
				Actor->ForEachComponent<UPrimitiveComponent>(false, [&Hidden](UPrimitiveComponent* Primitive)
				{
					if (Primitive->IsVisible() && !Primitive->bHiddenInGame)
					{
						Primitive->SetHiddenInGame(true);
						Hidden.Add(Primitive);
					}
				});
			}
		}
		else if (!bHide && !Hidden.IsEmpty())
		{
			for (const TWeakObjectPtr<UPrimitiveComponent>& Primitive : Hidden)
			{
				if (Primitive.IsValid())
				{
					Primitive->SetHiddenInGame(false);
				}
			}
			Hidden.Reset();
		}

		// Bots: out of their sight and memory while cloaked (they see through AI perception).
		UWorld* World = Pawn ? Pawn->GetWorld() : nullptr;
		UAIPerceptionSystem* Perception = World && Pawn->HasAuthority() ? UAIPerceptionSystem::GetCurrent(World) : nullptr;
		if (!Perception || bCloaked == bHiddenFromBots)
		{
			return;
		}
		bHiddenFromBots = bCloaked;
		if (bCloaked)
		{
			Perception->UnregisterSource(*Pawn, UAISense_Sight::StaticClass());
			for (TActorIterator<AAIController> It(World); It; ++It)
			{
				if (UAIPerceptionComponent* Senses = It->GetPerceptionComponent())
				{
					Senses->ForgetActor(Pawn);
				}
				if (It->GetFocusActor() == Pawn)
				{
					It->ClearFocus(EAIFocusPriority::Gameplay);
				}
			}
		}
		else
		{
			Perception->RegisterSourceForSenseClass(UAISense_Sight::StaticClass(), *Pawn);
		}
	}
}
