"""Gameplay for the map Gridlock (DH_Gridlock): a traffic jam on the elevated freeway of City Sample.
Runs locally with the editor open (Unreal MCP on port 8765): python Tools/Python/freeway_map.py
Uses the MCP helpers of roof_map.py.
- Traces the region on a 5 m grid; the freeway deck is the longest connected area 5 to 30 m above the street.
- Jammed cars on all lanes (City Sample parked cars that have collision), a jackknifed row of vans across each end.
- 16 player starts and 3 control points on the deck, blocking volumes where ramps lead down, walls at the edge of the
  region, KillZ below the deck: whoever jumps over the barrier dies."""
import math
import random

from roof_map import ADD, A, O, T, Editor, spread

MAP = '/Game/DH/Maps/DH_Gridlock'
CENTRE = (-50000, -12500)  # cm, as given to cs_region.py
HALF = 12000
STEP = 500
DECK_MIN, DECK_MAX = 500, 3000  # deck height above the street
FLAT = 80
CARS = ['/Game/Vehicle/vehCar_vehicle03/Mesh/SM_vehCar_vehicle03', '/Game/Vehicle/vehCar_vehicle05/Mesh/SM_vehCar_vehicle05',
        '/Game/Vehicle/vehVan_vehicle01/Mesh/SM_vehVan_vehicle01']  # the others have no collision
LANE = 370
END = 1500  # no cars this close to the region edge (barrier rows there)


def main():
    random.seed(7)
    editor = Editor()
    editor.script("def run():\n    return execute_tool(%r, json.dumps({'level_path': %r}))['returnValue']\n" % (T + 'load_level', MAP))
    cx, cy = CENTRE
    steps = int(HALF / STEP)
    cells = [(ix, iy) for ix in range(-steps, steps + 1) for iy in range(-steps, steps + 1)]
    pos = lambda c: (cx + c[0] * STEP, cy + c[1] * STEP)
    grid = {c: z for c, z in zip(cells, editor.heights([pos(c) for c in cells])) if z is not None}
    base = sorted(grid.values())[len(grid) // 10]

    # Connected areas in the deck height band; the deck is the longest one.
    label, groups = {}, []
    for cell, z in grid.items():
        if cell in label or not DECK_MIN <= z - base <= DECK_MAX:
            continue
        stack, group = [cell], []
        label[cell] = len(groups)
        while stack:
            c = stack.pop()
            group.append(c)
            for n in ((c[0] + 1, c[1]), (c[0] - 1, c[1]), (c[0], c[1] + 1), (c[0], c[1] - 1)):
                if n in grid and n not in label and DECK_MIN <= grid[n] - base <= DECK_MAX and abs(grid[n] - grid[c]) <= FLAT:
                    label[n] = len(groups)
                    stack.append(n)
        groups.append(group)
    extent = lambda g: max(math.hypot(a[0] - b[0], a[1] - b[1]) for a in (min(g), max(g)) for b in g)
    deck = set(max((g for g in groups if len(g) > 40), key=extent))

    # Deck axis (principal direction) and lanes across it.
    mx = sum(c[0] for c in deck) / len(deck)
    my = sum(c[1] for c in deck) / len(deck)
    sxx = sum((c[0] - mx) ** 2 for c in deck)
    syy = sum((c[1] - my) ** 2 for c in deck)
    sxy = sum((c[0] - mx) * (c[1] - my) for c in deck)
    angle = 0.5 * math.atan2(2 * sxy, sxx - syy)
    u, v = (math.cos(angle), math.sin(angle)), (-math.sin(angle), math.cos(angle))
    ox, oy = cx + mx * STEP, cy + my * STEP
    st = lambda p: ((p[0] - ox) * u[0] + (p[1] - oy) * u[1], (p[0] - ox) * v[0] + (p[1] - oy) * v[1])
    world = lambda s, t: (ox + s * u[0] + t * v[0], oy + s * u[1] + t * v[1])
    deck_st = [st(pos(c)) for c in deck]
    smin, smax = min(p[0] for p in deck_st), max(p[0] for p in deck_st)
    tmin, tmax = min(p[1] for p in deck_st), max(p[1] for p in deck_st)
    on_deck = lambda x, y: (round((x - cx) / STEP), round((y - cy) / STEP)) in deck
    height = lambda x, y: grid[(round((x - cx) / STEP), round((y - cy) / STEP))]
    yaw0 = math.degrees(angle)
    print('base', base, 'deck cells', len(deck), 'length', round(smax - smin), 'width', round(tmax - tmin), 'yaw', round(yaw0))

    actors, cars = [], []

    def car(s, t, yaw, mesh):
        x, y = world(s, t)
        # All four corners on the deck, not on the barrier or over the edge.
        if not all(on_deck(*world(s + ds, t + dt)) for ds in (-260, 260) for dt in (-110, 110)):
            return
        if any(math.hypot(x - q[0], y - q[1]) < 520 for q in cars):
            return
        cars.append((x, y))
        actors.append([mesh, 'DH_Car_%03d' % len(cars), (x, y, height(x, y) + 2), (0, yaw), None, True])
        glass = mesh.replace('/Mesh/SM_', '/Mesh/Transparent/SM_All_Trans_')
        actors.append([glass, 'DH_CarGlass_%03d' % len(cars), (x, y, height(x, y) + 2), (0, yaw), None, True])

    # Jam: lanes on both sides of the median, cars nose to tail with gaps to move through.
    t = tmin + 250
    while t < tmax - 250:
        reverse = t < (tmin + tmax) / 2
        s = smin + END + random.uniform(0, 400)
        while s < smax - END:
            if random.random() < 0.72:
                car(s, t + random.uniform(-50, 50), yaw0 + (180 if reverse else 0) + random.uniform(-9, 9), random.choice(CARS))
            s += random.uniform(620, 900)
        t += LANE
    # Jackknifed vans across each end.
    for s_end in (smin + END * 0.5, smax - END * 0.5):
        t = tmin + 200
        while t < tmax - 200:
            car(s_end + random.uniform(-80, 80), t, yaw0 + 90 + random.uniform(-25, 25), CARS[2])
            t += 260

    # Starts: deck cells away from cars, spread from one end to the other.
    free = [(x, y, height(x, y)) for x, y in (pos(c) for c in deck)
            if all(math.hypot(x - q[0], y - q[1]) > 350 for q in cars) and smin + END < st((x, y))[0] < smax - END]
    first = min(free, key=lambda p: st(p)[0])
    for index, p in enumerate(spread(free, 16, first)):
        face = yaw0 if st(p)[0] < 0 else yaw0 + 180
        actors.append(['/Script/LyraGame.LyraPlayerStart', 'DH_PlayerStart_%02d' % index, (p[0], p[1], p[2] + 100), (0, face), None, False])
    for index, frac in enumerate((0.2, 0.5, 0.8)):
        s = smin + (smax - smin) * frac
        p = min(free, key=lambda q: abs(st(q)[0] - s) + abs(st(q)[1] - (tmin + tmax) / 2))
        actors.append(['/ShooterCore/Blueprint/B_ControlPointVolume', 'DH_ControlPoint_' + 'ABC'[index], p, None, None, True])

    # Ramps: deck cells whose neighbour continues the road surface below the deck band get an invisible wall there.
    ramps = set()
    for c in deck:
        for n in ((c[0] + 1, c[1]), (c[0] - 1, c[1]), (c[0], c[1] + 1), (c[0], c[1] - 1)):
            if n in grid and n not in deck and abs(grid[n] - grid[c]) <= FLAT:
                ramps.add(n)
    for index, n in enumerate(sorted(ramps)):
        x, y = pos(n)
        actors.append(['/Script/Engine.BlockingVolume', 'DH_RampBlock_%02d' % index, (x, y, grid[n] + 400), None, (STEP / 200, STEP / 200, 8), False])

    top = max(grid[c] for c in deck)
    low = min(grid[c] for c in deck)
    actors.append(['/Script/NavigationSystem.NavMeshBoundsVolume', 'DH_NavBounds', (cx, cy, (low + top) / 2), None,
                   (HALF / 96, HALF / 96, (top - low) / 190 + 8), False])
    for side, (x, y, sx, sy) in {'N': (cx, cy + HALF, HALF / 100 + 1, 1), 'S': (cx, cy - HALF, HALF / 100 + 1, 1),
                                 'E': (cx + HALF, cy, 1, HALF / 100 + 1), 'W': (cx - HALF, cy, 1, HALF / 100 + 1)}.items():
        actors.append(['/Script/Engine.BlockingVolume', 'DH_Bounds_' + side, (x, y, top), None, (sx, sy, 60), False])
    actors.append(['/Script/Engine.SkyAtmosphere', 'DH_SkyAtmosphere', (cx, cy, 0), None, None, False])
    print('cars', len(cars), 'starts', len(free), 'ramp blocks', len(ramps))
    print(editor.script('T = %r\nA = %r\nO = %r\n' % (T, A, O) + ADD, ACTORS=actors, KILLZ=base + (low - base) * 0.4))


if __name__ == '__main__':
    main()
