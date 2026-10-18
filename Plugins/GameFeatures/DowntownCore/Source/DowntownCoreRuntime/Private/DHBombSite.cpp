#include "DHBombSite.h"

#include "Character/LyraHealthComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DHBombSite)

namespace
{
	const FLinearColor Idle(1.0f, 0.42f, 0.05f);
	const FLinearColor Armed(1.0f, 0.04f, 0.02f);
}

ADHBombSite::ADHBombSite()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	PrimaryActorTick.bCanEverTick = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// Dashed circle: 36 flat bars along the edge of the site.
	Ring = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Ring"));
	Ring->SetupAttachment(RootComponent);
	Ring->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	Ring->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Ring->SetCastShadow(false);
	constexpr int32 Bars = 36;
	for (int32 Index = 0; Index < Bars; ++Index)
	{
		const float Angle = 360.f * Index / Bars;
		const FVector Position = FRotator(0.f, Angle, 0.f).Vector() * Radius + FVector(0, 0, 4);
		// the cube is 100 units: bars 50 long, 14 wide, 4 high, along the circle
		Ring->AddInstance(FTransform(FRotator(0.f, Angle + 90.f, 0.f), Position, FVector(0.5f, 0.14f, 0.04f)));
	}

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(RootComponent);
	Light->SetRelativeLocation(FVector(0, 0, 120));
	Light->SetAttenuationRadius(900.f);
	Light->SetIntensity(0.f);
	Light->SetLightColor(Armed);
	Light->SetCastShadows(false);

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(RootComponent);
	Label->SetRelativeLocation(FVector(0, 0, 420));
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetVerticalAlignment(EVRTA_TextCenter);
	Label->SetWorldSize(260.f);
	Label->SetTextRenderColor(Idle.ToFColor(true));
}

void ADHBombSite::BeginPlay()
{
	Super::BeginPlay();
	if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		RingMaterial = UMaterialInstanceDynamic::Create(Base, this);
		Ring->SetMaterial(0, RingMaterial);
	}
	ExplosionEffect = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/Effects/Particles/Explosion/NS_Grenade_Explosion.NS_Grenade_Explosion"));
	ExplosionSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/MetaSounds/sfx_Weapon_GrenadeExplosion_nl_meta.sfx_Weapon_GrenadeExplosion_nl_meta"));
	OnRep_State();
}

void ADHBombSite::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADHBombSite, Letter);
	DOREPLIFETIME(ADHBombSite, bArmed);
}

void ADHBombSite::SetState(const FString& InLetter, bool bInArmed)
{
	Letter = InLetter;
	bArmed = bInArmed;
	OnRep_State();  // the server (listen host) sees it too
}

void ADHBombSite::OnRep_State()
{
	Label->SetText(FText::FromString(Letter));
	Label->SetTextRenderColor((bArmed ? Armed : Idle).ToFColor(true));
	if (RingMaterial)
	{
		RingMaterial->SetVectorParameterValue(TEXT("Color"), bArmed ? Armed : Idle);
	}
}

bool ADHBombSite::Contains(const FVector& Location) const
{
	const FVector Offset = Location - GetActorLocation();
	return Offset.Size2D() <= Radius && FMath::Abs(Offset.Z) < 400.f;
}

void ADHBombSite::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// The letter turns to the local camera.
	if (const APlayerController* Player = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (Player->PlayerCameraManager)
		{
			const FVector ToCamera = Player->PlayerCameraManager->GetCameraLocation() - Label->GetComponentLocation();
			Label->SetWorldRotation(FRotator(0.f, ToCamera.Rotation().Yaw, 0.f));
		}
	}
	const float Pulse = bArmed ? 0.5f + 0.5f * FMath::Sin(GetWorld()->GetTimeSeconds() * 8.f) : 0.f;
	Label->SetRelativeScale3D(FVector(1.f + 0.1f * Pulse));
	Light->SetIntensity(bArmed ? 20000.f * Pulse : 0.f);
}

void ADHBombSite::Explode()
{
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		if (FVector::Dist(It->GetActorLocation(), GetActorLocation()) < BlastRadius)
		{
			if (ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(*It))
			{
				Health->DamageSelfDestruct();
			}
		}
	}
	MulticastExplosion();
}

void ADHBombSite::MulticastExplosion_Implementation()
{
	if (ExplosionEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, ExplosionEffect, GetActorLocation() + FVector(0, 0, 50), FRotator::ZeroRotator, FVector(4.f));
	}
	if (ExplosionSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ExplosionSound, GetActorLocation(), 2.f);
	}
}
