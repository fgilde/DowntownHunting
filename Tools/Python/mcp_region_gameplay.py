"""Gameplay actors for a City Sample region map (after Tools/Build-RegionMap.ps1), as an Unreal MCP tool script in the
running editor: python mcp.py script mcp_region_gameplay.py  (edit MAP / CENTRE / HALF below first).
Traces the ground on a 5 m grid; points near the lowest common height are streets (elevated freeway decks and roofs are
higher and left out). Puts 16 player starts spread over the streets, 3 Lyra control points near the centre (Domination,
Search & Destroy uses the outer two), a navmesh bounds volume, blocking walls at the edge and a sky atmosphere.
The navmesh is built afterwards with the WorldPartitionNavigationDataBuilder (see docs/NOTES.md)."""
import json
import math

MAP = '/Game/DH/Maps/DH_Freeway'
CENTRE = (12500, -12500)  # cm, as given to cs_region.py
HALF = 12000              # walls this far from the centre (the play area of cs_region.py is 12500)

T = 'editor_toolset.toolsets.scene.SceneTools.'
A = 'editor_toolset.toolsets.asset.AssetTools.'


def trace(x, y):
    return execute_tool(T + 'trace_world', json.dumps({'start': {'x': x, 'y': y, 'z': 40000}, 'end': {'x': x, 'y': y, 'z': -40000}}))['returnValue']


def add(cls, name, loc, yaw=None, scale=None, asset=False):
    xf = {'location': {'x': loc[0], 'y': loc[1], 'z': loc[2]}}
    if yaw is not None:
        xf['rotation'] = {'pitch': 0, 'yaw': yaw, 'roll': 0}
    if scale:
        xf['scale'] = {'x': scale[0], 'y': scale[1], 'z': scale[2]}
    if asset:
        actor = execute_tool(T + 'add_to_scene_from_asset', json.dumps({'asset_path': cls, 'name': name, 'xform': xf}))['returnValue']
    else:
        actor = execute_tool(T + 'add_to_scene_from_class', json.dumps({'actor_type': {'refPath': cls}, 'name': name, 'xform': xf}))['returnValue']
    execute_tool(T + 'set_actor_folder', json.dumps({'actor': actor, 'folder_path': 'DH_Gameplay'}))
    return actor


def spread(points, count, first):
    """Farthest point sampling: count points as far apart as possible."""
    pick = [first]
    while len(pick) < count:
        pick.append(max(points, key=lambda p: min((p[0] - q[0]) ** 2 + (p[1] - q[1]) ** 2 for q in pick)))
    return pick


def run():
    execute_tool(T + 'load_level', json.dumps({'level_path': MAP}))
    cx, cy = CENTRE
    steps = int(HALF / 500) - 1
    ground = []
    for ix in range(-steps, steps + 1):
        for iy in range(-steps, steps + 1):
            x, y = cx + ix * 500, cy + iy * 500
            hit = trace(x, y)
            if hit is not None:
                ground.append((x, y, 40000 - hit))
    heights = sorted(p[2] for p in ground)
    base = heights[len(heights) // 4]
    street = [p for p in ground if p[2] - base < 250]
    centre = min(street, key=lambda p: (p[0] - cx) ** 2 + (p[1] - cy) ** 2)

    for index, p in enumerate(spread(street, 16, centre)):
        yaw = math.degrees(math.atan2(cy - p[1], cx - p[0]))  # facing the centre
        add('/Script/LyraGame.LyraPlayerStart', 'DH_PlayerStart_%02d' % index, (p[0], p[1], p[2] + 100), yaw)
    inner = [p for p in street if abs(p[0] - cx) < HALF * 0.66 and abs(p[1] - cy) < HALF * 0.66]
    points = spread(inner, 3, centre)
    points = [points[1], points[0], points[2]]  # A and C far apart, B in the middle
    for index, p in enumerate(points):
        add('/ShooterCore/Blueprint/B_ControlPointVolume', 'DH_ControlPoint_' + 'ABC'[index], p, asset=True)
    add('/Script/NavigationSystem.NavMeshBoundsVolume', 'DH_NavBounds', (cx, cy, base + 2500), None, (HALF / 96, HALF / 96, 30))
    walls = {'N': (cx, cy + HALF, HALF / 100 + 1, 1), 'S': (cx, cy - HALF, HALF / 100 + 1, 1),
             'E': (cx + HALF, cy, 1, HALF / 100 + 1), 'W': (cx - HALF, cy, 1, HALF / 100 + 1)}
    for side, (x, y, sx, sy) in walls.items():
        add('/Script/Engine.BlockingVolume', 'DH_Bounds_' + side, (x, y, base + 5000), None, (sx, sy, 60))
    add('/Script/Engine.SkyAtmosphere', 'DH_SkyAtmosphere', (cx, cy, 0))
    result = {'ground': len(ground), 'street': len(street), 'base': base, 'zmin': heights[0], 'zmax': heights[-1],
              'points': [[round(v) for v in p] for p in points]}
    result['save'] = execute_tool(A + 'save_assets', json.dumps({'asset_paths': []}))['returnValue']
    return result
