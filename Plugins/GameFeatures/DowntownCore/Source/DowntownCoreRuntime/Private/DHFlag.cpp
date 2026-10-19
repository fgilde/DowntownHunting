#include "DHFlag.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DHFlag)

ADHFlag::ADHFlag()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(true);  // position when dropped, attachment while carried

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	auto MakePart = [this](const TCHAR* Name, const TCHAR* Mesh, const FVector& Location, const FVector& Scale)
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(RootComponent);
		Part->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, Mesh));
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetRelativeLocation(Location);
		Part->SetRelativeScale3D(Scale);
		return Part;
	};
	// Engine shapes are 100 units: a 2.2 m pole, a 70 x 45 cm cloth at its top.
	Pole = MakePart(TEXT("Pole"), TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), FVector(0, 0, 110), FVector(0.05f, 0.05f, 2.2f));
	Cloth = MakePart(TEXT("Cloth"), TEXT("/Engine/BasicShapes/Cube.Cube"), FVector(37, 0, 195), FVector(0.7f, 0.03f, 0.45f));

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(RootComponent);
	Light->SetRelativeLocation(FVector(0, 0, 200));
	Light->SetAttenuationRadius(700.f);
	Light->SetIntensity(8000.f);
	Light->SetCastShadows(false);
}

void ADHFlag::BeginPlay()
{
	Super::BeginPlay();
	if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		ClothMaterial = UMaterialInstanceDynamic::Create(Base, this);
		Cloth->SetMaterial(0, ClothMaterial);
	}
	OnRep_Colour();
}

void ADHFlag::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADHFlag, Colour);
}

void ADHFlag::OnRep_Colour()
{
	if (ClothMaterial)
	{
		ClothMaterial->SetVectorParameterValue(TEXT("Color"), Colour);
	}
	Light->SetLightColor(Colour);
}

void ADHFlag::PickUp(APawn* Pawn)
{
	Carrier = Pawn;
	DroppedSeconds = -1.f;
	// On the carrier's back, sticking out above the head.
	AttachToActor(Pawn, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	SetActorRelativeLocation(FVector(-35.f, 0.f, -40.f));
}

void ADHFlag::Drop()
{
	const FVector Where = GetActorLocation();
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Carrier = nullptr;
	DroppedSeconds = 0.f;
	// On the ground below the carrier (the capsule centre is about a metre up).
	SetActorLocationAndRotation(Where - FVector(0.f, 0.f, 50.f), FRotator::ZeroRotator);
}

void ADHFlag::ReturnHome()
{
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Carrier = nullptr;
	DroppedSeconds = -1.f;
	SetActorLocationAndRotation(Home, FRotator::ZeroRotator);
}
