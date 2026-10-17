#include "DHTeamBandComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DHTeamBandComponent)

UDHTeamBandComponent::UDHTeamBandComponent()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TeamColor(TEXT("/Game/Characters/Cosmetics/M_TeamColorBasic.M_TeamColorBasic"));
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
		Band->SetOwnerNoSee(true); // the own team is known; seen from behind in third person the band looked like a can on the back
		Band->SetupAttachment(Mesh, Socket);
		Band->SetRelativeTransform(BandTransform);
		Band->RegisterComponent();
		Bands.Add(Band);
	}
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
