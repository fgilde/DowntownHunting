"""Builds the crowd characters from Tools/Characters.resolved.json (made by cs_crowd_export.py in City Sample) as Lyra
cosmetic parts: /DowntownCore/Characters/Crowd/<Id>/B_DH_Char_<Id> with baked material instances, and writes the character
list for the selection (Config/DefaultGame.ini, UDHCharacterSettings). Editor closed; run via Sync-Characters.cmd.

Each part copies the pawn's pose through Lyra's ABP_Mannequin_CopyPose; the crowd skeletons are marked compatible with
SK_Mannequin for that. Hair is groom (HairStrands plugin) bound to the face mesh."""
import json
import os
import re

import unreal

REPO = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", ".."))
ROOT = "/DowntownCore/Characters/Crowd"
TEMPLATE = "/DowntownCore/Characters/B_DH_PlayerMale"
COPY_POSE = "/Game/Characters/Heroes/Mannequin/Animations/ABP_Mannequin_CopyPose.ABP_Mannequin_CopyPose_C"
MANNEQUIN = "/Game/Characters/Heroes/Mannequin/Meshes/SK_Mannequin"
SKELETONS = ["/Game/Crowd/Character/Shared/Rig/metahuman_base_skel", "/Game/Crowd/Character/Shared/Rig/Face_Archetype_Skeleton"]
FIXED = [("Hunter", "/DowntownCore/Characters/B_DH_PlayerMale.B_DH_PlayerMale_C"),
         ("Huntress", "/DowntownCore/Characters/B_DH_PlayerFemale.B_DH_PlayerFemale_C")]

eal = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
subobjects = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
SOL = unreal.SubobjectDataBlueprintFunctionLibrary
unreal.AssetRegistryHelpers.get_asset_registry().search_all_assets(True)

mannequin = unreal.load_asset(MANNEQUIN)
for path in SKELETONS:
    skeleton = unreal.load_asset(path)
    compatible = list(skeleton.get_editor_property("compatible_skeletons"))
    if mannequin not in compatible:
        skeleton.set_editor_property("compatible_skeletons", compatible + [mannequin])
        eal.save_loaded_asset(skeleton)


def instance(folder, name, spec):
    """Material instance constant with the baked crowd parameters."""
    path = folder + "/" + name
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    mi = tools.create_asset(name, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property("parent", unreal.load_asset(spec["parent"]))
    for k, v in spec.get("scalars", {}).items():
        MEL.set_material_instance_scalar_parameter_value(mi, k, float(v))
    for k, v in spec.get("vectors", {}).items():
        MEL.set_material_instance_vector_parameter_value(mi, k, unreal.LinearColor(*v))
    for k, v in spec.get("textures", {}).items():
        if v:
            MEL.set_material_instance_texture_parameter_value(mi, k, unreal.load_asset(v))
    MEL.update_material_instance(mi)
    eal.save_loaded_asset(mi)
    return mi


def component_object(handle):
    return SOL.get_object(SOL.get_data(handle))


def add_component(bp, parent_handle, cls, name):
    handle, _ = subobjects.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=parent_handle, new_class=cls, blueprint_context=bp))
    subobjects.rename_subobject(handle, unreal.Text(name))
    return handle


def setup_skeletal(comp, mesh, materials):
    comp.set_editor_property("skeletal_mesh_asset", unreal.load_asset(mesh))
    comp.set_editor_property("animation_mode", unreal.AnimationMode.ANIMATION_BLUEPRINT)
    comp.set_editor_property("anim_class", unreal.load_class(None, COPY_POSE))
    comp.set_editor_property("override_materials", materials)


cfg = json.load(open(os.path.join(REPO, "Tools", "Characters.resolved.json"), encoding="utf-8"))
entries = list(FIXED)
for c in cfg["characters"]:
    cid, folder = c["id"], ROOT + "/" + c["id"]
    bp_path = folder + "/B_DH_Char_" + cid
    if eal.does_asset_exist(bp_path):
        eal.delete_asset(bp_path)
    bp = eal.duplicate_asset(TEMPLATE, bp_path)

    handles = subobjects.k2_gather_subobject_data_for_blueprint(bp)
    mesh_handle = next(h for h in handles if component_object(h) and "MeshComponent" in component_object(h).get_name()
                       and isinstance(component_object(h), unreal.SkeletalMeshComponent))
    body_mi = None
    comp_handles = {}
    for comp in c["components"]:
        mats = []
        for m in comp["materials"]:
            spec = m["instance"]
            if not spec or not spec.get("parent"):
                mats.append(unreal.load_asset(m["material"]) if m["material"] else None)
                continue
            if comp["name"] == "Bottom" and m["slot"] == "M_Body" and body_mi:
                mats.append(body_mi)  # exposed legs share the upper body's skin
                continue
            mi = instance(folder, "MI_%s_%s_%s" % (cid, comp["name"], re.sub(r"\W", "", m["slot"])), spec)
            if comp["name"] == "Body":
                body_mi = mi
            mats.append(mi)
        if comp["name"] == "Body":
            handle = mesh_handle
        else:
            parent = comp_handles.get("Body", mesh_handle)
            handle = add_component(bp, parent, unreal.SkeletalMeshComponent, comp["name"])
        setup_skeletal(component_object(handle), comp["mesh"], mats)
        comp_handles[comp["name"]] = handle

    for g in c["grooms"]:
        handle = add_component(bp, comp_handles["Head"], unreal.GroomComponent, "Groom_" + g["slot"])
        groom = component_object(handle)
        groom.set_editor_property("groom_asset", unreal.load_asset(g["groom"]))
        if g["binding"]:
            groom.set_editor_property("binding_asset", unreal.load_asset(g["binding"]))
        groom_mats = [instance(folder, "MI_%s_%s_%d" % (cid, g["slot"], i), {"parent": p, "scalars": g["scalars"]}) if p else None
                      for i, p in enumerate(g["materials"])]
        groom.set_editor_property("override_materials", groom_mats)

    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    eal.save_loaded_asset(bp)
    entries.append((cid, "%s.B_DH_Char_%s_C" % (bp_path, cid)))
    unreal.log("CHARS %s: %d parts, %d grooms" % (cid, len(c["components"]), len(c["grooms"])))

# character list for UDHCharacterSettings (replaces the section on every sync)
ini = os.path.join(REPO, "Config", "DefaultGame.ini")
text = open(ini, encoding="utf-8").read()
section = "[/Script/DowntownUI.DHCharacterSettings]"
text = re.sub(re.escape(section) + r"\n(?:[^\[\n][^\n]*\n?)*", "", text).rstrip() + "\n\n" + section + "\n"
text += "".join('+Characters=(Id="%s",PartClass="%s")\n' % e for e in entries)
open(ini, "w", encoding="utf-8").write(text)
unreal.log("CHARS done: %d characters" % len(entries))
