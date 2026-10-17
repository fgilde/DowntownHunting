"""Save the actors of a cs_region.py selection (Tools/Maps/<Name>.json) from the pruned City Sample map as /Game/DH/Maps/<Name>.
Run as commandlet: -run=pythonscript -script="<repo>/Tools/Python/save_region_map.py <Name>". Then flatten_dh_block.py <Name>."""
import json
import os
import sys

import unreal

REPO = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", ".."))
name = sys.argv[1]
sel = json.load(open(os.path.join(REPO, "Tools", "Maps", name + ".json")))
wanted = set(sel["play"] + sel["backdrop"] + sel["lights"])

unreal.EditorLoadingAndSavingUtils.load_map("/Game/Map/Small_City_LVL")
descs = [d for d in unreal.WorldPartitionBlueprintLibrary.get_actor_descs() if str(d.actor_package) in wanted]
unreal.WorldPartitionBlueprintLibrary.load_actors([d.guid for d in descs])
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
# Save-as keeps every actor the source references; drop what the selection doesn't want before saving.
extra = [a for a in actors if a.get_package().get_name() not in wanted and a.get_package().get_name().startswith("/Game/__ExternalActors__/")]
unreal.log("REGION %s wanted %d descs %d loaded %d extra %d" % (name, len(wanted), len(descs), len(actors), len(extra)))
ok = unreal.EditorLoadingAndSavingUtils.save_map(world, "/Game/DH/Maps/" + name)
unreal.log("REGION %s save_map %s" % (name, ok))
