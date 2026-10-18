#pragma once

#include "Equipment/LyraEquipmentManagerComponent.h"
#include "GameFramework/Pawn.h"
#include "GameplayTagContainer.h"
#include "Inventory/LyraInventoryItemInstance.h"

/**
 * Access to Lyra's weapon objects that LyraGame does not export (equipment manager class, ranged weapon instance,
 * item stat functions): found by name and called through reflection.
 */
namespace DHWeaponAccess
{
	/** The pawn's equipped Lyra ranged weapon instance. */
	inline UObject* ActiveWeapon(const APawn* Pawn)
	{
		static UClass* RangedClass = FindObject<UClass>(nullptr, TEXT("/Script/LyraGame.LyraRangedWeaponInstance"));
		for (UActorComponent* Component : Pawn && RangedClass ? Pawn->GetComponents() : TSet<UActorComponent*>())
		{
			if (Component->GetClass()->GetFName() == TEXT("LyraEquipmentManagerComponent"))
			{
				return reinterpret_cast<UObject*>(static_cast<ULyraEquipmentManagerComponent*>(Component)->GetFirstInstanceOfType(RangedClass));
			}
		}
		return nullptr;
	}

	/** The inventory item a weapon instance was equipped from (ULyraEquipmentInstance::Instigator). */
	inline ULyraInventoryItemInstance* ItemOf(const UObject* Weapon)
	{
		const FObjectProperty* Property = Weapon ? FindFProperty<FObjectProperty>(Weapon->GetClass(), TEXT("Instigator")) : nullptr;
		return Property ? Cast<ULyraInventoryItemInstance>(Property->GetObjectPropertyValue_InContainer(Weapon)) : nullptr;
	}

	inline int32 ItemStat(ULyraInventoryItemInstance* Item, FGameplayTag Tag)
	{
		struct
		{
			FGameplayTag Tag;
			int32 ReturnValue = 0;
		} Params{ Tag };
		if (UFunction* Function = Item ? Item->FindFunction(TEXT("GetStatTagStackCount")) : nullptr)
		{
			Item->ProcessEvent(Function, &Params);
		}
		return Params.ReturnValue;
	}

	/** Server only (the stats replicate). */
	inline void AddItemStat(ULyraInventoryItemInstance* Item, FGameplayTag Tag, int32 Count)
	{
		struct
		{
			FGameplayTag Tag;
			int32 StackCount;
		} Params{ Tag, Count };
		if (UFunction* Function = Item ? Item->FindFunction(TEXT("AddStatTagStack")) : nullptr)
		{
			Item->ProcessEvent(Function, &Params);
		}
	}
}
