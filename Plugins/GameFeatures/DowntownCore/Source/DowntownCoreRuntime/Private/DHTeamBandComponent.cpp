#include "DHTeamBandComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DHTeamBandComponent)

UDHTeamBandComponent::UDHTeamBandComponent()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TeamColor(TEXT("/Game/Characters/Cosmetics/M_TeamColorBasic.M_TeamColorBasic"));
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickInterval = 0.2f;
	BandMesh = Cylinder.Object;
	BandMaterial = TeamColor.Object;
}

void UDHTeamBandComponent::OnRegister()
{
	Super::OnRegister();

	AActor* Owner = GetOwner();
	USkeletalMeshComponent* Mesh = Owner ? Owner->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
	if (!Mesh || !BandMesh || !Bands.IsEmpty())
	{
		return;
	}

	for (const FName& Socket : Sockets)
	{
		UStaticMeshComponent* Band = NewObject<UStaticMeshComponent>(Owner, NAME_None, RF_Transient);
		Band->SetStaticMesh(BandMesh);
		Band->SetMaterial(0, BandMaterial);
		Band->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Band->SetCastShadow(false);
		Band->SetupAttachment(Mesh, Socket);
		// Right-side arm bones point their X axis back towards the body: mirror the offset, otherwise the band sits in the shoulder.
		FTransform Transform = BandTransform;
		if (Socket.ToString().EndsWith(TEXT("_r")))
		{
			Transform.SetLocation(-Transform.GetLocation());
		}
		Band->SetRelativeTransform(Transform);
		Band->RegisterComponent();
		Bands.Add(Band);
	}
}

void UDHTeamBandComponent::BeginPlay()
{
	Super::BeginPlay();
	SetComponentTickEnabled(true);
}

void UDHTeamBandComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// The own player knows their team; seen from behind the band looked like a can on the shoulder.
	// Lyra's cosmetic part actor has no owner and is attached to the pawn some frames after spawning,
	// so wait until the pawn is known and possessed, then decide once.
	const APawn* Pawn = GetOwner() ? Cast<APawn>(GetOwner()->GetAttachParentActor()) : nullptr;
	if (!Pawn || !Pawn->GetController())
	{
		return;
	}
	for (UStaticMeshComponent* Band : Bands)
	{
		if (Band)
		{
			Band->SetHiddenInGame(Pawn->IsPlayerControlled() && Pawn->IsLocallyControlled());  // bots count as local on the server
		}
	}
	SetComponentTickEnabled(false);
}

void UDHTeamBandComponent::OnUnregister()
{
	for (UStaticMeshComponent* Band : Bands)
	{
		if (Band)
		{
			Band->DestroyComponent();
		}
	}
	Bands.Reset();

	Super::OnUnregister();
}
