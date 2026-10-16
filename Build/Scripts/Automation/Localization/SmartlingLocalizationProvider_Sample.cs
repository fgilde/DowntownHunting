// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using AutomationTool;
using UnrealBuildTool;
using EpicGames.SmartlingLocalization;

public class SmartlingLocalizationProvider_Sample : SmartlingLocalizationProvider
{
	public SmartlingLocalizationProvider_Sample (LocalizationProviderArgs InArgs)
		: base(InArgs)
	{
		Config.UserId = Command.ParseParamValue("SmartlingUserId");
		Config.ProjectId = Command.ParseParamValue("SmartlingProjectId");
		Config.UserSecret = Command.ParseParamValue("SmartlingAPISecret");
	}

	public static string StaticGetLocalizationProviderId()
	{
		return "Smartling_Sample";
	}

	public override string GetLocalizationProviderId()
	{
		return StaticGetLocalizationProviderId();
	}
}
