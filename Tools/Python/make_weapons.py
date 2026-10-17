"""Extra Lyra weapons from Blacksite's CC0 meshes (/Game/Weapons/Classic, Classic Weapons Pack by Stein Games), step 1 of 2.
Duplicates Lyra's rifle/pistol chain (ID, WID, weapon instance, ability set, fire ability, damage effect, weapon actor)
into /DowntownCore/Weapons/<Name>, puts the new mesh into the weapon actor and adds a Muzzle socket to the mesh
("refresh" argument: only mesh and socket, keeps the copies and step 2's wiring).
Step 2, Tools/Python/mcp_weapon_values.py, wires the copies and sets stats in the running editor: Python cannot edit
most of these properties (EditDefaultsOnly, socket names), the editor's MCP tools can.
Editor closed: UnrealEditor-Cmd.exe DowntownHunting.uproject -run=pythonscript -script="<repo>/Tools/Python/make_weapons.py" -nullrhi"""
import sys

import unreal

ROOT = "/DowntownCore/Weapons"
# name: base weapon, mesh, muzzle socket location (mesh space, barrel along +Y)
WEAPONS = {"AK47": ("Rifle", "/Game/Weapons/Classic/AK47/SKM_AK47", (0, 71, 3.5)),
           "MP5": ("Rifle", "/Game/Weapons/Classic/MP5/SKM_MP5", (0, 41, 6.5)),
           "M1911": ("Pistol", "/Game/Weapons/Classic/1911/SKM_1911", (0, 17.5, 4.5))}
CHAIN = {"Rifle": ["B_Rifle", "GA_Weapon_Fire_Rifle_Auto", "AbilitySet_ShooterRifle", "B_WeaponInstance_Rifle", "WID_Rifle", "ID_Rifle"],
         "Pistol": ["B_Pistol", "GA_Weapon_Fire_Pistol", "AbilitySet_ShooterPistol", "B_WeaponInstance_Pistol", "WID_Pistol", "ID_Pistol"]}

eal = unreal.EditorAssetLibrary
subobjects = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
SOL = unreal.SubobjectDataBlueprintFunctionLibrary
unreal.AssetRegistryHelpers.get_asset_registry().search_all_assets(True)


def dup(src, dst):
    if eal.does_asset_exist(dst):
        eal.delete_asset(dst)
    return eal.duplicate_asset(src, dst)


def target_name(asset, base, name):
    return asset.replace("Shooter" + base, name).replace(base, name)


REFRESH = "refresh" in sys.argv[1:]

for name, (base, mesh_path, muzzle) in WEAPONS.items():
    src, dst = "/ShooterCore/Weapons/%s/" % base, "%s/%s/" % (ROOT, name)
    mesh = unreal.load_asset(mesh_path)
    # root bone of the pack's meshes is the parent of 'frame' (WPN_* -> frame, ANCHOR -> slide, magazine)
    unreal.DHEditorHelpers.set_mesh_socket(mesh, "Muzzle", mesh.get_bone_parent("frame"), unreal.Vector(*muzzle), unreal.Rotator())
    eal.save_loaded_asset(mesh)
    if not REFRESH:
        for asset in CHAIN[base]:
            dup(src + asset, dst + target_name(asset, base, name))
        # the fire ability's damage effect (the pistol's lives in another folder)
        ga = unreal.load_asset(dst + target_name(CHAIN[base][1], base, name))
        ge_class = unreal.get_default_object(ga.generated_class()).get_editor_property("GE_Damage")
        dup(ge_class.get_outer().get_path_name(), dst + "GE_Damage_" + name)

    actor_bp = unreal.load_asset(dst + "B_" + name)
    for handle in subobjects.k2_gather_subobject_data_for_blueprint(actor_bp):
        # the mesh component is inherited from B_Weapon: edit this blueprint's override, not the parent's template
        comp = SOL.get_object_for_blueprint(SOL.get_data(handle), actor_bp)
        if isinstance(comp, unreal.SkeletalMeshComponent):
            comp.set_editor_property("skeletal_mesh_asset", mesh)
    unreal.BlueprintEditorLibrary.compile_blueprint(actor_bp)
    if REFRESH:  # re-saving the untouched copies crashed (AnimationData) in a commandlet
        eal.save_loaded_asset(actor_bp)
        continue
    for path in eal.list_assets(dst, recursive=True, include_folder=False):
        eal.save_asset(path, only_if_is_dirty=False)
    unreal.log("WEAPONS %s: %s" % (name, [p.split("/")[-1].split(".")[0] for p in eal.list_assets(dst)]))
