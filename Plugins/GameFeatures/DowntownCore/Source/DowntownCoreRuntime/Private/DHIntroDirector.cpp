#include "DHIntroDirector.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DHIntroDirector)

namespace DHIntroSettings
{
	constexpr float ShotLength = 3.f;
	constexpr float FieldOfView = 70.f;

	// Saved/IntroFrames, or Saved/Record/<Name> for other clips (Tools/Branding/make_showcase.py).
	FString FramesDir(const FString& Name) { return Name.IsEmpty() ? FPaths::ProjectSavedDir() / TEXT("IntroFrames") : FPaths::ProjectSavedDir() / TEXT("Record") / Name; }

	bool IsAlive(const AActor* Actor)
	{
		static const FGameplayTag DeathTag = FGameplayTag::RequestGameplayTag(TEXT("Status.Death"));
		const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor);
		return IsValid(Actor) && !(ASC && ASC->HasMatchingGameplayTag(DeathTag));
	}

	AActor* EnemyOf(const APawn* Pawn)
	{
		const AAIController* AI = Pawn ? Cast<AAIController>(Pawn->GetController()) : nullptr;
		const UBlackboardComponent* Blackboard = AI ? AI->GetBlackboardComponent() : nullptr;
		AActor* Enemy = Blackboard ? Cast<AActor>(Blackboard->GetValueAsObject(TEXT("TargetEnemy"))) : nullptr;
		return IsAlive(Enemy) ? Enemy : nullptr;
	}

	FAutoConsoleCommandWithWorldAndArgs IntroCommand(TEXT("DH.Intro"),
		TEXT("Film the running bot match: DH.Intro [seconds] [clip name] [nocrane]. Frames go to Saved/IntroFrames or Saved/Record/<clip name>."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!World)
			{
				return;
			}
			ADHIntroDirector* Director = World->SpawnActor<ADHIntroDirector>();
			if (Args.Num() > 0)
			{
				Director->Duration = FCString::Atof(*Args[0]);
			}
			Director->OutDir = FramesDir(Args.Num() > 1 ? Args[1] : FString());
			if (Args.Num() > 2 && Args[2].Equals(TEXT("nocrane"), ESearchCase::IgnoreCase))
			{
				Director->CraneSeconds = 0.f;
			}
			IFileManager::Get().DeleteDirectory(*Director->OutDir, false, true);
			IFileManager::Get().MakeDirectory(*Director->OutDir, true);
		}));
}

ADHIntroDirector::ADHIntroDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;  // after the bots moved this frame
}

APawn* ADHIntroDirector::PickSubject() const
{
	// A bot in a fight, preferably at a filmable distance from its enemy and not the one just shown.
	APawn* Best = nullptr;
	float BestScore = -1.f;
	for (TActorIterator<AAIController> It(GetWorld()); It; ++It)
	{
		APawn* Pawn = It->GetPawn();
		if (!DHIntroSettings::IsAlive(Pawn))
		{
			continue;
		}
		float Score = FMath::FRand();
		if (const AActor* Target = DHIntroSettings::EnemyOf(Pawn))
		{
			const float Distance = FVector::Dist(Pawn->GetActorLocation(), Target->GetActorLocation());
			Score += Distance < 3500.f ? 3.f - FMath::Abs(Distance - 1500.f) / 1500.f : 0.5f;
			// Both in the picture only when they see each other.
			FHitResult Hit;
			FCollisionQueryParams Params(TEXT("DHIntroSight"), false, Pawn);
			Params.AddIgnoredActor(Target);
			if (!GetWorld()->LineTraceSingleByChannel(Hit, Pawn->GetActorLocation() + FVector(0, 0, 60), Target->GetActorLocation() + FVector(0, 0, 60), ECC_Visibility, Params))
			{
				Score += 2.f;
			}
		}
		if (Pawn == Subject.Get())
		{
			Score -= 2.f;
		}
		if (Score > BestScore)
		{
			BestScore = Score;
			Best = Pawn;
		}
	}
	return Best;
}

void ADHIntroDirector::Cut()
{
	Subject = PickSubject();
	Enemy = DHIntroSettings::EnemyOf(Subject.Get());
	ShotTime = 0.f;
	++Shot;
	FVector LookAt;
	bool bBlocked;
	SideSign = 1.f;
	FVector Location = ShotLocation(LookAt, bBlocked);
	if (bBlocked)
	{
		SideSign = -1.f;
		Location = ShotLocation(LookAt, bBlocked);
	}
	Camera->SetActorLocationAndRotation(Location, (LookAt - Location).Rotation());
}

FVector ADHIntroDirector::ShotLocation(FVector& OutLookAt, bool& bOutBlocked) const
{
	const APawn* Pawn = Subject.Get();
	if (!Pawn)
	{
		OutLookAt = Camera->GetActorLocation() + Camera->GetActorForwardVector();
		bOutBlocked = false;
		return Camera->GetActorLocation();
	}
	const FVector Body = Pawn->GetActorLocation() + FVector(0, 0, 50);
	const FVector Target = Enemy.IsValid() ? Enemy->GetActorLocation() + FVector(0, 0, 50) : Body + Pawn->GetActorForwardVector() * 1000.f;
	const FVector Dir = (Target - Body).GetSafeNormal2D();
	const float Drift = ShotTime * 25.f;

	// Two close framings, in turn: over the shoulder towards the enemy, low from the side. The side is picked per shot
	// (Cut) so the camera is not inside a wall.
	const FVector Side = FVector::CrossProduct(FVector::UpVector, Dir) * SideSign;
	FVector Desired;
	if (Shot % 2 == 0)
	{
		Desired = Body - Dir * (260.f - Drift) + Side * 85.f + FVector(0, 0, 45);
		OutLookAt = FMath::Lerp(Body, Target, 0.5f);
	}
	else
	{
		Desired = Body + Side * (300.f + Drift) - Dir * 80.f + FVector(0, 0, -15);
		OutLookAt = Body + Dir * 150.f;
	}

	// Pull in towards the subject when a wall is in the way (like a spring arm).
	FHitResult Hit;
	FCollisionQueryParams Params(TEXT("DHIntro"), false, Pawn);
	bOutBlocked = GetWorld()->LineTraceSingleByChannel(Hit, Body, Desired, ECC_Camera, Params);
	if (bOutBlocked)
	{
		Desired = Hit.Location + Hit.ImpactNormal * 30.f;
	}
	return Desired;
}

void ADHIntroDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	APlayerController* Player = GetWorld()->GetFirstPlayerController();
	if (!Camera)
	{
		// Out of the game: no own pawn (bots would hunt it), the view belongs to the camera.
		if (Player && Player->GetPawn())
		{
			APawn* Own = Player->GetPawn();
			Player->UnPossess();
			Own->Destroy();
		}
		Camera = GetWorld()->SpawnActor<ACameraActor>();
		Camera->GetCameraComponent()->SetFieldOfView(DHIntroSettings::FieldOfView);
		Camera->GetCameraComponent()->bConstrainAspectRatio = false;
		Cut();
	}
	if (Player && Player->GetViewTarget() != Camera)
	{
		Player->SetViewTarget(Camera);
	}

	Time += DeltaSeconds;
	ShotTime += DeltaSeconds;
	const float CraneStart = Duration - CraneSeconds;
	if (Time < CraneStart)
	{
		if (ShotTime > DHIntroSettings::ShotLength || !DHIntroSettings::IsAlive(Subject.Get()))
		{
			Cut();
		}
		FVector LookAt;
		bool bBlocked;
		const FVector Desired = ShotLocation(LookAt, bBlocked);
		const FVector Location = FMath::VInterpTo(Camera->GetActorLocation(), Desired, DeltaSeconds, 6.f);
		const FRotator Rotation = FMath::RInterpTo(Camera->GetActorRotation(), (LookAt - Location).Rotation(), DeltaSeconds, 8.f);
		Camera->SetActorLocationAndRotation(Location, Rotation);
	}
	else
	{
		if (!bCrane)
		{
			bCrane = true;
			// Look down the street the last fight was in, towards the enemy.
			const FVector Along = Subject.IsValid() && Enemy.IsValid()
				? (Enemy->GetActorLocation() - Subject->GetActorLocation()).GetSafeNormal2D() : Camera->GetActorForwardVector().GetSafeNormal2D();
			// Middle of the street the fight is in: halfway between the facades left and right.
			FVector Fight = Subject.IsValid() ? Subject->GetActorLocation() : Camera->GetActorLocation();
			const FVector Across = FVector::CrossProduct(FVector::UpVector, Along);
			FCollisionQueryParams Params(TEXT("DHIntroCrane"), false, Subject.Get());
			float Reach[2] = { 2500.f, 2500.f };
			for (int32 Index = 0; Index < 2; ++Index)
			{
				FHitResult Hit;
				const FVector Start = Fight + FVector(0, 0, 400);
				if (GetWorld()->LineTraceSingleByChannel(Hit, Start, Start + Across * (Index ? -2500.f : 2500.f), ECC_Visibility, Params))
				{
					Reach[Index] = Hit.Distance;
				}
			}
			Fight += Across * (Reach[0] - Reach[1]) / 2;
			CraneFrom = Camera->GetActorLocation();
			CraneMid = Fight - Along * 500.f + FVector(0, 0, CraneFrom.Z - Fight.Z + 250.f);
			CraneTo = CraneMid + FVector(0, 0, 3200.f);
			CraneLookAt = Fight + Along * 4500.f - FVector(0, 0, 300);
		}
		// Up over the middle of the street, looking down it.
		const float Alpha = FMath::InterpEaseInOut(0.f, 1.f, FMath::Clamp((Time - CraneStart) / CraneSeconds, 0.f, 1.f), 2.f);
		// First out to the middle of the street, then straight up.
		const FVector Location = Alpha < 0.25f ? FMath::Lerp(CraneFrom, CraneMid, Alpha / 0.25f) : FMath::Lerp(CraneMid, CraneTo, (Alpha - 0.25f) / 0.75f);
		Camera->SetActorLocationAndRotation(Location, (CraneLookAt - Location).Rotation());
	}

	CaptureFrame();
	if (Time >= Duration)
	{
		UE_LOG(LogTemp, Log, TEXT("DH.Intro: %d frames in %s"), Frame, *OutDir);
		SetActorTickEnabled(false);
	}
}

void ADHIntroDirector::CaptureFrame()
{
	FScreenshotRequest::RequestScreenshot(OutDir / FString::Printf(TEXT("%04d.png"), Frame++), false, false);
}
