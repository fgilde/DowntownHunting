"""Runs in the City Sample project. Resolves the characters in <repo>/Characters.json through City Sample's
CrowdCharacterDataAsset and writes
  Tools/Characters.resolved.json   meshes, per-slot material parameters, grooms (input for sync_characters.py)
  Tools/CitySample_Crowd.txt       City Sample packages to import (Import-CitySample.ps1 -Manifest ...)
  Tools/Crowd_catalog.txt          heads/outfits per gender and body type, to help editing Characters.json
The material values replicate City Sample's runtime setup for skeletal-mesh crowd characters
(CrowdBlueprintLibrary.cpp SetBodyMaterials/SetHeadMaterials/SetHairMaterials, FCrowdOutfitMaterialDefinition).

UnrealEditor-Cmd.exe CitySample.uproject -run=pythonscript -script="<repo>/Tools/Python/cs_crowd_export.py" -nullrhi
"""
import json
import os
import re

import unreal

REPO = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", ".."))
DATA = "/Game/Crowd/Character/Shared/Data/CrowdCharacterDataAsset"
HAIR_SLOTS = ["Hair", "Eyebrows", "Fuzz", "Eyelashes", "Mustache", "Beard"]  # ECrowdHairSlots order
HEAD_HAIR_MAPS = {  # slot: (attribute map, follicle mask, melanin, redness) on the head material
    "Beard": ("BeardAttributeMap", "FollicleMaskBeard", "BeardMelanin", "BeardRedness"),
    "Eyebrows": ("EyebrowsAttributeMap", None, "EyebrowsMelanin", "EyebrowsRedness"),
    "Hair": ("HairAttributeMap", "FollicleMaskHair", "HairMelanin", "HairRedness"),
    "Mustache": ("MustacheAttributeMap", "FollicleMaskMustache", "MustacheMelanin", "MustacheRedness"),
}
PATH = re.compile(r'(\w+)="(/Game/[^"]+)"')


def paths(struct):
    """Soft object references of a struct as {property: package path}, via its text export."""
    return {k: v.split(".")[0] for k, v in PATH.findall(struct.export_text())}


def mesh_of(data_asset):
    """Skeletal mesh of an AnimToTexture data asset (the crowd stores outfit/body meshes behind them)."""
    mesh = unreal.load_asset(data_asset).get_editor_property("skeletal_mesh")
    if isinstance(mesh, unreal.SoftObjectPath):
        return str(mesh.export_text()).split(".")[0]
    return mesh.get_path_name().split(".")[0] if mesh else None


def slots(mesh_path):
    mesh = unreal.load_asset(mesh_path)
    return [(str(m.material_slot_name), m.material_interface.get_path_name().split(".")[0] if m.material_interface else None)
            for m in mesh.materials]


def linear(c):  # FColor::ReinterpretAsLinear
    return [c.r / 255.0, c.g / 255.0, c.b / 255.0, c.a / 255.0]


def outfit_params(overrides, slot, color_index, option_index):
    """FCrowdOutfitMaterialDefinition::ApplyToComponent + GetPatternInfoForSlot for one material slot."""
    entry = overrides.get(slot)
    if entry is None:
        return None
    p = {"scalars": {}, "vectors": {}, "textures": {}}
    for o in entry.get_editor_property("parameter_overrides"):
        name, kind = str(o.get_editor_property("parameter_name")), str(o.get_editor_property("parameter_type"))
        if "VECTOR" in kind:
            v = o.get_editor_property("vector")
            p["vectors"][name] = [v.x, v.y, v.z, 0.0]
        elif "COLOR" in kind:
            p["vectors"][name] = linear(o.get_editor_property("color"))
        elif False:
            v = o.get_editor_property("vector")
            p["vectors"][name] = [v.x, v.y, v.z, 0.0]
        else:
            p["scalars"][name] = o.get_editor_property("float")
    scale, current = 1.0, entry
    for _ in range(8):
        usage = str(current.get_editor_property("pattern_usage"))
        if "PATTERN_LIST" in usage:
            colors, patterns = current.get_editor_property("complimentary_colors"), current.get_editor_property("patterns")
            if len(colors) and len(patterns):
                info = patterns[option_index % len(patterns)]
                p["vectors"]["A_CrowdColor_pattern"] = linear(colors[color_index % len(colors)])
                p["scalars"]["A_Pattern_Scale"] = info.get_editor_property("scale") * scale
                p["scalars"]["A_Pattern_Select"] = float(info.get_editor_property("selection"))
                p["scalars"]["A_Pattern_RoughMult"] = info.get_editor_property("roughness_multiplier")
            break
        if "DRIVEN" in usage:
            scale *= current.get_editor_property("scale_multiplier")
            current = overrides.get(str(current.get_editor_property("source_slot")))
            if current is None:
                break
        else:
            break
    return p


def skin_params(skin, modifier):
    return {"AtlasSelector": float(skin.get_editor_property("texture_atlas_index")),
            "MelaninRandomSeed": modifier.get_editor_property("offset_u"), "RednessRandomSeed": modifier.get_editor_property("offset_v"),
            "SourceU": skin.get_editor_property("source_u"), "SourceV": skin.get_editor_property("source_v"),
            "AlbedoDataMult": skin.get_editor_property("albedo_multiplier")}


cfg = json.load(open(os.path.join(REPO, "Characters.json"), encoding="utf-8"))
data = unreal.load_asset(DATA)
GENDER = {"A": unreal.CitySampleCrowdGender.A, "B": unreal.CitySampleCrowdGender.B}
BODY = {"NormalWeight": unreal.CitySampleCrowdBodyType.NORMAL_WEIGHT, "OverWeight": unreal.CitySampleCrowdBodyType.OVER_WEIGHT,
        "UnderWeight": unreal.CitySampleCrowdBodyType.UNDER_WEIGHT}

resolved, packages = [], set()
for c in cfg["characters"]:
    d = data.get_character_definition(GENDER[c["gender"]], BODY[c["body"]], c["head"], c["outfit"], c.get("outfitColor", 0),
                                      c["hair"], c["hairColor"], -1, c["skin"], c["skinTone"], 0)
    body_paths, outfit_paths = paths(d.get_editor_property("body_definition")), paths(d.get_editor_property("outfit_definition"))
    meshes = {"Body": mesh_of(body_paths["BodyData"]), "Head": paths(d)["Head"],
              "Top": mesh_of(outfit_paths["TopData"]), "Bottom": mesh_of(outfit_paths["BottomData"]), "Shoes": mesh_of(outfit_paths["ShoesData"])}
    skin, modifier = d.get_editor_property("skin_texture_definition"), d.get_editor_property("skin_texture_modifier_definition")
    skin_tex = paths(skin)
    skin_scalars = skin_params(skin, modifier)
    hair_color = d.get_editor_property("hair_color_definition")
    melanin, redness, roughness = (hair_color.get_editor_property(n) for n in ("hair_melanin", "hair_redness", "hair_roughness"))
    hair_defs = list(d.get_editor_property("hair_definitions"))
    overrides = {str(k): v for k, v in d.get_editor_property("outfit_material_definition").get_editor_property("material_overrides").items()}

    components = []
    for comp in ("Body", "Head", "Top", "Bottom", "Shoes"):
        mats = []
        for slot, parent in slots(meshes[comp]):
            p = None
            if comp == "Body" or comp == "Bottom" and slot == "M_Body":
                body_parent = slots(meshes["Body"])[0][1]  # the body mesh has one skin slot ("lambert1" on most)
                p = {"parent": body_parent, "scalars": dict(skin_scalars), "vectors": {}, "textures": {"Color_MAIN": skin_tex.get("BodyColor")}}
            elif comp == "Head" and slot.startswith("head_"):
                p = {"parent": parent, "scalars": dict(skin_scalars), "vectors": {},
                     "textures": {"Color_MAIN": skin_tex.get("FaceColor"), "Color_CHEST": skin_tex.get("ChestColor")}}
                for i, hair_slot in enumerate(HAIR_SLOTS):
                    if hair_slot not in HEAD_HAIR_MAPS or i >= len(hair_defs):
                        continue
                    attr, follicle, mel, red = HEAD_HAIR_MAPS[hair_slot]
                    p["scalars"][mel], p["scalars"][red] = melanin, redness
                    h = paths(hair_defs[i])
                    if "BakedGroomMap" in h and "Groom" not in h:  # strands rendered as groom at LOD0, baked map only without groom
                        p["textures"][attr] = h["BakedGroomMap"]
                    if follicle and "FollicleMask" in h:
                        p["textures"][follicle] = h["FollicleMask"]
            elif comp in ("Top", "Bottom", "Shoes"):
                o = outfit_params(overrides, slot, c.get("patternColor", 0), c.get("patternOption", 0))
                if o:
                    p = dict(o, parent=parent)
            mats.append({"slot": slot, "material": parent, "instance": p})
        components.append({"name": comp, "mesh": meshes[comp], "materials": mats})

    grooms = []
    for i, h in enumerate(hair_defs):
        hp = paths(h)
        if "Groom" not in hp:
            continue
        groom = unreal.load_asset(hp["Groom"])
        groom_mats = [m.get_editor_property("material").get_path_name().split(".")[0] if m.get_editor_property("material") else None
                      for m in groom.get_editor_property("hair_groups_materials")]
        grooms.append({"slot": HAIR_SLOTS[i], "groom": hp["Groom"], "binding": hp.get("GroomBinding"), "materials": groom_mats,
                       "scalars": {"hairMelanin": melanin, "hairRedness": redness, "HairRoughness": roughness}})

    entry = {"id": c["id"], "gender": c["gender"], "components": components, "grooms": grooms}
    resolved.append(entry)
    text = json.dumps(entry)
    packages.update(re.findall(r'"(/Game/[^"]+)"', text))
    unreal.log("CROWD %s: head %s, top %s, %d grooms" % (c["id"], meshes["Head"].split("/")[-1], meshes["Top"].split("/")[-1], len(grooms)))

json.dump({"characters": resolved}, open(os.path.join(REPO, "Tools", "Characters.resolved.json"), "w", encoding="utf-8"), indent=1)

registry = unreal.AssetRegistryHelpers.get_asset_registry()
options = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True, include_hard_package_references=True)
todo, seen = list(packages), set()
while todo:
    p = todo.pop()
    if p in seen:
        continue
    seen.add(p)
    for dep in registry.get_dependencies(p, options) or []:
        dep = str(dep)
        if dep.startswith("/Game/") and dep not in seen:
            todo.append(dep)
with open(os.path.join(REPO, "Tools", "CitySample_Crowd.txt"), "w", newline="\n") as f:
    f.write("# City Sample crowd character packages (Tools/Python/cs_crowd_export.py)\n" + "\n".join(sorted(seen)) + "\n")

with open(os.path.join(REPO, "Tools", "Crowd_catalog.txt"), "w", newline="\n") as f:
    for g, sk in (("A (male)", "skeleton_a"), ("B (female)", "skeleton_b")):
        gender = data.get_editor_property(sk)
        for body in ("normal_weight", "over_weight", "under_weight"):
            text = gender.get_editor_property(body).export_text()
            f.write("== gender %s, body %s\n" % (g, body))
            f.write("heads: %s\n" % ", ".join("%d=%s" % (i, h) for i, h in enumerate(re.findall(r"DA_(\w+?)_FaceMesh\.", text.split("Outfits=")[0]))))
            outfits = re.findall(r'TopData="[^"]*DA_(\w+?)\.\w+",BottomData="[^"]*DA_(\w+?)\.\w+",ShoesData="[^"]*DA_(\w+?)\.', text)
            for i, o in enumerate(outfits):
                f.write("outfit %d: %s + %s + %s\n" % (i, *o))
unreal.log("CROWD done: %d characters, %d packages" % (len(resolved), len(seen)))
