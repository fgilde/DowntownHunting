// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class LyraServerSteamEOSTarget : LyraServerTarget
{
	public LyraServerSteamEOSTarget(TargetInfo Target) : base(Target)
	{
		CustomConfig = "SteamEOS";

		EnablePlugins.AddRange(
			new string[]
			{
				"OnlineServicesEOS",
				"OnlineSubsystemEOS"
			}
		);

		OptionalPlugins.Add("EOSReservedHooks");
	}
}
