"""Runs in the City Sample project: finds areas for a rooftop map. Takes the bounds of all building actors of
Small_City_LVL and scores 120 x 120 m windows by how much of them is covered by roofs in one 10 m height band
(many neighbouring roofs at about the same height = short jumps and planks between them).
UnrealEditor-Cmd.exe CitySample.uproject -run=pythonscript -script="<repo>/Tools/Python/cs_roofs.py" -nullrhi"""
import math

import unreal

WINDOW = 6000.0   # half size, cm
STEP = 2500.0
BAND = 1000.0

unreal.EditorLoadingAndSavingUtils.load_map("/Game/Map/Small_City_LVL")
roofs = []
for d in unreal.WorldPartitionBlueprintLibrary.get_actor_descs():
    layers = " ".join(p.export_text() for p in d.data_layer_assets)
    if "Buildings" not in layers or "Collision" in layers:
        continue
    b = d.bounds
    w, h = b.max.x - b.min.x, b.max.y - b.min.y
    if 800 < w < 8000 and 800 < h < 8000 and b.max.z > 1500:
        roofs.append((b.min.x, b.min.y, b.max.x, b.max.y, b.max.z))
unreal.log("ROOFS %d building actors" % len(roofs))

xs = [r[0] for r in roofs] + [r[2] for r in roofs]
ys = [r[1] for r in roofs] + [r[3] for r in roofs]
results = []
x = min(xs)
while x < max(xs):
    y = min(ys)
    while y < max(ys):
        inside = [r for r in roofs if math.fabs((r[0] + r[2]) / 2 - x) < WINDOW and math.fabs((r[1] + r[3]) / 2 - y) < WINDOW]
        for base in range(1500, 6000, 500):
            band = [r for r in inside if base <= r[4] < base + BAND]
            area = sum((r[2] - r[0]) * (r[3] - r[1]) for r in band) / (4 * WINDOW * WINDOW)
            if len(band) >= 6:
                results.append((area, len(band), x, y, base))
        y += STEP
    x += STEP
results.sort(reverse=True)
seen = []
for area, count, x, y, base in results:
    if any(math.fabs(x - sx) < 15000 and math.fabs(y - sy) < 15000 for sx, sy in seen):
        continue
    seen.append((x, y))
    unreal.log("ROOFS centre (%6.0f m, %6.0f m) roofs %2d at %3.0f-%3.0f m, coverage %.2f" % (x / 100, y / 100, count, base / 100, (base + BAND) / 100, area))
    if len(seen) >= 12:
        break
