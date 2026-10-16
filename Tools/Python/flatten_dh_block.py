"""DH_Block is small: load everything always (no spatial streaming) and drop City Sample's data layers."""
import unreal

MAP = "/Game/DH/Maps/DH_Block"

unreal.EditorLoadingAndSavingUtils.load_map(MAP)
descs = unreal.WorldPartitionBlueprintLibrary.get_actor_descs()
unreal.WorldPartitionBlueprintLibrary.load_actors([d.guid for d in descs])
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()

layers = unreal.get_editor_subsystem(unreal.DataLayerEditorSubsystem)
layers.remove_actors_from_all_data_layers(actors)
changed = 0
for a in actors:
    if a.get_editor_property("is_spatially_loaded"):
        a.set_editor_property("is_spatially_loaded", False)
        changed += 1
unreal.log("DH_FLATTEN actors %d made always-loaded %d" % (len(actors), changed))
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
unreal.log("DH_FLATTEN save %s" % unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True))
