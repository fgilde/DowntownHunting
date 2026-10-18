"""Experiences beyond Lyra's two teams, as an Unreal MCP tool script in the running editor (python mcp.py script
mcp_make_experiences.py): instanced experience actions and EditDefaultsOnly maps are not editable from Python.
- B_DH_FFA (free-for-all), a copy of B_DH_TDM3: every player gets a team of their own (16 teams, all red).
- B_DH_TDM3 (three teams), changed in place.
- B_DH_Elimination (rounds without respawn), a copy of B_DH_TDM with AbilitySet_DH_Rounds (Lyra's elimination ability
  set without GA_AutoRespawn) and UDHRoundScoring.
- B_DH_SearchAndDestroy, a copy of B_DH_Elimination with UDHSearchAndDestroyScoring (bomb sites, plant, defuse).
In all of them, Lyra's two-team scoring and score widget are replaced by UDHFreeForAllScoring / UDHTeamScoring and
UDHFreeForAllHud, the music manager (reads the team scoring) is dropped."""
import json

O = 'editor_toolset.toolsets.object.ObjectTools.'
A = 'editor_toolset.toolsets.asset.AssetTools.'
B = 'editor_toolset.toolsets.blueprint.BlueprintTools.'
ROOT = '/DowntownCore/'
TEAMS = ROOT + 'Game/B_DH_TeamSetup_FFA'
EXPERIENCE = ROOT + 'Experiences/B_DH_FFA'
HUD = {'W_ScoreWidget_Elimination': '/Script/DowntownCoreRuntime.DHFreeForAllHud'}
# B_DH_TDM3 may already be rewired (DHTeamScoring) from an earlier run
FFA = dict(HUD, B_TeamDeathMatchScoring='/Script/DowntownCoreRuntime.DHFreeForAllScoring',
           **{'DowntownCoreRuntime.DHTeamScoring': '/Script/DowntownCoreRuntime.DHFreeForAllScoring'},
           B_DH_TeamSetup_ThreeTeams=TEAMS + '.B_DH_TeamSetup_FFA_C')
THREE_TEAMS = dict(HUD, B_TeamDeathMatchScoring='/Script/DowntownCoreRuntime.DHTeamScoring')
ROUND_ABILITIES = ROOT + 'Game/AbilitySet_DH_Rounds'
ELIMINATION = dict(HUD, B_TeamDeathMatchScoring='/Script/DowntownCoreRuntime.DHRoundScoring')
SEARCH_AND_DESTROY = {'DHRoundScoring': '/Script/DowntownCoreRuntime.DHSearchAndDestroyScoring'}
ELIMINATION_ABILITIES = {'/ShooterCore/Elimination/AbilitySet_Elimination.AbilitySet_Elimination':
                         ROUND_ABILITIES + '.AbilitySet_DH_Rounds'}
DROP = ['B_MusicManagerComponent_Elimination']


def cdo(path):
    n = path.split('/')[-1]
    return '%s.Default__%s_C' % (path, n)


def get(path, prop):
    return json.loads(execute_tool(O + 'get_properties', json.dumps({'instance': {'refPath': path}, 'properties': [prop]}))['returnValue'])[prop]


def put(path, values):
    return execute_tool(O + 'set_properties', json.dumps({'instance': {'refPath': path}, 'values': json.dumps(values)}))['returnValue']


def fresh_copy(src, dst):
    if execute_tool(A + 'exists', json.dumps({'path': dst}))['returnValue']:
        execute_tool(A + 'delete', json.dumps({'path': dst}))
    execute_tool(A + 'duplicate', json.dumps({'path': src, 'new_path': dst}))


def swap(entries, key, replace):
    """Returns (entries without the dropped ones, same with the classes replaced): MCP removes and edits in separate calls."""
    kept = [e for e in entries if not any(d in json.dumps(e[key]) for d in DROP)]
    swapped = json.loads(json.dumps(kept))
    for e in swapped:
        for old, new in replace.items():
            if old in json.dumps(e[key]):
                e[key] = new
    return kept, swapped


def put_list(path, prop, replace):
    kept, swapped = swap(get(path, prop), 'componentClass' if prop == 'ComponentList' else 'widgetClass', replace)
    put(path, {prop: kept})
    return put(path, {prop: swapped})


def rewire(experience, replace, abilities=None):
    r = {}
    for action in get(cdo(experience), 'Actions'):
        path = action['refPath']
        if 'AddComponents' in path:
            r['components'] = put_list(path, 'ComponentList', replace)
        elif 'AddWidgets' in path:
            r['widgets'] = put_list(path, 'Widgets', replace)
        elif 'AddAbilities' in path and abilities:
            text = json.dumps(get(path, 'AbilitiesList'))
            for old, new in abilities.items():
                text = text.replace(old, new)
            r['abilities'] = put(path, {'AbilitiesList': json.loads(text)})
    return r


def run():
    r = {}
    fresh_copy(ROOT + 'Game/B_DH_TeamSetup_ThreeTeams', TEAMS)
    r['teams'] = put(cdo(TEAMS), {'TeamsToCreate': {str(i): {'refPath': '/Game/System/Teams/TeamDA_Red.TeamDA_Red'} for i in range(1, 17)}})
    fresh_copy(ROOT + 'Experiences/B_DH_TDM3', EXPERIENCE)
    r['ffa'] = rewire(EXPERIENCE, FFA)
    three = ROOT + 'Experiences/B_DH_TDM3'
    r['tdm3'] = rewire(three, THREE_TEAMS)
    fresh_copy('/ShooterCore/Elimination/AbilitySet_Elimination', ROUND_ABILITIES)
    asset_ref = ROUND_ABILITIES + '.AbilitySet_DH_Rounds'
    granted = get(asset_ref, 'GrantedGameplayAbilities')
    r['no_respawn'] = put(asset_ref, {'GrantedGameplayAbilities': [g for g in granted if 'GA_AutoRespawn' not in json.dumps(g)]})
    elimination = ROOT + 'Experiences/B_DH_Elimination'
    fresh_copy(ROOT + 'Experiences/B_DH_TDM', elimination)
    r['elimination'] = rewire(elimination, ELIMINATION, ELIMINATION_ABILITIES)
    snd = ROOT + 'Experiences/B_DH_SearchAndDestroy'
    fresh_copy(elimination, snd)
    r['snd'] = rewire(snd, SEARCH_AND_DESTROY)
    for bp in (TEAMS, EXPERIENCE, three, elimination, snd):
        execute_tool(B + 'compile_blueprint', json.dumps({'blueprint': {'refPath': '%s.%s' % (bp, bp.split('/')[-1])}, 'warnings_as_errors': False}))
    r['save'] = execute_tool(A + 'save_assets', json.dumps({'asset_paths': [TEAMS, EXPERIENCE, three, elimination, snd, ROUND_ABILITIES]}))['returnValue']
    r['check'] = [json.dumps(get(a['refPath'], 'ComponentList' if 'AddComponents' in a['refPath'] else 'Widgets'))[:700]
                  for e in (EXPERIENCE, three, elimination, snd) for a in get(cdo(e), 'Actions') if 'AddComponents' in a['refPath'] or 'AddWidgets' in a['refPath']]
    return r
