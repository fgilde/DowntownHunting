using UnrealBuildTool;

public class DowntownUI : ModuleRules
{
	public DowntownUI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "CommonUI", "CommonInput" });
		PrivateDependencyModuleNames.AddRange(new string[] { "UMG", "Slate", "SlateCore", "CommonGame", "CommonLoadingScreen", "CommonUser", "GameplayTags", "InputCore", "EnhancedInput", "MediaAssets", "AudioMixer", "RHI", "RenderCore" });
	}
}
