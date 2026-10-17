"""Runs in the City Sample project. Selects a play area (+ backdrop ring) of Small_City_LVL and writes
   Tools/Maps/<Name>.json        actor packages, centre, sizes
   Tools/CitySample_<Name>.txt   City Sample packages to import (Import-CitySample.ps1 -Manifest ...)

UnrealEditor-Cmd.exe CitySample.uproject -run=pythonscript -script="<repo>/Tools/Python/cs_region.py <Name> <x_cm> <y_cm> [half_cm] [ring_cm]" -nullrhi
"""
import json
import os
import sys

import unreal

REPO = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", ".."))
name, cx, cy = sys.argv[1], float(sys.argv[2]), float(sys.argv[3])
half = float(sys.argv[4]) if len(sys.argv) > 4 else 12500.0
ring = float(sys.argv[5]) if len(sys.argv) > 5 else 40000.0

PLAY_LAYERS = {"Buildings", "Buildings_Collision", "Buildings_RoofGeo", "ALL_ROOFTOP", "Buildings_Props", "Buildings_WindowStickers",
               "Ground", "Ground_Collision", "Ground_Decals", "StreetFurniture", "StreetFurniture_Parked", "StreetFuniture_Dependant",
               "StreetFurniture_TrashCans", "Parked_Cars", "TrafficLights", "Hero_Buildings"}
RING_LAYERS = {"Buildings", "Buildings_RoofGeo", "Ground", "Hero_Buildings", "Buildings_Collision", "Ground_Collision"}
LIGHTS = {"SkyLight", "DirectionalLight", "ExponentialHeightFog", "PostProcessVolume", "SkyAtmosphere", "VolumetricCloud"}
SKIP_CLASSES = ("WorldAudioDataClusterActor", "WorldPartitionHLOD", "MassSpawner", "MassTraffic", "ZoneGraph", "SliceAndDice")

unreal.EditorLoadingAndSavingUtils.load_map("/Game/Map/Small_City_LVL")
play, backdrop, lights = [], [], []
for d in unreal.WorldPartitionBlueprintLibrary.get_actor_descs():
    native = d.native_class.get_name() if d.native_class else ""
    if any(s in native for s in SKIP_CLASSES):
        continue
    package = str(d.actor_package)
    if native in LIGHTS and not d.is_spatially_loaded:
        lights.append(package)
        continue
    b = d.bounds
    x, y = (b.min.x + b.max.x) / 2, (b.min.y + b.max.y) / 2
    dx, dy = abs(x - cx), abs(y - cy)
    layers = {p.export_text().split("/")[-1].split(".")[0] for p in d.data_layer_assets}
    if dx <= half and dy <= half and (layers & PLAY_LAYERS or (not layers and "TRAFFIC_LIGHT" in str(d.label))):
        play.append(package)
    elif dx <= ring and dy <= ring and layers & RING_LAYERS:
        backdrop.append(package)

registry = unreal.AssetRegistryHelpers.get_asset_registry()
options = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True, include_hard_package_references=True)
todo, seen = play + backdrop + lights, set()
while todo:
    p = todo.pop()
    if p in seen:
        continue
    seen.add(p)
    for dep in registry.get_dependencies(p, options) or []:
        dep = str(dep)
        if dep.startswith("/Game/") and dep not in seen and not dep.startswith("/Game/__ExternalActors__/"):
            todo.append(dep)

os.makedirs(os.path.join(REPO, "Tools", "Maps"), exist_ok=True)
json.dump({"name": name, "center": [cx, cy], "half": half, "ring": ring, "play": play, "backdrop": backdrop, "lights": lights},
          open(os.path.join(REPO, "Tools", "Maps", name + ".json"), "w"), indent=0)
with open(os.path.join(REPO, "Tools", "CitySample_%s.txt" % name), "w", newline="\n") as f:
    f.write("# City Sample packages for %s (Tools/Python/cs_region.py)\n" % name)
    f.write("\n".join(sorted(p for p in seen if p.startswith("/Game/"))) + "\n")
unreal.log("CS_REGION %s: play %d, backdrop %d, lights %d, packages %d" % (name, len(play), len(backdrop), len(lights), len(seen)))
