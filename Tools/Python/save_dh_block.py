"""Load every actor of the pruned City Sample map and save it as /Game/DH/Maps/DH_Block (World Partition save-as)."""
import unreal

SRC, DST = "/Game/Map/Small_City_LVL", "/Game/DH/Maps/DH_Block"

world = unreal.EditorLoadingAndSavingUtils.load_map(SRC)
descs = unreal.WorldPartitionBlueprintLibrary.get_actor_descs()
unreal.WorldPartitionBlueprintLibrary.load_actors([d.guid for d in descs])
loaded = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
unreal.log("DH_BLOCK descs %d loaded %d" % (len(descs), len(loaded)))
ok = unreal.EditorLoadingAndSavingUtils.save_map(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world(), DST)
unreal.log("DH_BLOCK save_map %s" % ok)
