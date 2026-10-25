// Copyright Epic Games, Inc. All Rights Reserved.

#include "DowntownCoreRuntimeModule.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Teams/LyraTeamDisplayAsset.h"
#include "Teams/LyraTeamSubsystem.h"

#define LOCTEXT_NAMESPACE "FDowntownCoreRuntimeModule"

void FDowntownCoreRuntimeModule::StartupModule()
{
	// Team colours from the viewer's side: the own team is always blue, every other team red, on every machine (so in an
	// online match both players see themselves blue). Lyra asks its team subsystem for every team colour (characters,
	// arm bands, name plates, kill feed, control points, scores); UDHGameRulesComponent refreshes them when the local
	// player's team changes. Dedicated servers and viewers without a team keep the teams' own colours.
	ULyraTeamSubsystem::DisplayAssetPerspective = [](const ULyraTeamSubsystem& Teams, int32 TeamId, ULyraTeamDisplayAsset* TeamAsset)
	{
		const UWorld* World = Teams.GetWorld();
		const APlayerController* Viewer = World && World->GetNetMode() != NM_DedicatedServer ? World->GetFirstPlayerController() : nullptr;
		const int32 ViewerTeam = Viewer ? Teams.FindTeamFromObject(Viewer) : INDEX_NONE;
		if (ViewerTeam == INDEX_NONE)
		{
			return TeamAsset;
		}
		auto Keep = [](const TCHAR* Path)
		{
			ULyraTeamDisplayAsset* Asset = LoadObject<ULyraTeamDisplayAsset>(nullptr, Path);
			if (Asset)
			{
				Asset->AddToRoot();  // also in modes whose teams do not use it
			}
			return Asset;
		};
		static ULyraTeamDisplayAsset* Blue = Keep(TEXT("/Game/System/Teams/TeamDA_Blue.TeamDA_Blue"));
		static ULyraTeamDisplayAsset* Red = Keep(TEXT("/Game/System/Teams/TeamDA_Red.TeamDA_Red"));
		ULyraTeamDisplayAsset* Side = TeamId == ViewerTeam ? Blue : Red;
		return Side ? Side : TeamAsset;
	};
}

void FDowntownCoreRuntimeModule::ShutdownModule()
{
	ULyraTeamSubsystem::DisplayAssetPerspective = nullptr;
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FDowntownCoreRuntimeModule, DowntownCoreRuntime)
