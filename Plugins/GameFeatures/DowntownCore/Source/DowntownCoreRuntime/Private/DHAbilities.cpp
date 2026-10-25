#include "DHAbilities.h"

#include "AIController.h"
#include "Components/MeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Teams/LyraTeamSubsystem.h"
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

	void UpdateCloak(APawn* Pawn, FDHCloakState& State)
	{
		const bool bCloaked = Pawn && IsCloaked(Pawn->GetPlayerState());
		UWorld* World = Pawn ? Pawn->GetWorld() : nullptr;
		if (World && bCloaked != State.bWasCloaked && World->GetNetMode() != NM_DedicatedServer)
		{
			// Where it vanishes or shows up again: Lyra's portal dissolve.
			if (UNiagaraSystem* Dissolve = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/Effects/Particles/Environmental/NS_CharacterPortalDissolve.NS_CharacterPortalDissolve"), nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Dissolve, Pawn->GetActorLocation());
			}
		}
		State.bWasCloaked = bCloaked;

		// The local viewer's relation decides: enemies see nothing, the owner and the team a shimmer.
		const APlayerController* Viewer = World ? World->GetFirstPlayerController() : nullptr;
		const ULyraTeamSubsystem* Teams = World ? World->GetSubsystem<ULyraTeamSubsystem>() : nullptr;
		const bool bFriendly = Pawn && (Pawn->IsLocallyControlled()
			|| (Viewer && Teams && Teams->CompareTeams(Pawn, Viewer) == ELyraTeamComparison::OnSameTeam));
		const bool bHide = bCloaked && !bFriendly;
		const bool bShimmer = bCloaked && bFriendly;

		TArray<AActor*> Actors;
		if (bCloaked && Pawn)
		{
			// Again each time: weapons and first-person copies made meanwhile are new.
			Pawn->GetAttachedActors(Actors, /*bResetArray=*/ true, /*bRecursivelyIncludeAttachedActors=*/ true);
			Actors.Add(Pawn);
		}
		if (bHide)
		{
			for (AActor* Actor : Actors)
			{
				Actor->ForEachComponent<UPrimitiveComponent>(false, [&State](UPrimitiveComponent* Primitive)
				{
					if (Primitive->IsVisible() && !Primitive->bHiddenInGame)
					{
						Primitive->SetHiddenInGame(true);
						State.Hidden.Add(Primitive);
					}
				});
			}
		}
		else if (!State.Hidden.IsEmpty())
		{
			for (const TWeakObjectPtr<UPrimitiveComponent>& Primitive : State.Hidden)
			{
				if (Primitive.IsValid())
				{
					Primitive->SetHiddenInGame(false);
				}
			}
			State.Hidden.Reset();
		}

		UMaterialInterface* Shimmer = bShimmer ? LoadObject<UMaterialInterface>(nullptr, TEXT("/DowntownCore/Effects/M_DH_Cloak.M_DH_Cloak")) : nullptr;
		if (Shimmer)
		{
			for (AActor* Actor : Actors)
			{
				Actor->ForEachComponent<UMeshComponent>(false, [&State, Shimmer](UMeshComponent* Mesh)
				{
					if (State.Swapped.Contains(Mesh) || Mesh->GetNumMaterials() == 0)
					{
						return;
					}
					// Hair (grooms) has no material slots to swap: it goes for the time being.
					TArray<UMaterialInterface*>& Originals = State.Swapped.Add(Mesh);
					for (int32 Index = 0; Index < Mesh->GetNumMaterials(); ++Index)
					{
						Originals.Add(Mesh->GetMaterial(Index));
						Mesh->SetMaterial(Index, Shimmer);
					}
				});
				Actor->ForEachComponent<UPrimitiveComponent>(false, [&State](UPrimitiveComponent* Primitive)
				{
					if (Primitive->GetClass()->GetFName() == TEXT("GroomComponent") && Primitive->IsVisible() && !Primitive->bHiddenInGame)
					{
						Primitive->SetHiddenInGame(true);
						State.Hidden.Add(Primitive);
					}
				});
			}
		}
		else if (!State.Swapped.IsEmpty())
		{
			for (const TPair<TWeakObjectPtr<UMeshComponent>, TArray<UMaterialInterface*>>& Entry : State.Swapped)
			{
				for (int32 Index = 0; Entry.Key.IsValid() && Index < Entry.Value.Num(); ++Index)
				{
					Entry.Key->SetMaterial(Index, Entry.Value[Index]);
				}
			}
			State.Swapped.Reset();
		}

		// Bots: out of their sight and memory while cloaked (they see through AI perception).
		UAIPerceptionSystem* Perception = World && Pawn->HasAuthority() ? UAIPerceptionSystem::GetCurrent(World) : nullptr;
		if (!Perception || bCloaked == State.bHiddenFromBots)
		{
			return;
		}
		State.bHiddenFromBots = bCloaked;
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
