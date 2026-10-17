"""Copies the City Sample backdrop ring (Tools/CitySample_DH_Block_Ring_Actors.json) from the pruned
/Game/Map/Small_City_LVL into /Game/DH/Maps/DH_Block, always loaded and without City Sample data layers.
Uses the editor's copy/paste (EDIT COPY / EDIT PASTE), which keeps world positions."""
import json
import os

import unreal

ROOT = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", ".."))
SOURCE, TARGET = "/Game/Map/Small_City_LVL", "/Game/DH/Maps/DH_Block"
FOLDER = "CitySample_Backdrop"

actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
packages = set(json.load(open(os.path.join(ROOT, "Tools", "CitySample_DH_Block_Ring_Actors.json"))))

unreal.EditorLoadingAndSavingUtils.load_map(SOURCE)
guids = [d.guid for d in unreal.WorldPartitionBlueprintLibrary.get_actor_descs() if str(d.actor_package) in packages]
unreal.WorldPartitionBlueprintLibrary.load_actors(guids)
ring = [a for a in actor_sub.get_all_level_actors() if a.get_package().get_name() in packages]
unreal.log("DH_RING source actors %d of %d" % (len(ring), len(packages)))
actor_sub.set_selected_level_actors(ring)
unreal.SystemLibrary.execute_console_command(editor.get_editor_world(), "EDIT COPY")

unreal.EditorLoadingAndSavingUtils.load_map(TARGET)
descs = unreal.WorldPartitionBlueprintLibrary.get_actor_descs()
unreal.WorldPartitionBlueprintLibrary.load_actors([d.guid for d in descs])
before = set(a.get_path_name() for a in actor_sub.get_all_level_actors())
actor_sub.select_nothing()
unreal.SystemLibrary.execute_console_command(editor.get_editor_world(), "EDIT PASTE")
pasted = [a for a in actor_sub.get_all_level_actors() if a.get_path_name() not in before]
unreal.get_editor_subsystem(unreal.DataLayerEditorSubsystem).remove_actors_from_all_data_layers(pasted)
for a in pasted:
    a.set_editor_property("is_spatially_loaded", False)
    a.set_folder_path(FOLDER)
unreal.log("DH_RING pasted %d" % len(pasted))
unreal.log("DH_RING save %s" % unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True))

if "-quitafter" in unreal.SystemLibrary.get_command_line().lower():
    unreal.SystemLibrary.quit_editor()
