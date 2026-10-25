"""DowntownCore's input: creates IA_DH_Sprint and the killstreak actions and writes the mappings of IMC_DH_Perspective (priority 10, above Lyra's and
ShooterCore's contexts, so its keys win and are consumed there). Gamepad defaults of the controller screen come from here.
Discrete actions get a Pressed trigger: without one Lyra fires an ability input every frame while the button is held
(Y switched the weapon back and forth).
Editor closed: UnrealEditor-Cmd.exe DowntownHunting.uproject -run=pythonscript -script="<repo>/Tools/Python/setup_input.py" -nullrhi"""
import unreal as u

FOLDER = '/DowntownCore/Input'
tools = u.AssetToolsHelpers.get_asset_tools()


def action(path):
    asset = u.load_asset(path)
    if not asset:
        raise RuntimeError('missing ' + path)
    return asset


def bool_action(name):
    asset = u.load_asset(FOLDER + '/' + name)
    if not asset:
        asset = tools.create_asset(name, FOLDER, u.InputAction, u.InputAction_Factory())
        asset.set_editor_property('value_type', u.InputActionValueType.BOOLEAN)
        u.EditorAssetLibrary.save_loaded_asset(asset, False)
    return asset


sprint = bool_action('IA_DH_Sprint')
# Killstreaks (UDHStreakHud): one key each on the keyboard, call in / select on the D-pad.
streaks = [bool_action('IA_DH_Streak%d' % n) for n in (1, 2, 3)]
streak_use = bool_action('IA_DH_StreakUse')
streak_next = bool_action('IA_DH_StreakNext')

toggle = action(FOLDER + '/IA_DH_TogglePerspective')
ability2 = action(FOLDER + '/IA_DH_Ability2')
ability1 = action('/Game/Input/Actions/IA_Ability_Dash')
melee = action('/ShooterCore/Input/Actions/IA_Melee')
cycle = action('/ShooterCore/Input/Actions/IA_QuickSlot_CycleForward')
grenade = action('/ShooterCore/Input/Actions/IA_Grenade')

# (action, key, pressed trigger)
MAPPINGS = [
    (toggle, 'V', False), (toggle, 'Gamepad_DPad_Up', False),
    (sprint, 'LeftShift', False), (sprint, 'Gamepad_LeftThumbstick', False),
    (ability1, 'E', True), (ability1, 'Gamepad_LeftShoulder', True),
    (ability2, 'F', False), (ability2, 'Gamepad_DPad_Right', False),
    (grenade, 'Gamepad_RightShoulder', True),
    (melee, 'Gamepad_RightThumbstick', True),
    (cycle, 'Gamepad_FaceButton_Top', True),
    (streaks[0], 'Four', True), (streaks[1], 'Five', True), (streaks[2], 'Six', True),
    (streak_use, 'Gamepad_DPad_Left', True), (streak_next, 'Gamepad_DPad_Down', True),
]

imc = u.load_asset(FOLDER + '/IMC_DH_Perspective')
mappings = []
for act, key_name, pressed in MAPPINGS:
    mapping = u.EnhancedActionKeyMapping()
    mapping.set_editor_property('action', act)
    key = u.Key()
    key.set_editor_property('key_name', key_name)
    mapping.set_editor_property('key', key)
    if pressed:
        mapping.set_editor_property('triggers', [u.new_object(u.InputTriggerPressed, outer=imc)])
    mappings.append(mapping)
data = imc.get_editor_property('default_key_mappings')
data.set_editor_property('mappings', mappings)
imc.set_editor_property('default_key_mappings', data)
u.EditorAssetLibrary.save_loaded_asset(imc, False)

for m in imc.get_editor_property('default_key_mappings').get_editor_property('mappings'):
    print('DH input: %s -> %s %s' % (m.get_editor_property('key').get_editor_property('key_name'), m.get_editor_property('action').get_name(),
                                     [t.get_class().get_name() for t in m.get_editor_property('triggers')]))
# The credits text was patched in the package file; make sure it still loads.
print('DH credits loads: %s' % bool(u.load_asset('/Game/UI/Credits/W_Credits')))
