"""save_region_map.py's save-as also copies actors of other regions already in the pruned source map.
Delete every actor of /Game/DH/Maps/<Name> that lies outside the region's ring box (Tools/Maps/<Name>.json)."""
import json
import os
import sys

import unreal

REPO = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", ".."))
name = sys.argv[1]
sel = json.load(open(os.path.join(REPO, "Tools", "Maps", name + ".json")))
(cx, cy), ring = sel["center"], sel["ring"] + 5000
LIGHTS = {"SkyLight", "DirectionalLight", "ExponentialHeightFog", "PostProcessVolume", "SkyAtmosphere", "VolumetricCloud"}

unreal.EditorLoadingAndSavingUtils.load_map("/Game/DH/Maps/" + name)
descs = unreal.WorldPartitionBlueprintLibrary.get_actor_descs()
unreal.WorldPartitionBlueprintLibrary.load_actors([d.guid for d in descs])
# Actor desc bounds, not get_actor_bounds: instanced-mesh actors report zero bounds in a -nullrhi commandlet.
far = set()
for d in descs:
    b = d.bounds
    x, y = (b.min.x + b.max.x) / 2, (b.min.y + b.max.y) / 2
    lighting = d.native_class and d.native_class.get_name() in LIGHTS
    if not lighting and (b.max - b.min).length() > 0 and (abs(x - cx) > ring or abs(y - cy) > ring):
        far.add(str(d.actor_package))
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
out = [a for a in actors if a.get_package().get_name() in far]
unreal.log("PRUNE %s actors %d delete %d" % (name, len(actors), len(out)))
unreal.get_editor_subsystem(unreal.EditorActorSubsystem).destroy_actors(out)
unreal.log("PRUNE save %s" % unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True))
