#include "DHGameRulesComponent.h"

#include "DHPlayerChoicesComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DHGameRulesComponent)

DEFINE_LOG_CATEGORY(LogDHRules);

UDHGameRulesComponent::UDHGameRulesComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);

	WeaponItems.Add(TEXT("Pistol"), TSoftClassPtr<UObject>(FSoftObjectPath(TEXT("/ShooterCore/Weapons/Pistol/ID_Pistol.ID_Pistol_C"))));
	WeaponItems.Add(TEXT("Rifle"), TSoftClassPtr<UObject>(FSoftObjectPath(TEXT("/ShooterCore/Weapons/Rifle/ID_Rifle.ID_Rifle_C"))));
	WeaponItems.Add(TEXT("Shotgun"), TSoftClassPtr<UObject>(FSoftObjectPath(TEXT("/ShooterCore/Weapons/Shotgun/ID_Shotgun.ID_Shotgun_C"))));
	for (const TCHAR* Name : { TEXT("AK47"), TEXT("MP5"), TEXT("M1911") })  // Tools/Python/make_weapons.py
	{
		WeaponItems.Add(Name, TSoftClassPtr<UObject>(FSoftObjectPath(FString::Printf(TEXT("/DowntownCore/Weapons/%s/ID_%s.ID_%s_C"), Name, Name, Name))));
	}
}

const UDHGameRulesComponent* UDHGameRulesComponent::Find(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	return GameState ? GameState->FindComponentByClass<UDHGameRulesComponent>() : nullptr;
}

void UDHGameRulesComponent::BeginPlay()
{
	Super::BeginPlay();

	const AGameModeBase* GameMode = GetWorld()->GetAuthGameMode();
	if (!GameMode)
	{
		return;
	}
	const FString& Options = GameMode->OptionsString;

	const FString PerspectiveOption = UGameplayStatics::ParseOption(Options, TEXT("Perspective"));
	if (PerspectiveOption.Equals(TEXT("FP"), ESearchCase::IgnoreCase))
	{
		Perspective = EDHPerspectiveRule::FirstPerson;
	}
	else if (PerspectiveOption.Equals(TEXT("Choice"), ESearchCase::IgnoreCase))
	{
		Perspective = EDHPerspectiveRule::Choice;
	}

	bWeaponSwitch = UGameplayStatics::GetIntOption(Options, TEXT("WeaponSwitch"), 1) != 0;

	TArray<FString> Names;
	UGameplayStatics::ParseOption(Options, TEXT("Loadout")).ParseIntoArray(Names, TEXT(","));
	bPlayerLoadout = Names.Num() == 1 && Names[0].TrimStartAndEnd().Equals(TEXT("Player"), ESearchCase::IgnoreCase);
	for (const FString& Name : Names)
	{
		Loadout.Add(FName(*Name.TrimStartAndEnd()));
	}
	if (!bWeaponSwitch && Loadout.Num() > 1)
	{
		Loadout.SetNum(1);
	}

	UE_LOG(LogDHRules, Log, TEXT("Rules from '%s': perspective %d, weapon switch %d, loadout %d items"),
		*Options, static_cast<int32>(Perspective), bWeaponSwitch ? 1 : 0, Loadout.Num());
}

TArray<UClass*> UDHGameRulesComponent::GetLoadoutItems(const AController* Controller) const
{
	TArray<FName> Names = Loadout;
	if (bPlayerLoadout)
	{
		const UDHPlayerChoicesComponent* Choices = Controller ? Controller->FindComponentByClass<UDHPlayerChoicesComponent>() : nullptr;
		Names = Choices ? Choices->GetLoadout() : TArray<FName>();
		if (Names.IsEmpty())
		{
			static const FName Primaries[] = { TEXT("Rifle"), TEXT("Shotgun"), TEXT("AK47"), TEXT("MP5") };
			static const FName Secondaries[] = { TEXT("Pistol"), TEXT("M1911") };
			Names = { Primaries[FMath::RandHelper(UE_ARRAY_COUNT(Primaries))], Secondaries[FMath::RandHelper(UE_ARRAY_COUNT(Secondaries))] };
		}
		if (!bWeaponSwitch)
		{
			Names.SetNum(1);
		}
	}

	TArray<UClass*> Items;
	for (const FName& Name : Names)
	{
		if (const TSoftClassPtr<UObject>* Item = WeaponItems.Find(Name))
		{
			if (UClass* Class = Item->LoadSynchronous())
			{
				Items.Add(Class);
			}
		}
	}
	return Items;
}

void UDHGameRulesComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UDHGameRulesComponent, Perspective);
	DOREPLIFETIME(UDHGameRulesComponent, bWeaponSwitch);
}
