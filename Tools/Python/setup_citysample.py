"""Changes to imported City Sample assets that git does not track (City Sample content is not in git).

Run once after Tools/Import-CitySample.ps1:
  UnrealEditor-Cmd.exe DowntownHunting.uproject -run=pythonscript -script=Tools/Python/setup_citysample.py -nullrhi
"""
import unreal

LYRA_SKELETON = "/Game/Characters/Heroes/Mannequin/Meshes/SK_Mannequin.SK_Mannequin"

# Lets Lyra's mannequin animation blueprints (ABP_Mannequin_CopyPose) run on City Sample's MetaHuman-based meshes.
for path in ["/Game/Character/Shared/Skeleton/SK_Base", "/Game/Character/Shared/Skeleton/SK_Base_Head"]:
    skeleton = unreal.load_asset(path)
    skeleton.set_editor_property("compatible_skeletons", [unreal.load_asset(LYRA_SKELETON)])
    unreal.EditorAssetLibrary.save_loaded_asset(skeleton, False)
    unreal.log("setup_citysample: %s compatible with SK_Mannequin" % path)

# City Sample defines 9 physical surface types (1 Concrete ... 9 Ground), Lyra only 3 (1 Character, 2 Concrete,
# 3 Glass), and Lyra's impact effects switch on Lyra's numbering. Map City Sample's physical materials onto it:
# glass stays glass, everything else becomes concrete. Marked with a metadata tag so re-runs do nothing.
import os

TAG = "DH_SurfaceRemapped"
CITY_GLASS, LYRA_CONCRETE, LYRA_GLASS = 4, 2, 3
manifests = [os.path.join(os.path.dirname(__file__), "..", n) for n in ("CitySample_DH_Block.txt", "CitySample_Characters.txt")]
packages = set()
for manifest in manifests:
    with open(manifest) as f:
        packages.update(line.strip() for line in f if line.startswith("/Game/"))

registry = unreal.AssetRegistryHelpers.get_asset_registry()
remapped = 0
for data in registry.get_assets_by_class(unreal.TopLevelAssetPath("/Script/PhysicsCore", "PhysicalMaterial"), True):
    if str(data.package_name) not in packages:
        continue
    material = data.get_asset()
    if unreal.EditorAssetLibrary.get_metadata_tag(material, TAG) == "1":
        continue
    surface = material.get_editor_property("surface_type").value
    if surface != 0:
        lyra = LYRA_GLASS if surface == CITY_GLASS else LYRA_CONCRETE
        material.set_editor_property("surface_type", unreal.PhysicalSurface.cast(lyra))
    unreal.EditorAssetLibrary.set_metadata_tag(material, TAG, "1")
    unreal.EditorAssetLibrary.save_loaded_asset(material, False)
    remapped += 1
unreal.log("setup_citysample: remapped %d physical materials to Lyra surface types" % remapped)
