"""Imports the menu art and sound (all cooked via DirectoriesToAlwaysCook /Game/DH/UI):
- Tools/Branding/Splash/*.png -> /Game/DH/UI/Splash (character select splash art)
- Tools/Branding/MenuArt/T_DH_Art_*.png -> /Game/DH/UI/MenuArt (main menu showcase), T_DH_Ability_*.png -> /Game/DH/UI/Abilities
- Tools/Branding/Voices/*.wav -> /Game/DH/UI/Voices (character voice lines)
- Tools/Branding/Music/*.wav -> /Game/DH/UI/Music (menu music: looping, sound class Music so the music volume applies)
Editor closed: UnrealEditor-Cmd.exe DowntownHunting.uproject -run=pythonscript -script="<repo>/Tools/Python/import_menu_art.py" -nullrhi"""
from pathlib import Path
import unreal as u

branding = Path(__file__).resolve().parents[1] / 'Branding'
tools = u.AssetToolsHelpers.get_asset_tools()


def import_files(sources, folder):
    tasks = []
    for source in sources:
        task = u.AssetImportTask()
        task.filename = str(source)
        task.destination_path = folder
        task.destination_name = source.stem
        task.automated = True
        task.replace_existing = True
        task.save = False
        tasks.append(task)
    tools.import_asset_tasks(tasks)
    return [u.load_asset(path) for task in tasks for path in task.imported_object_paths]


textures = (import_files(sorted((branding / 'Splash').glob('*.png')), '/Game/DH/UI/Splash')
            + import_files(sorted((branding / 'MenuArt').glob('T_DH_Art_*.png')), '/Game/DH/UI/MenuArt')
            + import_files(sorted((branding / 'MenuArt').glob('T_DH_Ability_*.png')), '/Game/DH/UI/Abilities'))
for texture in textures:
    texture.set_editor_property('compression_settings', u.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property('lod_group', u.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property('mip_gen_settings', u.TextureMipGenSettings.TMGS_NO_MIPMAPS)
sounds = import_files(sorted((branding / 'Voices').glob('*.wav')), '/Game/DH/UI/Voices')
music = import_files(sorted((branding / 'Music').glob('*.wav')), '/Game/DH/UI/Music')
music_class = u.load_asset('/Game/Audio/Classes/Music')
for wave in music:
    wave.set_editor_property('looping', True)
    wave.set_editor_property('sound_class_object', music_class)
for asset in textures + sounds + music:
    u.EditorAssetLibrary.save_loaded_asset(asset, False)
print('DH import: %d textures, %d sounds, %d music' % (len(textures), len(sounds), len(music)))
