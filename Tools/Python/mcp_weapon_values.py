"""Extra weapons, step 2 of 2 (after make_weapons.py): wires the duplicated chain and sets the stats.
Runs as an Unreal MCP tool script in the editor (python mcp.py script mcp_weapon_values.py), because these properties
are not editable from Python (EditDefaultsOnly, gameplay effect magnitudes). Muzzle sockets: make_weapons.py."""
import json

O = 'editor_toolset.toolsets.object.ObjectTools.'
A = 'editor_toolset.toolsets.asset.AssetTools.'
B = 'editor_toolset.toolsets.blueprint.BlueprintTools.'
T = 'editor_toolset.toolsets.texture.TextureTools.'
ROOT = '/DowntownCore/Weapons/'
ICONS = '/Game/DH/UI/WeaponIcons/'  # HUD/loadout icons from Tools/Branding/make_weapon_icons.py
ICON_SOURCE = 'C:/dev/privat/github/DowntownHunting/Tools/Branding/Weapons/'
# name: mesh, root bone, muzzle (mesh space), damage (None = 1.25 x base), seconds between shots (None = keep), magazine, spare, display
WEAPONS = {
    'AK47': ('/Game/Weapons/Classic/AK47/SKM_AK47', 'WPN_AK47_CLASSIC', (0, 71, 3.5), 15.0, 0.10, 30, 90, 'AK-47'),
    'MP5': ('/Game/Weapons/Classic/MP5/SKM_MP5', 'WPN_MP5', (0, 41, 6.5), 9.0, 0.075, 30, 120, 'MP5'),
    'M1911': ('/Game/Weapons/Classic/1911/SKM_1911', 'WPN_1911_CLASSIC', (0, 17.5, 4.5), None, None, 7, 35, 'M1911'),
    # bolt action: one heavy shot (a headshot kills, two body hits), slow follow-up
    'Sniper': ('/Game/Weapons/Classic/M700/SKM_M700', 'WPN_308SNIPER', (0, 73.7, 3.06), 80.0, 1.25, 5, 20, 'M700'),
}
def curve(*points):
    return {'editorCurveData': {'keys': [{'interpMode': 'RCIM_Linear', 'time': t, 'value': v} for t, v in points]}, 'externalCurve': 'None'}


# Weapon instance overrides (spread, range): the sniper hits the same at any distance, is wild from the hip and
# nearly exact when aiming.
INSTANCE = {
    # Same key counts as the rifle's curves (MCP cannot resize and edit an array in one call).
    'Sniper': {'DistanceDamageFalloff': curve((0, 1.0), (30000, 1.0), (60000, 1.0)), 'MaxDamageRange': 60000.0, 'SpreadExponent': 2.0,
               'HeatToSpreadCurve': curve((0, 2.0), (4, 3.5), (8, 5.0), (12, 6.0)), 'SpreadAngleMultiplier_Aiming': 0.03},
}
# Only these (damage None scales the current value: running a weapon twice would scale it twice). Run e.g. with
# Tools/Python/roof_map.Editor().script(text, ONLY=['Sniper']); without ONLY all weapons.


def get(path, prop):
    return json.loads(execute_tool(O + 'get_properties', json.dumps({'instance': {'refPath': path}, 'properties': [prop]}))['returnValue'])[prop]


def put(path, values):
    return execute_tool(O + 'set_properties', json.dumps({'instance': {'refPath': path}, 'values': json.dumps(values)}))['returnValue']


def asset(name, prefix):
    found = [a for a in execute_tool(A + 'find_assets', json.dumps({'folder_path': ROOT + name, 'name': prefix, 'recursive': False}))['returnValue']
             if a.split('/')[-1].startswith(prefix)]
    return found[0]


def cdo(path):
    n = path.split('/')[-1]
    return '%s.Default__%s_C' % (path, n)


def cls(path):
    n = path.split('/')[-1]
    return {'refPath': '%s.%s_C' % (path, n)}


def run():
    out = {}
    try:
        names = ONLY
    except NameError:
        names = list(WEAPONS)
    for name in names:
        mesh, bone, muzzle, damage, delay, magazine, spare, display = WEAPONS[name]
        r = {}

        ge, ga = asset(name, 'GE_Damage_'), asset(name, 'GA_Weapon_Fire_')
        execs = get(cdo(ge), 'Executions')
        mag = execs[0]['calculationModifiers'][0]['modifierMagnitude']['scalableFloatMagnitude']
        r['base_damage'] = mag['value']
        mag['value'] = damage if damage is not None else round(mag['value'] * 1.25, 1)
        r['damage'] = put(cdo(ge), {'Executions': execs})
        ga_values = {'GE_Damage': cls(ge)}
        if delay is not None:
            ga_values['FireDelayTimeSecs'] = delay
        r['ga'] = put(cdo(ga), ga_values)

        abilities = asset(name, 'AbilitySet_')
        granted = get('%s.%s' % (abilities, abilities.split('/')[-1]), 'GrantedGameplayAbilities')
        for entry in granted:
            if 'GA_Weapon_Fire' in json.dumps(entry['ability']):
                entry['ability'] = cls(ga)
        r['abilities'] = put('%s.%s' % (abilities, abilities.split('/')[-1]), {'GrantedGameplayAbilities': granted})

        wid, instance, actor = asset(name, 'WID_'), asset(name, 'B_WeaponInstance_'), ROOT + name + '/B_' + name
        if name in INSTANCE:
            r['instance'] = put(cdo(instance), INSTANCE[name])
        spawns = get(cdo(wid), 'ActorsToSpawn')
        for s in spawns:
            s['actorToSpawn'] = cls(actor)
        r['wid'] = put(cdo(wid), {'InstanceType': cls(instance), 'ActorsToSpawn': spawns,
                                  'AbilitySetsToGrant': [{'refPath': '%s.%s' % (abilities, abilities.split('/')[-1])}]})

        icon = 'T_UI_Icon_RangedWeapons_' + name
        texture = execute_tool(T + 'import_file', json.dumps({'folder_path': ICONS, 'asset_name': icon, 'source_file': ICON_SOURCE + icon + '.png'}))['returnValue'][0]
        put(texture['refPath'], {'LODGroup': 'TEXTUREGROUP_UI', 'MipGenSettings': 'TMGS_NoMipmaps'})

        item = asset(name, 'ID_')
        put(cdo(item), {'DisplayName': display})
        for fragment in get(cdo(item), 'Fragments'):
            path = fragment['refPath']
            if 'EquippableItem' in path:
                put(path, {'EquipmentDefinition': cls(wid)})
            elif 'SetStats' in path:
                stats = get(path, 'InitialItemStats')
                put(path, {'InitialItemStats': {k: (spare if 'SpareAmmo' in k else magazine) for k in stats}})
            elif 'QuickBarIcon' in path:
                brush = get(path, 'Brush')
                brush['resourceObject'] = {'refPath': '%s%s.%s' % (ICONS, icon, icon)}
                put(path, {'Brush': brush})
            elif 'PickupIcon' in path:
                put(path, {'SkeletalMesh': {'refPath': '%s.%s' % (mesh, mesh.split('/')[-1])}, 'DisplayName': display})

        for bp in (ge, ga, instance, wid, item):
            execute_tool(B + 'compile_blueprint', json.dumps({'blueprint': {'refPath': '%s.%s' % (bp, bp.split('/')[-1])}, 'warnings_as_errors': False}))
        r['save'] = execute_tool(A + 'save_assets', json.dumps({'asset_paths': [ge, ga, abilities, instance, wid, item, mesh, ICONS + icon]}))['returnValue']
        out[name] = r
    return out
