"""Turns World Partition streaming off for the city maps, so everything is always loaded: no pop-in, and bots far from
the player still have navmesh (with streaming, nav data only loaded around the player and bots got partial paths).
The maps are small cut-outs of City Sample, loading them whole is fine.
UnrealEditor-Cmd.exe DowntownHunting.uproject -run=pythonscript -script="<repo>/Tools/Python/disable_streaming.py [map ...]" -nullrhi"""
import sys

import unreal

MAPS = sys.argv[1:] or ["/Game/DH/Maps/DH_Block", "/Game/DH/Maps/DH_Park", "/Game/DH/Maps/DH_Freeway", "/Game/DH/Maps/DH_Roofs"]

for path in MAPS:
    unreal.EditorLoadingAndSavingUtils.load_map(path)
    world = unreal.EditorLevelLibrary.get_editor_world()
    ok = unreal.DHEditorHelpers.set_world_partition_streaming(world, False)
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, False)
    unreal.log("STREAMING %s off: %s" % (path, ok))
