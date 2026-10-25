"""Draws the poses dumped by make_moves.py (Saved/moves_pose.json) as stick figures, side and front view, into
Saved/moves_pose.png: python Tools/Python/plot_pose.py. Side view: forward to the right, front view: the figure's left on
the right. Left side bones cyan, right side magenta."""
import json
import os

from PIL import Image, ImageDraw

SAVED = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'Saved')
data = json.load(open(os.path.join(SAVED, 'moves_pose.json')))
parents = data.pop('parents')
names = sorted(data)
CELL, SCALE = 300, 1.4
image = Image.new('RGB', (CELL * 2, CELL * len(names)), (20, 20, 28))
draw = ImageDraw.Draw(image)
for row, name in enumerate(names):
    pose = data[name]
    for col, (axis, label) in enumerate(((1, 'side'), (0, 'front'))):
        ox, oy = col * CELL + CELL // 2, row * CELL + CELL - 30
        draw.line([(col * CELL, oy), (col * CELL + CELL, oy)], fill=(60, 60, 70))
        draw.text((col * CELL + 6, row * CELL + 4), '%s  %s' % (name, label), fill=(220, 220, 220))
        for bone, parent in parents.items():
            if parent is None or 'twist' in bone or 'ik_' in bone or bone not in pose or parent not in pose:
                continue
            a, b = pose[parent], pose[bone]
            colour = (0, 220, 255) if bone.endswith('_l') else (255, 40, 140) if bone.endswith('_r') else (255, 220, 0)
            draw.line([(ox + a[axis] * SCALE, oy - a[2] * SCALE), (ox + b[axis] * SCALE, oy - b[2] * SCALE)], fill=colour, width=3)
image.save(os.path.join(SAVED, 'moves_pose.png'))
print('saved')
