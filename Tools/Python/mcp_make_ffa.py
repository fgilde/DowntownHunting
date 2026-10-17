"""Free-for-all experience B_DH_FFA from the three-team one (B_DH_TDM3), as an Unreal MCP tool script in the running
editor (python mcp.py script mcp_make_ffa.py): instanced experience actions and EditDefaultsOnly maps are not editable
from Python. Every player gets a team of their own (16 teams, all red), Lyra's two-team scoring and score widget are
replaced by UDHFreeForAllScoring and UDHFreeForAllHud, the music manager (reads the team scoring) is dropped."""
import json

O = 'editor_toolset.toolsets.object.ObjectTools.'
A = 'editor_toolset.toolsets.asset.AssetTools.'
B = 'editor_toolset.toolsets.blueprint.BlueprintTools.'
ROOT = '/DowntownCore/'
TEAMS = ROOT + 'Game/B_DH_TeamSetup_FFA'
EXPERIENCE = ROOT + 'Experiences/B_DH_FFA'
REPLACE = {
    'B_TeamDeathMatchScoring': '/Script/DowntownCoreRuntime.DHFreeForAllScoring',
    'B_DH_TeamSetup_ThreeTeams': TEAMS + '.B_DH_TeamSetup_FFA_C',
    'W_ScoreWidget_Elimination': '/Script/DowntownCoreRuntime.DHFreeForAllHud',
}
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


def swap(entries, key):
    """Returns (entries without the dropped ones, same with the classes replaced): MCP removes and edits in separate calls."""
    kept = [e for e in entries if not any(d in json.dumps(e[key]) for d in DROP)]
    swapped = json.loads(json.dumps(kept))
    for e in swapped:
        for old, new in REPLACE.items():
            if old in json.dumps(e[key]):
                e[key] = new
    return kept, swapped


def put_list(path, prop):
    kept, swapped = swap(get(path, prop), 'componentClass' if prop == 'ComponentList' else 'widgetClass')
    put(path, {prop: kept})
    return put(path, {prop: swapped})


def run():
    r = {}
    fresh_copy(ROOT + 'Game/B_DH_TeamSetup_ThreeTeams', TEAMS)
    r['teams'] = put(cdo(TEAMS), {'TeamsToCreate': {str(i): {'refPath': '/Game/System/Teams/TeamDA_Red.TeamDA_Red'} for i in range(1, 17)}})

    fresh_copy(ROOT + 'Experiences/B_DH_TDM3', EXPERIENCE)
    for action in get(cdo(EXPERIENCE), 'Actions'):
        path = action['refPath']
        if 'AddComponents' in path:
            r['components'] = put_list(path, 'ComponentList')
        elif 'AddWidgets' in path:
            r['widgets'] = put_list(path, 'Widgets')
    for bp in (TEAMS, EXPERIENCE):
        execute_tool(B + 'compile_blueprint', json.dumps({'blueprint': {'refPath': '%s.%s' % (bp, bp.split('/')[-1])}, 'warnings_as_errors': False}))
    r['save'] = execute_tool(A + 'save_assets', json.dumps({'asset_paths': [TEAMS, EXPERIENCE]}))['returnValue']
    r['check'] = [json.dumps(get(a['refPath'], 'ComponentList' if 'AddComponents' in a['refPath'] else 'Widgets'))[:900]
                  for a in get(cdo(EXPERIENCE), 'Actions') if 'AddComponents' in a['refPath'] or 'AddWidgets' in a['refPath']]
    return r
