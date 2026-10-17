using UnrealBuildTool;

public class DowntownUI : ModuleRules
{
	public DowntownUI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "CommonUI" });
		PrivateDependencyModuleNames.AddRange(new string[] { "UMG", "Slate", "SlateCore", "CommonGame", "CommonLoadingScreen", "GameplayTags", "InputCore", "MediaAssets", "RHI", "RenderCore" });
	}
}
