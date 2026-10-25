"""Weapon and ability sounds (MCP tool script, run with roof_map.Editor().script(open(...).read())).
- AK47, MP5, Sniper get their own fire cue: a copy of Lyra's rifle cue (muzzle flash, tracers, impacts, early
  reflections, whiz-bys) with another sound set, under a new tag their fire ability uses (Gameplay Cue TagFiring).
  AK47: Lyra's unused outdoor rifle set (with a far layer), MP5: the pistol set (9 mm), Sniper: the outdoor rifle set
  plus the heavy report of UDHSoundCue.
- GCN_DH_Sounds: the Blueprint child of UDHSoundCue for GameplayCue.DH (abilities, sniper report)."""
import json

O = 'editor_toolset.toolsets.object.ObjectTools.'
A = 'editor_toolset.toolsets.asset.AssetTools.'
B = 'editor_toolset.toolsets.blueprint.BlueprintTools.'
FOLDER = '/DowntownCore/GameplayCues'  # registered at runtime by UDHGameRulesComponent (DowntownCore has no cue path action)
SOURCE = '/ShooterCore/Weapons/Rifle/GCN_Weapon_Rifle_Fire'
SOUNDS = {
    'AK47': '/Game/Audio/Sounds/Weapons/Rifle/MSS_Weapons_Rifle_Fire.MSS_Weapons_Rifle_Fire',
    'MP5': '/Game/Audio/Sounds/Weapons/Pistol/MSS_Weapons_Pistol_Fire.MSS_Weapons_Pistol_Fire',
    'Sniper': '/Game/Audio/Sounds/Weapons/Rifle/MSS_Weapons_Rifle_Fire.MSS_Weapons_Rifle_Fire',
}


def tool(name, **args):
    return execute_tool(name, json.dumps(args))['returnValue']


def obj(path):
    return {'refPath': path + '.' + path.split('/')[-1]}


def cdo(path):
    n = path.split('/')[-1]
    return {'refPath': '%s.Default__%s_C' % (path, n)}


def put(instance, values):
    return tool(O + 'set_properties', instance=instance, values=json.dumps(values))


def run():
    saved = []
    for weapon, sound in SOUNDS.items():
        cue = '%s/GCN_Weapon_%s_Fire' % (FOLDER, weapon)
        tag = 'GameplayCue.Weapon.%s.Fire' % weapon
        if not tool(A + 'exists', path=cue):
            tool(A + 'duplicate', path=SOURCE, new_path=cue)
        graph = {'refPath': obj(cue)['refPath'] + ':OnBurst'}
        for info in tool(B + 'get_node_infos', nodes=tool(B + 'find_nodes', graph=graph, title='')):
            if info['type_id'].endswith('TriggerFireAudio'):
                for pin in info['input_pins']:
                    if pin['name'] == 'Sound':
                        tool(B + 'set_pin_value', pin=pin['pin_id'], value=sound)
        tool(B + 'compile_blueprint', blueprint=obj(cue))
        put(cdo(cue), {'GameplayCueTag': {'tagName': tag}})
        ability = '/DowntownCore/Weapons/%s/GA_Weapon_Fire_%s_Auto' % (weapon, weapon)
        put(cdo(ability), {'Gameplay Cue TagFiring': {'tagName': tag}})
        saved += [cue, ability]

    sounds = FOLDER + '/GCN_DH_Sounds'
    if not tool(A + 'exists', path=sounds):
        tool(B + 'create', folder_path=FOLDER, asset_name='GCN_DH_Sounds', asset_type={'refPath': '/Script/DowntownCoreRuntime.DHSoundCue'})
    tool(B + 'compile_blueprint', blueprint=obj(sounds))
    put(cdo(sounds), {'GameplayCueTag': {'tagName': 'GameplayCue.DH'}})
    saved.append(sounds)
    tool(A + 'save_assets', asset_paths=saved)
    return {cue: tool(O + 'get_properties', instance=cdo(cue), properties=['GameplayCueTag']) for cue in saved if 'GCN' in cue}
