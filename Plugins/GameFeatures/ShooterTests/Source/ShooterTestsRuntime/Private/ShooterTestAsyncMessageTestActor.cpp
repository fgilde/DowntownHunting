// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShooterTestAsyncMessageTestActor.h"

#include "AudioThread.h"
#include "Async/Async.h"
#include "AsyncMessageBindingOptions.h"
#include "AsyncGameplayMessageSystem.h"
#include "AsyncMessageWorldSubsystem.h"
#include "AsyncMessageSystemLogs.h"
#include "Components/StaticMeshComponent.h"
#include "NativeGameplayTags.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ShooterTestAsyncMessageTestActor)

UE_DEFINE_GAMEPLAY_TAG(Message_Tag_Red, "Change.Color.Red")
UE_DEFINE_GAMEPLAY_TAG(Message_Tag_Green, "Change.Color.Green")
UE_DEFINE_GAMEPLAY_TAG(Message_Tag_Blue, "Change.Color.Blue")
UE_DEFINE_GAMEPLAY_TAG(Message_Tag_Async_Test, "Async.Message.Test")
UE_DEFINE_GAMEPLAY_TAG(Message_Tag_Test_Heavy, "Tests.HeavyMessage")
UE_DEFINE_GAMEPLAY_TAG(Message_Tag_Test_HeavySubmessage, "Tests.HeavyMessage.SubMessage")

static FAsyncMessageId Message_ChangeColor_Red = { Message_Tag_Red };
static FAsyncMessageId Message_ChangeColor_Green = { Message_Tag_Green};
static FAsyncMessageId Message_ChangeColor_Blue = { Message_Tag_Blue };

static FAsyncMessageId Message_Async_Test = { Message_Tag_Async_Test };

///////////////////////////////////////////////////////////////
// AAsyncColorChangeBroadcastActor

AAsyncColorChangeBroadcastActor::AAsyncColorChangeBroadcastActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ColorMessageToBroadcast = Message_ChangeColor_Red;
}

void AAsyncColorChangeBroadcastActor::BeginPlay()
{
	Super::BeginPlay();
	
	SpawnBackgroundTask();

	SpawnGameplayTagBroadcaster();
}

void AAsyncColorChangeBroadcastActor::SpawnBackgroundTask()
{	
	TWeakObjectPtr<AAsyncColorChangeBroadcastActor> WeakThis(this);

	// Queue a message from a non-game thread, a normal thread from the task pool in this case
	AsyncTask(ENamedThreads::AnyNormalThreadNormalTask, [WeakThis]()
	{
		if (!WeakThis.IsValid())
		{ 
			return;
		}

		if (TSharedPtr<FAsyncGameplayMessageSystem> Sys = UAsyncMessageWorldSubsystem::GetSharedMessageSystem<FAsyncGameplayMessageSystem>(WeakThis->GetWorld()))
		{
			// Queue a color change message
			Sys.Get()->QueueMessageForBroadcast(WeakThis->ColorMessageToBroadcast, TStructView<FColorChangingMessage>(WeakThis->ColorChangeData));
		}

		WeakThis->NumBroadcasts++;
	});

	// Set up a timer to spawn a new background task every second
	GetWorld()->GetTimerManager().SetTimer(StartBackgroundTaskTimerHandle,
		FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				SpawnBackgroundTask();
			}), 1.0f, false, ColorMessageBroadcastFrequency);
}

void AAsyncColorChangeBroadcastActor::SpawnGameplayTagBroadcaster()
{
	if (!GameplayTagToBroadcastA.IsValid() && !GameplayTagToBroadcastB.IsValid())
	{
		return;
	}
	
	TWeakObjectPtr<UWorld> WeakWorld = GetWorld();
	TWeakObjectPtr<AAsyncColorChangeBroadcastActor> WeakThis = this;
	
	// Set up a timer to spawn a new background task. This will queue the message on the game thread
	GetWorld()->GetTimerManager().SetTimer(GameplayTagSpawner,
		FTimerDelegate::CreateWeakLambda(this, [WeakWorld, WeakThis]()
		{
			if (!WeakThis.IsValid())
			{
				return;
			}
			
			// Queue the gameplay tag message a for broadcast
			if (TSharedPtr<FAsyncGameplayMessageSystem> Sys = UAsyncMessageWorldSubsystem::GetSharedMessageSystem<FAsyncGameplayMessageSystem>(WeakWorld.Get()))
			{
				if (WeakThis->GameplayTagToBroadcastA.IsValid())
				{
					Sys->QueueMessageForBroadcast(WeakThis->GameplayTagToBroadcastA);	
				}

				if (WeakThis->GameplayTagToBroadcastB.IsValid())
				{
					Sys->QueueMessageForBroadcast(WeakThis->GameplayTagToBroadcastB);
				}
			}
		}), 1.0f, true, GameplayTagBroadcastFrequency);
}

///////////////////////////////////////////////////////////////
// AColorChangingTestListener

AColorChangingTestListener::AColorChangingTestListener(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ColorChangingMesh"));
	SetRootComponent(MeshComp);
	
	// We will be overriding the materials on this mesh to change their color, so make some dynamic instances of the material
	for (int32 MatIdx = 0; MatIdx < MeshComp->GetNumMaterials(); ++MatIdx)
	{
		MeshComp->CreateDynamicMaterialInstance(MatIdx);
	}
}

void AColorChangingTestListener::BeginPlay()
{
	Super::BeginPlay();
	
	if (TSharedPtr<FAsyncGameplayMessageSystem> Sys = UAsyncMessageWorldSubsystem::GetSharedMessageSystem<FAsyncGameplayMessageSystem>(GetWorld()))
	{
		// Listen to ALL color messages
		BoundHandle_ColorParent = Sys->BindListener(
			Message_ChangeColor_Red.GetParentMessageId(),
			TWeakObjectPtr<AColorChangingTestListener>(this),
			&ThisClass::HandleAnyColorChange);
	}

	if (bShouldSpawnTaskOnOtherThread)
	{
		SetupNamedThreadListener();
	}
	
	PreviousColorBoundTo = FMath::RandRange(0, 2);
	
	SetupColorListener();
}

void AColorChangingTestListener::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	ResetListenerToColor();

	if (TSharedPtr<FAsyncGameplayMessageSystem> Sys = UAsyncMessageWorldSubsystem::GetSharedMessageSystem<FAsyncGameplayMessageSystem>(GetWorld()))
	{
		Sys->UnbindListener(BoundHandle_ColorParent);
	}
}

void AColorChangingTestListener::SetupColorListener()
{
	if (TSharedPtr<FAsyncGameplayMessageSystem> Sys = UAsyncMessageWorldSubsystem::GetSharedMessageSystem<FAsyncGameplayMessageSystem>(GetWorld()))
	{
		const FAsyncMessageId MessageToListenFor = GetColorChangeToListenTo();

		FAsyncMessageBindingOptions Opts = {};

		// Listen to the color messages
		BoundHandle_Color = Sys->BindListener(
			MessageToListenFor,
			TWeakObjectPtr<AColorChangingTestListener>(this),
			&ThisClass::HandleColorChange,
			Opts);
	}
}

void AColorChangingTestListener::ResetListenerToColor()
{
	if (TSharedPtr<FAsyncGameplayMessageSystem> Sys = UAsyncMessageWorldSubsystem::GetSharedMessageSystem<FAsyncGameplayMessageSystem>(GetWorld()))
	{
		Sys->UnbindListener(BoundHandle_Color);
		BoundHandle_Color = FAsyncMessageHandle::Invalid;
	}
}

void AColorChangingTestListener::SetupNamedThreadListener()
{
	check(bShouldSpawnTaskOnOtherThread);

	// We want to receive our event on the RHI thread
	FAsyncMessageBindingOptions Opts = {};
	Opts.SetNamedThreads(ENamedThreads::RHIThread);

	if (TSharedPtr<FAsyncGameplayMessageSystem> Sys = UAsyncMessageWorldSubsystem::GetSharedMessageSystem<FAsyncGameplayMessageSystem>(GetWorld()))
	{
		Sys->BindListener(Message_Async_Test, [](const FAsyncMessage& Message)
		{
			// Ensure that we are actually geting our message on the thread we have requested to
			const uint32 CurrentThread = FPlatformTLS::GetCurrentThreadId();
			ensure(CurrentThread == Message.GetThreadQueuedFromThreadId());
			
			UE_LOG(LogAsyncMessageSystem, Verbose, TEXT("Successfully received message on RHI thread"));
		}, Opts);
	}

	// Wait 5 seconds, and then spawn an async task on the background thread to queue the message
	const float NextDelay = 5.0f;

	TWeakObjectPtr<UWorld> WeakWorld = GetWorld();

	// Set up a timer to spawn a new background task every second
	GetWorld()->GetTimerManager().SetTimer(BackgroundQueueMessageHandle,
		FTimerDelegate::CreateWeakLambda(this, [WeakWorld]()
			{
				// Spawn an async task to queue an message, on a background thread
				AsyncTask(ENamedThreads::RHIThread, [WeakWorld]()
				{
					if (TSharedPtr<FAsyncGameplayMessageSystem> Sys = UAsyncMessageWorldSubsystem::GetSharedMessageSystem<FAsyncGameplayMessageSystem>(WeakWorld.Get()))
					{
						Sys->QueueMessageForBroadcast(Message_Async_Test);
					}
				});
			
				// loop every 1 second
			}), 1.0f, true, NextDelay);
}

void AColorChangingTestListener::HandleColorChange(const FAsyncMessage& Message)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(AColorChangingTestListener::HandleColorChange);

	ensure(IsInGameThread() || IsInParallelGameThread());
	
	// Some specific color change here
	const FColorChangingMessage* Data = Message.GetPayloadData<const FColorChangingMessage>();
	if (!Data)
	{
		ensure(false);
		return;
	}

	// Change the dynamic material param colors
	FLinearColor Col = Data->DesiredColorToChange;
	MeshComp->SetColorParameterValueOnMaterials(MaterialColorParamName, Col);	
	
	// If we are listening for a specific message already, don't attempt to rebind
	if (OverrideMessageBinding.IsValid())
	{
		return;
	}
	
	// Stop listening for the color that we just got
	ResetListenerToColor();

	// And start listening to a new fancy color message
	SetupColorListener();
}

void AColorChangingTestListener::HandleAnyColorChange(const FAsyncMessage& Message)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(AColorChangingTestListener::HandleAnyColorChange);
	
	ensure(IsInGameThread() || IsInParallelGameThread());

	const FColorChangingMessage* Data = Message.GetPayloadData<const FColorChangingMessage>();
	if (!Data)
	{
		ensure(false);
		return;
	}

	const uint32 MyThread = FPlatformTLS::GetCurrentThreadId();

	// Any time there is a color change, rotate the actor a bit	
	AddActorLocalRotation(FRotator(0.0, 0.0, 10.0));
}

const FAsyncMessageId AColorChangingTestListener::GetColorChangeToListenTo()
{
	// If you have specified a specific color to listen for let you do so here
	if (OverrideMessageBinding.IsValid())
	{
		return OverrideMessageBinding;
	}
	
	// Increment to the next color
	PreviousColorBoundTo = FMath::WrapExclusive(++PreviousColorBoundTo, 0, 3);

	int32 IndexToBindTo = PreviousColorBoundTo;
	
	if (IndexToBindTo == 0)
	{
		return Message_ChangeColor_Red;
	}
	else if (IndexToBindTo == 1)
	{
		return Message_ChangeColor_Green;
	}
	else if (IndexToBindTo == 2)
	{
		return Message_ChangeColor_Blue;
	}
	
	return Message_ChangeColor_Blue;
}

AHeavyPerformanceBroadcastor::AHeavyPerformanceBroadcastor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RootMeshComp"));
	SetRootComponent(MeshComp);
	
	MessagePayloadToQueue = FInstancedStruct::Make(FTestHeavyMessage{});

	// The tick function is TG_PrePhysics by default
	// Tick this actor every time pre-physics
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = CustomTickGroup;
}

void AHeavyPerformanceBroadcastor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	QueueMessage();	
}

void AHeavyPerformanceBroadcastor::QueueMessage()
{
	TSharedPtr<FAsyncGameplayMessageSystem> Sys = UAsyncMessageWorldSubsystem::GetSharedMessageSystem<FAsyncGameplayMessageSystem>(GetWorld());
	if (!Sys)
	{
		return;
	}

	for (int32 i = 0; i < NumberOfTimesToQueue; ++i)
	{
		// Queue a color change message
		Sys->QueueMessageForBroadcast(MessageToQueue, MessagePayloadToQueue);	
	}
}

AHeavyPerformanceListener::AHeavyPerformanceListener(const FObjectInitializer& ObjectInitializer)
{
	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ListenerRootMeshComp"));
	SetRootComponent(MeshComp);
	MeshComp->SetMobility(EComponentMobility::Movable);
}

void AHeavyPerformanceListener::BeginPlay()
{
	Super::BeginPlay();

	SetupListener();
}

void AHeavyPerformanceListener::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	
	RemoveListener();
}

void AHeavyPerformanceListener::HandleMessage(const FAsyncMessage& Message)
{
	const FTestHeavyMessage* Data = Message.GetPayloadData<const FTestHeavyMessage>();
	
	if (!Data)
	{
		ensure(false);
		return;
	}

	// Rotate the actor a little itsy bitsy, just to replicate doing some kind of simple gameplay work.
	AddActorLocalRotation(Data->AmountToRotate);
}

void AHeavyPerformanceListener::SetupListener()
{
	check(!ListenerHandle.IsValid());
	TSharedPtr<FAsyncGameplayMessageSystem> Sys = UAsyncMessageWorldSubsystem::GetSharedMessageSystem<FAsyncGameplayMessageSystem>(GetWorld());
	if (!Sys)
	{
		return;
	}

	FAsyncMessageBindingOptions BindingOpts = {};
	BindingOpts.SetTickGroup(GroupToListenFor);
	
	ListenerHandle = Sys->BindListener(
		MessageToListenFor,
		TWeakObjectPtr<AHeavyPerformanceListener>(this),
		&AHeavyPerformanceListener::HandleMessage,
		BindingOpts);

	ensure(ListenerHandle.IsValid());
}

void AHeavyPerformanceListener::RemoveListener()
{
	if (!ListenerHandle.IsValid())
	{
		return;
	}
	
	TSharedPtr<FAsyncGameplayMessageSystem> Sys = UAsyncMessageWorldSubsystem::GetSharedMessageSystem<FAsyncGameplayMessageSystem>(GetWorld());
	if (!Sys)
	{
		return;
	}

	Sys->UnbindListener(ListenerHandle);
}
