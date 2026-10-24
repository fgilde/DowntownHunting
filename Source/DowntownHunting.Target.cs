// The packaged game: DowntownHunting.exe (Package.cmd). Lyra's game target with Steam online (Config/Custom/Steam);
// the module stays LyraGame.

using UnrealBuildTool;

public class DowntownHuntingTarget : LyraGameTarget
{
	public DowntownHuntingTarget(TargetInfo Target) : base(Target)
	{
		CustomConfig = "Steam";
	}
}
