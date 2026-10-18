#pragma once

#include "GameFramework/Actor.h"

#include "DHIntroDirector.generated.h"

class ACameraActor;

/**
 * Films the intro movie from a running bot match (console: DH.Intro [seconds]). The local player's pawn is removed and
 * a camera cuts between bots that are fighting (over the shoulder, from the side, from the front), then cranes up over
 * the street. Every frame is saved to Saved/IntroFrames/<frame>.png without UI; start the game with
 * -UseFixedTimeStep -FPS=30 so the frames play back at real speed. Tools/Branding/make_intro_movie.py cuts the movie.
 */
UCLASS(NotPlaceable, Transient)
class ADHIntroDirector : public AActor
{
	GENERATED_BODY()

public:
	ADHIntroDirector();

	virtual void Tick(float DeltaSeconds) override;

	float Duration = 20.f;

private:
	APawn* PickSubject() const;
	void Cut();
	FVector ShotLocation(FVector& OutLookAt, bool& bOutBlocked) const;
	void CaptureFrame();

	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> Camera;

	TWeakObjectPtr<APawn> Subject;
	TWeakObjectPtr<AActor> Enemy;
	int32 Shot = 0;
	float SideSign = 1.f;
	int32 Frame = 0;
	float Time = 0.f;
	float ShotTime = 0.f;
	bool bCrane = false;
	FVector CraneFrom, CraneMid, CraneTo, CraneLookAt;
};
