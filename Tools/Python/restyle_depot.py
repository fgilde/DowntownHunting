"""Lyra's Convolution arena, restyled as an old concrete depot: /DowntownCore/Maps/DH_Depot.
Copies the map (Lyra stays untouched), creates realistic triplanar materials from Lyra's own Megascans
surfaces and swaps the grid-line blockout materials on every mesh. City Sample textures don't work here:
they are virtual textures and Lyra's triplanar material samples regular ones (renders black in game).
Editor closed: UnrealEditor-Cmd.exe DowntownHunting.uproject -run=pythonscript -script="<repo>/Tools/Python/restyle_depot.py" -nullrhi
Re-running rebuilds the materials and re-applies them; the map copy is only made once."""
import unreal

SRC, DST = "/ShooterMaps/Maps/L_Convolution_Blockout", "/DowntownCore/Maps/DH_Depot"
MAT_DIR = "/DowntownCore/Maps/Materials"
TRIPLANAR = "/LyraExampleContent/MSPresets/M_MS_Surface_Material/M_MS_Surface_Material_TriPlanar"
WALL = "/LyraExampleContent/Megascans/Surfaces/Dirty_Painted_Concrete_Wall_vdqfcg3/"
TEX = "/LyraExampleContent/Materials/Textures/"
WALL_ORD = WALL + "T_DirtyPaintedConcreteWall_vdqfcg3_4K_ORDp"
SHELTER_ORD = "/LyraExampleContent/Megascans/Surfaces/Fortified_Shelter_Wall_ubimbhrdy/T_FortifiedShelterWall_ubimbhrdy_4K_ORDp"

# name: (albedo, normal, AO/R/D, tint, triplanar scale, min/max roughness, normal strength)
SURFACES = {
    "MI_DH_Depot_Concrete": (WALL + "T_Dirty_Painted_Concrete_Wall_vdqfcg3_4K_D", WALL + "T_Dirty_Painted_Concrete_Wall_vdqfcg3_4K_N",
                             WALL_ORD, (0.58, 0.56, 0.52), 400, (0.55, 0.95), 1.0),
    "MI_DH_Depot_Floor": (TEX + "T_Concrete_Diffuse", TEX + "T_Rough_Concrete_Floor_ulmmbiuo_4K_N",
                          SHELTER_ORD, (0.42, 0.41, 0.39), 300, (0.5, 0.9), 1.0),
    "MI_DH_Depot_Steel": (TEX + "T_Paint_Diffuse", TEX + "T_Paint_Normal",
                          WALL_ORD, (0.32, 0.36, 0.33), 200, (0.35, 0.75), 1.0),
    "MI_DH_Depot_WetFloor": (TEX + "T_Concrete_Diffuse", TEX + "T_Rough_Concrete_Floor_ulmmbiuo_4K_N",
                             SHELTER_ORD, (0.3, 0.3, 0.3), 300, (0.3, 0.7), 0.8),
}
SWAP = {
    "/LyraExampleContent/Materials/MI_MS_Surface_7": "MI_DH_Depot_Concrete",
    "/LyraExampleContent/Materials/MI_MS_Surface_1": "MI_DH_Depot_Floor",
    "/LyraExampleContent/Materials/MI_MS_Surface_13": "MI_DH_Depot_Steel",
    "/LyraExampleContent/Materials/MI_Black_GlossFakeFloor": "MI_DH_Depot_WetFloor",
    "/LyraExampleContent/Materials/MI_Glass_Tile": "MI_DH_Depot_Glass",
    "/LyraExampleContent/Materials/MI_Glass_Tile1": "MI_DH_Depot_Glass",
    "/LyraExampleContent/Materials/MI_GlassA_1": "MI_DH_Depot_Glass",
    "/ShooterCore/Effects/Material/M_Launcher_Inst3": "MI_DH_Depot_Lamp",  # cyan light strips -> warm fluorescent tubes
    "/ShooterCore/Effects/Material/MT_CP_Bar": "MI_DH_Depot_Lamp",
    "/Game/Tools/Materials/MT_DebugTool": "MI_DH_Depot_Steel",
}

unreal.AssetRegistryHelpers.get_asset_registry().search_all_assets(True)  # commandlet starts with a partial scan
assets = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary


def instance(name, parent):
    path = MAT_DIR + "/" + name
    mi = eal.load_asset(path) if eal.does_asset_exist(path) else \
        assets.create_asset(name, MAT_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property("parent", unreal.load_asset(parent))
    return mi


def surface(name, albedo, normal, ord_map, tint, scale, rough, normal_strength):
    mi = instance(name, TRIPLANAR)
    for p, t in (("AlbedoTriplanar", albedo), ("NormalMap", normal), ("AO/R/DTriplanar", ord_map)):
        MEL.set_material_instance_texture_parameter_value(mi, p, unreal.load_asset(t))
    for p in ("TriplanarScale_Albedo", "TriplanarScale_Roughness", "TriplanarScale_Normal"):
        MEL.set_material_instance_scalar_parameter_value(mi, p, scale)
    MEL.set_material_instance_scalar_parameter_value(mi, "Min Roughness", rough[0])
    MEL.set_material_instance_scalar_parameter_value(mi, "Max Roughness", rough[1])
    MEL.set_material_instance_scalar_parameter_value(mi, "Normal Strength", normal_strength)
    MEL.set_material_instance_vector_parameter_value(mi, "Albedo Tint", unreal.LinearColor(*tint, 1.0))
    for p in ("Use Grid", "UseSubGrid", "Use Invert Color Grid"):  # the blockout grid lines are most of the sci-fi look
        MEL.set_material_instance_static_switch_parameter_value(mi, p, False)
    MEL.update_material_instance(mi)
    return mi


materials = {n: surface(n, *v) for n, v in SURFACES.items()}
glass = instance("MI_DH_Depot_Glass", "/LyraExampleContent/Materials/M_Glass_Tile")
MEL.set_material_instance_vector_parameter_value(glass, "Color", unreal.LinearColor(0.82, 0.84, 0.82, 1.0))
MEL.set_material_instance_scalar_parameter_value(glass, "Roughness", 0.25)
MEL.update_material_instance(glass)
materials["MI_DH_Depot_Glass"] = glass
lamp = instance("MI_DH_Depot_Lamp", "/ShooterCore/Effects/Material/M_Launcher")
MEL.set_material_instance_vector_parameter_value(lamp, "Color", unreal.LinearColor(9.0, 7.6, 5.6, 0.0))
MEL.update_material_instance(lamp)
materials["MI_DH_Depot_Lamp"] = lamp
eal.save_directory(MAT_DIR)

if not eal.does_asset_exist(DST):
    unreal.EditorLoadingAndSavingUtils.load_map(SRC)
    unreal.WorldPartitionBlueprintLibrary.load_actors([d.guid for d in unreal.WorldPartitionBlueprintLibrary.get_actor_descs()])
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    unreal.log("DEPOT save_map %s" % unreal.EditorLoadingAndSavingUtils.save_map(world, DST))
    # Loading another map in this process trips the world leak check (the source's level blueprint stays referenced).
    unreal.log("DEPOT map copied, run the script again to apply the materials")
    raise SystemExit

unreal.EditorLoadingAndSavingUtils.load_map(DST)
unreal.WorldPartitionBlueprintLibrary.load_actors([d.guid for d in unreal.WorldPartitionBlueprintLibrary.get_actor_descs()])
swapped = 0
for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    for sky in actor.get_components_by_class(unreal.SkyLightComponent):
        # The copy has no BuiltData (Lyra's captured sky cubemap); a static capture in game waits for all textures first.
        actor.modify()
        sky.set_editor_property("real_time_capture", True)
    if actor.get_editor_property("is_spatially_loaded"):  # small arena: always loaded, also visible in the editor right away
        actor.modify()
        actor.set_editor_property("is_spatially_loaded", False)
    for comp in actor.get_components_by_class(unreal.MeshComponent):
        for i in range(comp.get_num_materials()):
            mat = comp.get_material(i)
            new = mat and SWAP.get(mat.get_path_name().split(".")[0])
            if new:
                actor.modify()
                comp.modify()
                comp.set_material(i, materials[new])
                swapped += 1
unreal.log("DEPOT swapped %d material slots" % swapped)
unreal.log("DEPOT save %s" % unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, False))  # map + actors only, not Lyra assets dirtied by loading
