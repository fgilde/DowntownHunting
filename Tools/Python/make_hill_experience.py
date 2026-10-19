"""B_DH_Hill (King of the Hill): a copy of B_DH_TDM with UDHHillScoring and UDHFreeForAllHud in place of Lyra's team
deathmatch scoring and score widget. Runs locally with the editor open (Unreal MCP): python Tools/Python/make_hill_experience.py
Uses the helpers of mcp_make_experiences.py (sent along to the editor; its own run() is left out)."""
import os

from roof_map import Editor

HERE = os.path.dirname(os.path.abspath(__file__))

RUN = '''
def run():
    hill = ROOT + 'Experiences/B_DH_Hill'
    fresh_copy(ROOT + 'Experiences/B_DH_TDM', hill)
    r = {'rewire': rewire(hill, dict(HUD, B_TeamDeathMatchScoring='/Script/DowntownCoreRuntime.DHHillScoring'))}
    execute_tool(B + 'compile_blueprint', json.dumps({'blueprint': {'refPath': hill + '.B_DH_Hill'}, 'warnings_as_errors': False}))
    r['save'] = execute_tool(A + 'save_assets', json.dumps({'asset_paths': [hill]}))['returnValue']
    r['check'] = [json.dumps(get(a['refPath'], 'ComponentList' if 'AddComponents' in a['refPath'] else 'Widgets'))[:600]
                  for a in get(cdo(hill), 'Actions') if 'AddComponents' in a['refPath'] or 'AddWidgets' in a['refPath']]
    return r
'''

if __name__ == '__main__':
    with open(os.path.join(HERE, 'mcp_make_experiences.py'), encoding='utf-8') as f:
        helpers = f.read().split('\ndef run():')[0]
    print(Editor().script(helpers + RUN))
