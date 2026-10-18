"""Runs in the City Sample project: finds candidate areas for new maps. Bins the actors of Small_City_LVL into 250 m cells
and logs per cell how many freeway/bridge, tree/park, building actors there are and the highest building, so a
region for cs_region.py can be picked by character (under the freeway, park, skyscrapers ...).
UnrealEditor-Cmd.exe CitySample.uproject -run=pythonscript -script="<repo>/Tools/Python/cs_survey.py" -nullrhi"""
import collections

import unreal

CELL = 25000.0
KEYWORDS = {"freeway": ("freeway", "highway", "overpass", "ramp"), "bridge": ("bridge",), "trees": ("tree", "park", "foliage", "plant"),
            "plaza": ("plaza", "fountain", "statue"), "rail": ("rail", "train", "subway")}

unreal.EditorLoadingAndSavingUtils.load_map("/Game/Map/Small_City_LVL")
cells = collections.defaultdict(lambda: collections.Counter())
for d in unreal.WorldPartitionBlueprintLibrary.get_actor_descs():
    b = d.bounds
    x, y = (b.min.x + b.max.x) / 2, (b.min.y + b.max.y) / 2
    key = (int(x // CELL), int(y // CELL))
    text = (str(d.label) + " " + str(d.actor_package) + " " + " ".join(p.export_text() for p in d.data_layer_assets)).lower()
    c = cells[key]
    for name, words in KEYWORDS.items():
        if any(w in text for w in words):
            c[name] += 1
    if "building" in text:
        c["buildings"] += 1
        c["height"] = max(c["height"], int(b.max.z / 100))
    c["actors"] += 1

for name in list(KEYWORDS) + ["height"]:
    best = sorted(cells.items(), key=lambda kv: kv[1][name], reverse=True)[:6]
    for (ix, iy), c in best:
        if c[name]:
            unreal.log("SURVEY %-8s cell centre (%6.0f m, %6.0f m): %s" % (name, (ix + 0.5) * CELL / 100, (iy + 0.5) * CELL / 100, dict(c)))
