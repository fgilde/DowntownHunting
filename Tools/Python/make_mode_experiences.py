"""Team modes on top of B_DH_TDM: copies with one of our scoring components and UDHFreeForAllHud in place of Lyra's
team deathmatch scoring and score widget. B_DH_Hill (King of the Hill), B_DH_Infected, B_DH_CTF (Capture the Flag).
Runs locally with the editor open (Unreal MCP): python Tools/Python/make_mode_experiences.py
Uses the helpers of mcp_make_experiences.py (sent along to the editor; its own run() is left out)."""
import os

from roof_map import Editor

HERE = os.path.dirname(os.path.abspath(__file__))
MODES = {'B_DH_Hill': 'DHHillScoring', 'B_DH_Infected': 'DHInfectedScoring', 'B_DH_CTF': 'DHFlagScoring'}

RUN = '''
def run():
    r = {}
    for name, scoring in MODES.items():
        path = ROOT + 'Experiences/' + name
        fresh_copy(ROOT + 'Experiences/B_DH_TDM', path)
        r[name] = rewire(path, dict(HUD, B_TeamDeathMatchScoring='/Script/DowntownCoreRuntime.' + scoring))
        execute_tool(B + 'compile_blueprint', json.dumps({'blueprint': {'refPath': path + '.' + name}, 'warnings_as_errors': False}))
        r[name + '_check'] = [json.dumps(get(a['refPath'], 'ComponentList'))[:200] for a in get(cdo(path), 'Actions') if 'AddComponents' in a['refPath']]
    r['save'] = execute_tool(A + 'save_assets', json.dumps({'asset_paths': [ROOT + 'Experiences/' + n for n in MODES]}))['returnValue']
    return r
'''

if __name__ == '__main__':
    with open(os.path.join(HERE, 'mcp_make_experiences.py'), encoding='utf-8') as f:
        helpers = f.read().split('\ndef run():')[0]
    print(Editor().script(helpers + RUN, MODES=MODES))
