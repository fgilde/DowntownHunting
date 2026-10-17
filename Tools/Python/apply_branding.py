"""Points Lyra's menu card art at Downtown Hunting images (references are redirected, Lyra's textures removed).
Run once as commandlet: UnrealEditor-Cmd.exe DowntownHunting.uproject -run=pythonscript -script=Tools/Python/apply_branding.py"""
import unreal

CARDS = {
    "/Game/UI/Menu/Art/T_UI_TileArt_Host": "/Game/DH/UI/Cards/T_DH_Card_Host",
    "/Game/UI/Menu/Art/T_UI_TileArt_Quickplay": "/Game/DH/UI/Cards/T_DH_Card_Quickplay",
    "/Game/UI/Menu/Art/T_UI_TileArt_Browse": "/Game/DH/UI/Cards/T_DH_Card_Browse",
}
for lyra, ours in CARDS.items():
    if not unreal.EditorAssetLibrary.does_asset_exist(lyra):
        continue
    ok = unreal.EditorAssetLibrary.consolidate_assets(unreal.load_asset(ours), [unreal.load_asset(lyra)])
    unreal.log("apply_branding: %s -> %s %s" % (lyra, ours, ok))
unreal.EditorLoadingAndSavingUtils.save_dirty_packages(False, True)
