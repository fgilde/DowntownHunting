#include "DHCardButton.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DHCardButton)

UDHCardButton::UDHCardButton(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UDHCardButton::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	OnClicked.AddUniqueDynamic(this, &ThisClass::HandleClicked);
}

void UDHCardButton::HandleClicked()
{
	OnCardClicked.ExecuteIfBound(this);
}
