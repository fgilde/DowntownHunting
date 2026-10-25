#include "DHBoundaries.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/BlockingVolume.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"

namespace DHBoundaries
{
	void Show(UWorld* World)
	{
		UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		UMaterialInterface* Field = LoadObject<UMaterialInterface>(nullptr, TEXT("/DowntownCore/Effects/M_DH_Boundary.M_DH_Boundary"));
		if (!World || !Cube || !Field || World->GetNetMode() == NM_DedicatedServer)
		{
			return;
		}
		TArray<FBox> Walls;
		for (TActorIterator<ABlockingVolume> It(World); It; ++It)
		{
			Walls.Add(It->GetComponentsBoundingBox(true));
		}
		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		AActor* Holder = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
		if (!Holder)
		{
			return;
		}
		USceneComponent* Root = NewObject<USceneComponent>(Holder);
		Holder->SetRootComponent(Root);
		Root->RegisterComponent();
		for (const FBox& Wall : Walls)
		{
			UStaticMeshComponent* Box = NewObject<UStaticMeshComponent>(Holder);
			Box->SetStaticMesh(Cube);
			Box->SetMaterial(0, Field);
			Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Box->SetCastShadow(false);
			Box->SetupAttachment(Root);
			Box->RegisterComponent();
			// The basic cube is 100 cm with its pivot in the middle.
			Box->SetWorldLocationAndRotation(Wall.GetCenter(), FRotator::ZeroRotator);
			Box->SetWorldScale3D(Wall.GetSize() / 100.f);
			UE_LOG(LogTemp, Verbose, TEXT("DH boundary at %s, size %s"), *Wall.GetCenter().ToCompactString(), *Wall.GetSize().ToCompactString());
		}
		UE_LOG(LogTemp, Log, TEXT("DH boundaries: %d walls shown"), Walls.Num());
	}
}
