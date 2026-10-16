// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AsyncMessageHandle.h"
#include "AsyncMessageId.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "ShooterTestAsyncMessageTestActor.generated.h"

struct FAsyncMessage;
class UStaticMeshComponent;

USTRUCT(BlueprintType)
struct FColorChangingMessage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category="Test")
	FLinearColor DesiredColorToChange = FLinearColor::Red;
};

/**
 * A test actor which will spawn a task outside of the game thread to
 * broadcast different color messages
 */
UCLASS(BlueprintType)
class AAsyncColorChangeBroadcastActor : public AActor
{
	GENERATED_BODY()
public:

	AAsyncColorChangeBroadcastActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	
	virtual void BeginPlay() override;

	void SpawnBackgroundTask();
	
	void SpawnGameplayTagBroadcaster();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Test")
	TInstancedStruct<FColorChangingMessage> ColorChangeData;

	UPROPERTY(EditAnywhere, Category="Test")
	FAsyncMessageId ColorMessageToBroadcast;

	UPROPERTY(EditAnywhere, Category="Test")
	FGameplayTag GameplayTagToBroadcastA;

	UPROPERTY(EditAnywhere, Category="Test")
	FGameplayTag GameplayTagToBroadcastB;

	UPROPERTY(BlueprintReadOnly, Category="Test")
	int32 NumBroadcasts = 0;

	UPROPERTY(EditAnywhere, Category="Test")
	float ColorMessageBroadcastFrequency = 3.0f;

	UPROPERTY(EditAnywhere, Category="Test")
	float GameplayTagBroadcastFrequency = 3.0f;

	FTimerHandle StartBackgroundTaskTimerHandle;

	FTimerHandle GameplayTagSpawner;
};

/**
 * An actor which will change the color of its static mesh
 * in response to an async message that it listens to
 */
UCLASS(BlueprintType)
class AColorChangingTestListener : public AActor
{
	GENERATED_BODY()
public:
	
	AColorChangingTestListener(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	void HandleColorChange(const FAsyncMessage& Message);

	void HandleAnyColorChange(const FAsyncMessage& Message);

	const FAsyncMessageId GetColorChangeToListenTo();

	void SetupColorListener();
	void ResetListenerToColor();
	void SetupNamedThreadListener();
	
	UPROPERTY(EditAnywhere, Category="Test")
	TObjectPtr<UStaticMeshComponent> MeshComp;

	UPROPERTY(EditAnywhere, Category="Test")
	FName MaterialColorParamName = "ChangeColorParam";

	UPROPERTY(EditAnywhere, Category="Test")
	FAsyncMessageId OverrideMessageBinding;

	UPROPERTY(EditAnywhere, Category="Test")
	FGameplayTag TagToBindTo;
	
	UPROPERTY(EditAnywhere, Category="Test")
	bool bListenForParentTag = false;

	// Test receiving and binding messages on another thread
	UPROPERTY(EditAnywhere, Category="Test")
	bool bShouldSpawnTaskOnOtherThread = false;
	
	FTimerHandle BackgroundQueueMessageHandle;
	
	FAsyncMessageHandle BoundHandle_Color;

	FAsyncMessageHandle BoundHandle_ColorParent;
	
	int32 PreviousColorBoundTo = -1;
};


USTRUCT(BlueprintType)
struct FTestHeavyMessage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category="Test")
	FLinearColor DesiredColorToChange = FLinearColor::Red;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Test")
	FRotator AmountToRotate;
};


/**
 * This is an actor which will queue a message for broadcast every single tick.
 */
UCLASS(BlueprintType)
class AHeavyPerformanceBroadcastor : public AActor
{
	GENERATED_BODY()

	AHeavyPerformanceBroadcastor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	
public:
	virtual void Tick(float DeltaSeconds) override;

	void QueueMessage();
	
	UPROPERTY(EditAnywhere, Category="Test")
	TObjectPtr<UStaticMeshComponent> MeshComp;
	
	UPROPERTY(EditAnywhere, Category="Test")
	FAsyncMessageId MessageToQueue;

	UPROPERTY(EditAnywhere, Category="Test")
	FInstancedStruct MessagePayloadToQueue;

	UPROPERTY(EditAnywhere, Category="Test", meta=(UIMin=1))
	int32 NumberOfTimesToQueue = 1;

	UPROPERTY(EditAnywhere, Category="Test")
	TEnumAsByte<enum ETickingGroup> CustomTickGroup = TG_PrePhysics;
};


UCLASS(BlueprintType)
class AHeavyPerformanceListener : public AActor
{
	GENERATED_BODY()

	AHeavyPerformanceListener(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	
public:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	UPROPERTY(EditAnywhere, Category="Test")
	TObjectPtr<UStaticMeshComponent> MeshComp;

	UPROPERTY(EditAnywhere, Category="Test")
	FAsyncMessageId MessageToListenFor;

	UPROPERTY(EditAnywhere, Category="Test")
	TEnumAsByte<ETickingGroup> GroupToListenFor = ETickingGroup::TG_PostPhysics;

private:

	void HandleMessage(const FAsyncMessage& Message);
	void SetupListener();
	void RemoveListener();
	
	FAsyncMessageHandle ListenerHandle;
};