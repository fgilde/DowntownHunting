"""Imports the character select art: Tools/Branding/Splash/*.png as UI textures into /Game/DH/UI/Splash and
Tools/Branding/Voices/*.wav as sound waves into /Game/DH/UI/Voices (both cooked via DirectoriesToAlwaysCook /Game/DH/UI).
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


textures = import_files(sorted((branding / 'Splash').glob('*.png')), '/Game/DH/UI/Splash')
for texture in textures:
    texture.set_editor_property('compression_settings', u.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property('lod_group', u.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property('mip_gen_settings', u.TextureMipGenSettings.TMGS_NO_MIPMAPS)
sounds = import_files(sorted((branding / 'Voices').glob('*.wav')), '/Game/DH/UI/Voices')
for asset in textures + sounds:
    u.EditorAssetLibrary.save_loaded_asset(asset, False)
print('DH import: %d textures, %d sounds' % (len(textures), len(sounds)))
