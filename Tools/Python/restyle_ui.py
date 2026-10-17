"""Downtown look for Lyra's UI: recolors the material instances of menus and HUD from Lyra's cyan/blue to amber/orange
on warm graphite. Hue is replaced, brightness, saturation and alpha are kept; team colors (red/blue) stay.
Editor closed: UnrealEditor-Cmd.exe DowntownHunting.uproject -run=pythonscript -script="<repo>/Tools/Python/restyle_ui.py" -nullrhi
Running it twice is harmless: colors that are already warm are left alone."""
import unreal

FOLDERS = ["/Game/UI/Menu/Art", "/Game/UI/Hud/Art", "/Game/UI/Foundation"]
KEEP = ("Teams_", "Scoreboard", "ScoreBoard", "Nameplate", "_Red", "_Blue", "RedTeam", "BlueTeam", "DebugOnly", "ElimFeed")
AMBER = (1.0, 0.42, 0.08)
MEL = unreal.MaterialEditingLibrary


def warm(c):
    r, g, b, a = c.r, c.g, c.b, c.a
    hi, lo = max(r, g, b), min(r, g, b)
    if b <= r or hi <= 0.0:
        return None  # already warm or neutral black
    if hi < 0.06:  # dark navy panels -> warm graphite of the same darkness
        return unreal.LinearColor(hi, hi * 0.9, hi * 0.78, a)
    s = (hi - lo) / hi
    return unreal.LinearColor(*(hi * ((1 - s) + s * k) for k in AMBER), a)


assert warm(unreal.LinearColor(0, 0.283, 1, 0.7)).g < 0.5  # saturated blue -> amber
assert warm(unreal.LinearColor(1, 0.4, 0.1, 1)) is None    # warm stays

registry = unreal.AssetRegistryHelpers.get_asset_registry()
changed = 0
for folder in FOLDERS:
    for data in registry.get_assets_by_path(folder, True):
        if str(data.asset_class_path.asset_name) != "MaterialInstanceConstant" or any(k in str(data.asset_name) for k in KEEP):
            continue
        mi = data.get_asset()
        dirty = False
        for name in MEL.get_vector_parameter_names(mi):
            new = warm(MEL.get_material_instance_vector_parameter_value(mi, name))
            if new:
                MEL.set_material_instance_vector_parameter_value(mi, name, new)
                dirty = True
        if dirty:
            MEL.update_material_instance(mi)
            unreal.EditorAssetLibrary.save_loaded_asset(mi)
            changed += 1
unreal.log("RESTYLE_UI recolored %d material instances" % changed)
