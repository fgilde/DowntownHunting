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
	PrimaryActorTick.bCanEverTick = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	// Pole and cloth turn slowly (locally, Tick) so the flag is seen from every side.
	Spin = CreateDefaultSubobject<USceneComponent>(TEXT("Spin"));
	Spin->SetupAttachment(RootComponent);
	auto MakePart = [this](const TCHAR* Name, const TCHAR* Mesh, const FVector& Location, const FVector& Scale)
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(Spin);
		Part->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, Mesh));
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetRelativeLocation(Location);
		Part->SetRelativeScale3D(Scale);
		return Part;
	};
	// Engine shapes are 100 units: a 3 m pole, a 1.2 x 0.75 m cloth at its top.
	Pole = MakePart(TEXT("Pole"), TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), FVector(0, 0, 150), FVector(0.08f, 0.08f, 3.f));
	Cloth = MakePart(TEXT("Cloth"), TEXT("/Engine/BasicShapes/Cube.Cube"), FVector(62, 0, 260), FVector(1.2f, 0.04f, 0.75f));

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(RootComponent);
	Light->SetRelativeLocation(FVector(0, 0, 260));
	Light->SetAttenuationRadius(700.f);
	Light->SetIntensity(20000.f);
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

void ADHFlag::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Spin->AddRelativeRotation(FRotator(0.f, 60.f * DeltaSeconds, 0.f));
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
