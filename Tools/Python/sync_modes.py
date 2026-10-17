"""Creates/updates the menu entries (Lyra user-facing experiences) in /DowntownCore/Playlists from Modes.json.
Entries named DA_DH_* that are no longer in Modes.json are deleted. Run through Sync-Modes.cmd."""
import json
import os

import unreal

ROOT = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", ".."))
FOLDER = "/DowntownCore/Playlists"
GAMES = {
    # game: (experience, template entry that provides tile icon and loading screen)
    "TDM": ("B_DH_TDM", "/ShooterMaps/System/Playlists/DA_Expanse_TDM"),
    "TDM3": ("B_DH_TDM3", "/ShooterMaps/System/Playlists/DA_Expanse_TDM"),
    "Domination": ("B_DH_Domination", "/ShooterMaps/System/Playlists/DA_Convolution_ControlPoint"),
}

with open(os.path.join(ROOT, "Modes.json"), encoding="utf-8") as f:
    modes = json.load(f)["modes"]

assets = unreal.EditorAssetLibrary


def asset_id(kind, name):
    return unreal.PrimaryAssetId(unreal.PrimaryAssetType(kind), name)


wanted = set()
for mode in modes:
    experience, template = GAMES[mode["game"]]
    name = "DA_DH_" + mode["id"]
    path = FOLDER + "/" + name
    wanted.add(path)
    if not assets.does_asset_exist(path):
        assets.duplicate_asset(template, path)
    entry = unreal.load_asset(path)
    entry.set_editor_property("map_id", asset_id("Map", mode["map"]))
    entry.set_editor_property("experience_id", asset_id("LyraExperienceDefinition", experience))
    entry.set_editor_property("extra_args", {k: str(v) for k, v in mode.get("rules", {}).items()})
    entry.set_editor_property("tile_title", mode["title"])
    entry.set_editor_property("tile_sub_title", mode.get("subtitle", ""))
    entry.set_editor_property("tile_description", mode.get("description", ""))
    entry.set_editor_property("max_player_count", int(mode.get("maxPlayers", 16)))
    entry.set_editor_property("show_in_front_end", bool(mode.get("show", True)))
    entry.set_editor_property("is_default_experience", bool(mode.get("default", False)))
    assets.save_loaded_asset(entry, False)
    unreal.log("sync_modes: %s -> %s on %s %s" % (name, experience, mode["map"], mode.get("rules", {})))

for path in assets.list_assets(FOLDER, recursive=False, include_folder=False):
    package = path.split(".")[0]
    if package.split("/")[-1].startswith("DA_DH_") and package not in wanted:
        # delete_asset does not remove the file in a commandlet; delete it directly (nothing references entries).
        unreal.log("sync_modes: removing %s" % package)
        file = os.path.join(ROOT, "Plugins", "GameFeatures", "DowntownCore", "Content", package[len("/DowntownCore/"):] + ".uasset")
        if os.path.exists(file):
            os.remove(file)
