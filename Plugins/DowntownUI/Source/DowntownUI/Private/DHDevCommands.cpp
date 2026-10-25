// Developer console commands to drive and check the menus without touching the real mouse/keyboard:
//   DH.OpenScreen Character|Loadout|Settings|<widget class path>   push a screen onto the menu layer
//   DH.Key <Key>[:Seconds] ...  press (and hold) keys through Slate, e.g. Gamepad_DPad_Down Gamepad_Special_Left:3 Escape
//   DH.Shot <Name> [noui]      screenshot (with UI unless "noui") to Saved/DHShots/<Name>.png
//   DH.Record <Seconds> <Name> [noui]  Seconds * 30 frames to Saved/Record/<Name>/ (+ times.txt, real time per frame);
//                              with -UseFixedTimeStep -FPS=30 every frame is 1/30 s of game time however slow the PC is
//   DH.Run cmd; wait 2; cmd    run console commands one after another, "wait N" pauses N seconds
//   DH.Teleport X Y Z [Yaw]    move the first player's pawn (listen server / standalone), e.g. onto a bomb site
// Example: -ExecCmds="DH.Run wait 40; DH.OpenScreen Loadout; wait 2; DH.Shot loadout"
// Not compiled into shipping builds.

#if !UE_BUILD_SHIPPING

#include "CommonActivatableWidget.h"
#include "CommonUIExtensions.h"
#include "Containers/Ticker.h"
#include "DHCharacterSelectScreen.h"
#include "DHControllerScreen.h"
#include "DHLoadoutScreen.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HighResScreenshot.h"
#include "InputCoreTypes.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace
{
	UWorld* GameWorld()
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE)
			{
				return Context.World();
			}
		}
		return nullptr;
	}

	void OpenScreen(const TArray<FString>& Args)
	{
		UWorld* World = GameWorld();
		ULocalPlayer* Player = World && World->GetGameInstance() ? World->GetGameInstance()->GetFirstGamePlayer() : nullptr;
		if (!Player || Args.IsEmpty())
		{
			return;
		}
		UClass* Class = nullptr;
		if (Args[0] == TEXT("Controller"))
		{
			Class = UDHControllerScreen::StaticClass();
		}
		else if (Args[0] == TEXT("Character"))
		{
			Class = UDHCharacterSelectScreen::StaticClass();
		}
		else if (Args[0] == TEXT("Loadout"))
		{
			Class = UDHLoadoutScreen::StaticClass();
		}
		else
		{
			const FString Path = Args[0] == TEXT("Settings") ? TEXT("/Game/UI/Settings/W_LyraSettingScreen.W_LyraSettingScreen_C") : Args[0];
			Class = LoadClass<UCommonActivatableWidget>(nullptr, *Path);
		}
		if (Class)
		{
			UCommonUIExtensions::PushContentToLayer_ForPlayer(Player, FGameplayTag::RequestGameplayTag(TEXT("UI.Layer.Menu")), Class);
		}
	}

	void PressKeys(const TArray<FString>& Args)
	{
		FSlateApplication& Slate = FSlateApplication::Get();
		for (const FString& Arg : Args)
		{
			// "Key" or "Key:Seconds" to hold it
			FString Name = Arg, Hold;
			Arg.Split(TEXT(":"), &Name, &Hold);
			Name = Name.IsEmpty() ? Arg : Name;
			const float Seconds = Hold.IsEmpty() ? 0.1f : FCString::Atof(*Hold);
			const FKey Key(*Name);
			if (!Key.IsValid())
			{
				UE_LOG(LogTemp, Warning, TEXT("DH.Key: unknown key %s"), *Name);
				continue;
			}
			UE_LOG(LogTemp, Log, TEXT("DHUI key %s"), *Name);
			const FKeyEvent Event(Key, FModifierKeysState(), 0, false, 0, 0);
			Slate.ProcessKeyDownEvent(Event);
			// release a moment later like a real button, not in the same frame
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Event](float)
			{
				FSlateApplication::Get().ProcessKeyUpEvent(Event);
				return false;
			}), Seconds);
		}
	}

	void Shot(const TArray<FString>& Args)
	{
		const FString Name = Args.IsEmpty() ? TEXT("shot") : Args[0];
		const bool bShowUI = !(Args.Num() > 1 && Args[1].Equals(TEXT("noui"), ESearchCase::IgnoreCase));
		FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("DHShots") / (Name + TEXT(".png")), bShowUI, false);
	}

	void Record(const TArray<FString>& Args)
	{
		const int32 Frames = FMath::RoundToInt((Args.Num() > 0 ? FCString::Atof(*Args[0]) : 5.f) * 30.f);
		const FString Dir = FPaths::ProjectSavedDir() / TEXT("Record") / (Args.Num() > 1 ? Args[1] : TEXT("clip"));
		const bool bShowUI = !(Args.Num() > 2 && Args[2].Equals(TEXT("noui"), ESearchCase::IgnoreCase));
		IFileManager::Get().DeleteDirectory(*Dir, false, true);
		IFileManager::Get().MakeDirectory(*Dir, true);
		// Frames counted in game time; the real times go to times.txt, for clips recorded without a fixed time step
		// (menus: their background videos run in real time) to be cut with the real frame durations.
		int32 Frame = 0;
		TArray<FString> Times;
		const double Start = FPlatformTime::Seconds();
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Dir, Frames, bShowUI, Frame, Times, Start](float) mutable
		{
			FScreenshotRequest::RequestScreenshot(Dir / FString::Printf(TEXT("%04d.png"), Frame), bShowUI, false);
			Times.Add(FString::Printf(TEXT("%.4f"), FPlatformTime::Seconds() - Start));
			if (++Frame < Frames)
			{
				return true;
			}
			FFileHelper::SaveStringArrayToFile(Times, *(Dir / TEXT("times.txt")));
			UE_LOG(LogTemp, Log, TEXT("DH.Record: %d frames in %s"), Frames, *Dir);
			return false;
		}));
	}

	void Teleport(const TArray<FString>& Args)
	{
		UWorld* World = GameWorld();
		APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
		APawn* Pawn = Player ? Player->GetPawn() : nullptr;
		if (Pawn && Args.Num() >= 3)
		{
			Pawn->TeleportTo(FVector(FCString::Atod(*Args[0]), FCString::Atod(*Args[1]), FCString::Atod(*Args[2])), Pawn->GetActorRotation());
			if (Args.Num() >= 4)
			{
				Player->SetControlRotation(FRotator(0.f, FCString::Atof(*Args[3]), 0.f));
			}
		}
	}

	void RunSteps(TArray<FString> Steps)
	{
		while (!Steps.IsEmpty())
		{
			const FString Step = Steps[0].TrimStartAndEnd();
			Steps.RemoveAt(0);
			float Seconds = 0.f;
			if (Step.StartsWith(TEXT("wait ")) && LexTryParseString(Seconds, *Step.Mid(5)))
			{
				FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Steps](float) mutable
				{
					RunSteps(MoveTemp(Steps));
					return false;
				}), Seconds);
				return;
			}
			if (!Step.IsEmpty())
			{
				GEngine->Exec(GameWorld(), *Step);
			}
		}
	}

	void Run(const TArray<FString>& Args)
	{
		TArray<FString> Steps;
		FString::Join(Args, TEXT(" ")).ParseIntoArray(Steps, TEXT(";"));
		RunSteps(MoveTemp(Steps));
	}

	FAutoConsoleCommand OpenScreenCommand(TEXT("DH.OpenScreen"), TEXT("Push a menu screen: Character, Loadout, Settings or a widget class path"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&OpenScreen));
	FAutoConsoleCommand PadDumpCommand(TEXT("DH.PadDump"), TEXT("Log the keys each gamepad layout action is mapped to"),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			UWorld* World = GameWorld();
			UDHControllerScreen::LogBindings(World && World->GetGameInstance() ? World->GetGameInstance()->GetFirstGamePlayer() : nullptr);
		}));
	FAutoConsoleCommand KeyCommand(TEXT("DH.Key"), TEXT("Press keys through Slate, e.g. Gamepad_DPad_Down Gamepad_FaceButton_Bottom"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&PressKeys));
	FAutoConsoleCommand ShotCommand(TEXT("DH.Shot"), TEXT("Screenshot to Saved/DHShots/<Name>.png, 'noui' as second argument hides the UI"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&Shot));
	FAutoConsoleCommand RecordCommand(TEXT("DH.Record"), TEXT("Every frame for N seconds to Saved/Record/<Name>/: DH.Record <Seconds> <Name> [noui]"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&Record));
	FAutoConsoleCommand TeleportCommand(TEXT("DH.Teleport"), TEXT("Move the first player's pawn to X Y Z, optionally looking along Yaw"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&Teleport));
	FAutoConsoleCommand RunCommand(TEXT("DH.Run"), TEXT("Run console commands separated by ';', 'wait N' pauses N seconds"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&Run));
}

#endif
