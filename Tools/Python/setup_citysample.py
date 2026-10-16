"""Changes to imported City Sample assets that git does not track (City Sample content is not in git).

Run once after Tools/Import-CitySample.ps1:
  UnrealEditor-Cmd.exe DowntownHunting.uproject -run=pythonscript -script=Tools/Python/setup_citysample.py -nullrhi
"""
import unreal

LYRA_SKELETON = "/Game/Characters/Heroes/Mannequin/Meshes/SK_Mannequin.SK_Mannequin"

# Lets Lyra's mannequin animation blueprints (ABP_Mannequin_CopyPose) run on City Sample's MetaHuman-based meshes.
for path in ["/Game/Character/Shared/Skeleton/SK_Base", "/Game/Character/Shared/Skeleton/SK_Base_Head"]:
    skeleton = unreal.load_asset(path)
    skeleton.set_editor_property("compatible_skeletons", [unreal.SoftObjectPath(LYRA_SKELETON)])
    unreal.EditorAssetLibrary.save_loaded_asset(skeleton, False)
    unreal.log("setup_citysample: %s compatible with SK_Mannequin" % path)
