"""Gameplay for the rooftop map Rooftops (DH_Roofs, a City Sample region after Tools/Build-RegionMap.ps1).
Runs locally with the editor open (Unreal MCP on port 8765): python Tools/Python/roof_map.py
The editor only traces and places actors (in batches, the MCP script sandbox is slow); the rest is computed here.
- Traces the region on a 4 m grid and groups roof points (12 m and more above the street) into flat roofs.
- Neighbouring roofs get planks across the gap between them (with a ramp over the parapet where needed); only roofs
  connected to the largest group are played on.
- 16 player starts and 3 control points on those roofs, navmesh bounds, walls at the edge, KillZ a few metres above
  the street: whoever falls off dies."""
import json
import math
import urllib.request

MAP = '/Game/DH/Maps/DH_Roofs'
CENTRE = (-34200, 18100)  # cm, as given to cs_region.py
HALF = 7500               # play area half size (cs_region.py cut 8000)
STEP = 400                # grid, cm
ROOF = 1200               # roofs start this high above the street
FLAT = 60                 # height step within one roof
MIN_CELLS = 10            # smaller roofs are props (water tanks, stair houses)
MAX_GAP = 2400            # longest plank
EXTRA_LINKS = 4           # links beyond a spanning tree, for loops
PLANK_WIDTH, PLANK_THICK = 160, 16

URL = 'http://127.0.0.1:8765/mcp'
T = 'editor_toolset.toolsets.scene.SceneTools.'
A = 'editor_toolset.toolsets.asset.AssetTools.'
O = 'editor_toolset.toolsets.object.ObjectTools.'


class Editor:
    def __init__(self):
        self.session = None
        self.post({'jsonrpc': '2.0', 'id': 1, 'method': 'initialize',
                   'params': {'protocolVersion': '2025-03-26', 'capabilities': {}, 'clientInfo': {'name': 'roof_map', 'version': '1'}}})
        self.post({'jsonrpc': '2.0', 'method': 'notifications/initialized'})

    def post(self, body):
        headers = {'Content-Type': 'application/json', 'Accept': 'application/json, text/event-stream'}
        if self.session:
            headers['Mcp-Session-Id'] = self.session
        with urllib.request.urlopen(urllib.request.Request(URL, json.dumps(body).encode(), headers), timeout=1800) as r:
            text = r.read().decode('utf-8', 'replace')
            self.session = self.session or r.headers.get('Mcp-Session-Id')
        for line in text.splitlines():
            if line.startswith('data:'):
                text = line[5:]
        return json.loads(text) if text.strip() else None

    def script(self, body, **data):
        """Runs def run() of body in the editor with data as JSON globals; returns its result."""
        code = 'import json\nimport math\n' + ''.join('%s = json.loads(%r)\n' % (k, json.dumps(v)) for k, v in data.items())
        code += body.replace('def run():', 'def body():') + "\ndef run():\n    return {'value': body()}\n"  # run() must return a dict
        res = self.post({'jsonrpc': '2.0', 'id': 2, 'method': 'tools/call', 'params': {'name': 'call_tool', 'arguments': {
            'toolset_name': 'editor_toolset.toolsets.programmatic.ProgrammaticToolset', 'tool_name': 'execute_tool_script',
            'arguments': {'script': code}}}})
        text = res['result']['content'][0]['text']
        if res['result'].get('isError'):
            raise RuntimeError(text)
        return json.loads(json.loads(text)['returnValue'])['value']

    def heights(self, points):
        """Ground height (highest hit) at each (x, y), None where nothing is hit."""
        body = '''
def run():
    out = []
    for x, y in POINTS:
        hit = execute_tool('%strace_world', json.dumps({'start': {'x': x, 'y': y, 'z': 40000}, 'end': {'x': x, 'y': y, 'z': -40000}}))['returnValue']
        out.append(None if hit is None else 40000 - hit)
    return out
''' % T
        out = []
        for i in range(0, len(points), 1000):
            out += self.script(body, POINTS=[list(p) for p in points[i:i + 1000]])
        return out


ADD = '''
def run():
    names = []
    for cls, name, loc, rot, scale, asset in ACTORS:
        xf = {'location': {'x': loc[0], 'y': loc[1], 'z': loc[2]}}
        if rot is not None:
            xf['rotation'] = {'pitch': rot[0], 'yaw': rot[1], 'roll': 0}
        if scale:
            xf['scale'] = {'x': scale[0], 'y': scale[1], 'z': scale[2]}
        if asset:
            actor = execute_tool(T + 'add_to_scene_from_asset', json.dumps({'asset_path': cls, 'name': name, 'xform': xf}))['returnValue']
        else:
            actor = execute_tool(T + 'add_to_scene_from_class', json.dumps({'actor_type': {'refPath': cls}, 'name': name, 'xform': xf}))['returnValue']
        execute_tool(T + 'set_actor_folder', json.dumps({'actor': actor, 'folder_path': 'DH_Gameplay'}))
        names.append(name)
    for settings in execute_tool(T + 'find_actors', json.dumps({'name': '', 'tag': '', 'collision_channels': [],
                                                               'actor_type': {'refPath': '/Script/Engine.WorldSettings'}}))['returnValue']:
        execute_tool(O + 'set_properties', json.dumps({'instance': settings, 'values': json.dumps({'KillZ': KILLZ})}))
    return {'added': len(names), 'save': execute_tool(A + 'save_assets', json.dumps({'asset_paths': []}))['returnValue']}
'''


PLANK = '/Game/DH/Meshes/SM_DH_Plank'
MAKE_PLANK = '''
def run():
    # A copy of the engine cube with Depot's painted steel (triplanar: no stretching on long boards), in /Game so
    # the map does not reference the game feature plugin. Actor property edits are not saved through MCP, assets are.
    if not execute_tool(A + 'exists', json.dumps({'path': MATERIAL}))['returnValue']:
        execute_tool(A + 'duplicate', json.dumps({'path': SOURCE, 'new_path': MATERIAL}))
    if not execute_tool(A + 'exists', json.dumps({'path': PLANK}))['returnValue']:
        execute_tool(A + 'duplicate', json.dumps({'path': '/Engine/BasicShapes/Cube', 'new_path': PLANK}))
    mesh = {'refPath': PLANK + '.' + PLANK.split('/')[-1]}
    slots = json.loads(execute_tool(O + 'get_properties', json.dumps({'instance': mesh, 'properties': ['StaticMaterials']}))['returnValue'])['StaticMaterials']
    for slot in slots:
        slot['materialInterface'] = {'refPath': MATERIAL + '.' + MATERIAL.split('/')[-1]}
    execute_tool(O + 'set_properties', json.dumps({'instance': mesh, 'values': json.dumps({'StaticMaterials': slots})}))
    return {'slots': slots, 'save': execute_tool(A + 'save_assets', json.dumps({'asset_paths': [PLANK, MATERIAL]}))['returnValue']}
'''


def make_plank(editor):
    print(editor.script('T = %r\nA = %r\nO = %r\nPLANK = %r\n' % (T, A, O, PLANK) + MAKE_PLANK,
                        SOURCE='/DowntownCore/Maps/Materials/MI_DH_Depot_Steel', MATERIAL='/Game/DH/Materials/MI_DH_Plank_Steel'))


def islands(grid, base):
    """Flat roof areas: grid cells high above the street, joined to neighbours of about the same height."""
    label, groups = {}, []
    for cell, z in grid.items():
        if cell in label or z - base < ROOF:
            continue
        stack, group = [cell], []
        label[cell] = len(groups)
        while stack:
            c = stack.pop()
            group.append(c)
            for n in ((c[0] + 1, c[1]), (c[0] - 1, c[1]), (c[0], c[1] + 1), (c[0], c[1] - 1)):
                if n in grid and n not in label and abs(grid[n] - grid[c]) <= FLAT:
                    label[n] = len(groups)
                    stack.append(n)
        groups.append(group)
    return [g for g in groups if len(g) >= MIN_CELLS]


def bridge(samples, za, zb):
    """Planks along a height profile [(x, y, z)] from roof a to roof b over one gap, or None if that does not work.
    Returns (p, q) segments: the board across and ramps up to parapets."""
    low = min(za, zb) - 300
    gap = [i for i, s in enumerate(samples) if s[2] is None or s[2] < low]
    if not gap or gap[0] == 0 or gap[-1] == len(samples) - 1:
        return None
    first, last = gap[0], gap[-1]
    if any(s[2] is not None and s[2] >= low for s in samples[first:last + 1]):
        return None  # another building in between
    ha = max(s[2] for s in samples[max(0, first - 6):first])
    hb = max(s[2] for s in samples[last + 1:last + 7])
    ea, eb = samples[first - 1], samples[last + 1]
    span = math.hypot(eb[0] - ea[0], eb[1] - ea[1])
    if span > MAX_GAP or abs(hb - ha) > 0.45 * span:
        return None
    segments = [((ea[0], ea[1], ha + 5), (eb[0], eb[1], hb + 5))]
    for side, h, z, edge, inner in ((0, ha, za, first - 1, range(first - 1, -1, -1)), (1, hb, zb, last + 1, range(last + 1, len(samples)))):
        rise = h - z
        if rise <= 40:
            continue
        need = rise / 0.47  # ramp of about 25 degrees back from the edge onto the roof
        start = next((i for i in inner if math.hypot(samples[i][0] - samples[edge][0], samples[i][1] - samples[edge][1]) >= need), None)
        if start is None:
            return None
        lo, hi = sorted((start, edge))
        for i in range(lo, hi + 1):
            frac = math.hypot(samples[i][0] - samples[start][0], samples[i][1] - samples[start][1]) / need
            if samples[i][2] > z + rise * frac + 40:
                return None  # obstacle above the ramp
        foot = (samples[start][0], samples[start][1], z + 2)
        top = (samples[edge][0], samples[edge][1], h + 5)
        segments.append((foot, top) if side == 0 else (top, foot))
    return segments


def plank(name, p, q):
    """Actor spec of a board whose top runs from p to q."""
    dx, dy, dz = q[0] - p[0], q[1] - p[1], q[2] - p[2]
    flat = math.hypot(dx, dy)
    length = math.sqrt(flat * flat + dz * dz)
    pitch = math.degrees(math.atan2(dz, flat))
    mid = ((p[0] + q[0]) / 2, (p[1] + q[1]) / 2, (p[2] + q[2]) / 2 - PLANK_THICK / 2 * math.cos(math.radians(pitch)))
    return [PLANK, name, mid, (pitch, math.degrees(math.atan2(dy, dx))), (length / 100, PLANK_WIDTH / 100, PLANK_THICK / 100), True]


def spread(points, count, first):
    """Farthest point sampling: count points as far apart as possible."""
    pick = [first]
    while len(pick) < count:
        pick.append(max(points, key=lambda p: min((p[0] - q[0]) ** 2 + (p[1] - q[1]) ** 2 for q in pick)))
    return pick


def main():
    editor = Editor()
    editor.script("def run():\n    return execute_tool(%r, json.dumps({'level_path': %r}))['returnValue']\n" % (T + 'load_level', MAP))
    cx, cy = CENTRE
    steps = int(HALF / STEP)
    cells = [(ix, iy) for ix in range(-steps, steps + 1) for iy in range(-steps, steps + 1)]
    pos = lambda c: (cx + c[0] * STEP, cy + c[1] * STEP)
    grid = {c: z for c, z in zip(cells, editor.heights([pos(c) for c in cells])) if z is not None}
    heights = sorted(grid.values())
    base = heights[len(heights) // 10]
    roofs = islands(grid, base)
    print('grid', len(grid), 'base', base, 'roofs', len(roofs))

    # Candidate links: closest cells of each roof pair, shortest first.
    pairs = []
    for i in range(len(roofs)):
        for j in range(i + 1, len(roofs)):
            ca, cb = min(((a, b) for a in roofs[i] for b in roofs[j]), key=lambda p: (p[0][0] - p[1][0]) ** 2 + (p[0][1] - p[1][1]) ** 2)
            d = math.hypot(ca[0] - cb[0], ca[1] - cb[1]) * STEP
            if d <= MAX_GAP + 2 * STEP:
                pairs.append((d, i, j, ca, cb))
    pairs.sort()
    # Height profiles of all candidates in one go, from two cells inside roof a to two cells inside roof b.
    lines = []
    for d, i, j, ca, cb in pairs:
        va, vb = pos(ca), pos(cb)
        ux, uy = (vb[0] - va[0]) / d, (vb[1] - va[1]) / d
        a = (va[0] - ux * 2 * STEP, va[1] - uy * 2 * STEP)
        b = (vb[0] + ux * 2 * STEP, vb[1] + uy * 2 * STEP)
        n = int(math.hypot(b[0] - a[0], b[1] - a[1]) / 50)
        lines.append([(a[0] + (b[0] - a[0]) * k / n, a[1] + (b[1] - a[1]) * k / n) for k in range(n + 1)])
    flat_points = [p for line in lines for p in line]
    zs = iter(editor.heights(flat_points))
    profiles = [[(x, y, next(zs)) for x, y in line] for line in lines]

    parent = list(range(len(roofs)))

    def find(k):
        while parent[k] != k:
            k = parent[k]
        return k

    links, extra = [], 0
    for (d, i, j, ca, cb), profile in zip(pairs, profiles):
        joined = find(i) == find(j)
        if joined and extra >= EXTRA_LINKS:
            continue
        segments = bridge(profile, grid[ca], grid[cb])
        if segments is None:
            continue
        links.append(segments)
        extra += joined
        parent[find(i)] = find(j)

    groups = {}
    for k in range(len(roofs)):
        groups.setdefault(find(k), []).append(k)
    main_roofs = max(groups.values(), key=lambda ks: sum(len(roofs[k]) for k in ks))
    played = {c for k in main_roofs for c in roofs[k]}
    inner = [c for c in played if all(n in played for n in ((c[0] + 1, c[1]), (c[0] - 1, c[1]), (c[0], c[1] + 1), (c[0], c[1] - 1)))]
    points = [(pos(c)[0], pos(c)[1], grid[c]) for c in inner]
    centre = min(points, key=lambda p: (p[0] - cx) ** 2 + (p[1] - cy) ** 2)
    print('links', len(links), 'played roofs', len(main_roofs), 'cells', len(played),
          'heights', sorted(round(sum(grid[c] for c in roofs[k]) / len(roofs[k]) - base) for k in main_roofs))

    actors = []
    for segment in (s for segments in links for s in segments):
        actors.append(plank('DH_Plank_%02d' % len(actors), *segment))
    for index, p in enumerate(spread(points, 16, centre)):
        actors.append(['/Script/LyraGame.LyraPlayerStart', 'DH_PlayerStart_%02d' % index, (p[0], p[1], p[2] + 100),
                       (0, math.degrees(math.atan2(cy - p[1], cx - p[0]))), None, False])
    cps = spread(points, 3, centre)
    for index, p in enumerate([cps[1], cps[0], cps[2]]):
        actors.append(['/ShooterCore/Blueprint/B_ControlPointVolume', 'DH_ControlPoint_' + 'ABC'[index], p, None, None, True])
    top = max(p[2] for p in points)
    actors.append(['/Script/NavigationSystem.NavMeshBoundsVolume', 'DH_NavBounds', (cx, cy, (base + top) / 2 + 500), None,
                   (HALF / 96, HALF / 96, (top - base) / 190 + 10), False])
    for side, (x, y, sx, sy) in {'N': (cx, cy + HALF, HALF / 100 + 1, 1), 'S': (cx, cy - HALF, HALF / 100 + 1, 1),
                                 'E': (cx + HALF, cy, 1, HALF / 100 + 1), 'W': (cx - HALF, cy, 1, HALF / 100 + 1)}.items():
        actors.append(['/Script/Engine.BlockingVolume', 'DH_Bounds_' + side, (x, y, top), None, (sx, sy, 60), False])
    actors.append(['/Script/Engine.SkyAtmosphere', 'DH_SkyAtmosphere', (cx, cy, 0), None, None, False])
    make_plank(editor)
    print(editor.script('T = %r\nA = %r\nO = %r\n' % (T, A, O) + ADD, ACTORS=actors, KILLZ=base + 600))


if __name__ == '__main__':
    main()
