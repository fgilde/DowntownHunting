"""Weapon icons for the HUD/loadout (white silhouettes like Lyra's T_UI_Icon_RangedWeapons_*), from side-view captures
in front of the sky (shot_i_<Name>.png): python make_weapon_icons.py <capture dir> <out dir>"""
import sys

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageOps


def largest_component(mask):
    """Keeps the biggest connected blob (the weapon), drops neighbours and specks; works on a 1/4 size grid."""
    small = mask.resize((mask.width // 4, mask.height // 4))
    w, h = small.size
    px = small.load()
    seen, best = set(), set()
    for y in range(h):
        for x in range(w):
            if px[x, y] > 127 and (x, y) not in seen:
                blob, todo = set(), [(x, y)]
                seen.add((x, y))
                while todo:
                    cx, cy = todo.pop()
                    blob.add((cx, cy))
                    for nx, ny in ((cx + 1, cy), (cx - 1, cy), (cx, cy + 1), (cx, cy - 1)):
                        if 0 <= nx < w and 0 <= ny < h and px[nx, ny] > 127 and (nx, ny) not in seen:
                            seen.add((nx, ny))
                            todo.append((nx, ny))
                if len(blob) > len(best):
                    best = blob
    keep = Image.new("L", (w, h), 0)
    kp = keep.load()
    for x, y in best:
        kp[x, y] = 255
    keep = keep.resize(mask.size).filter(ImageFilter.MaxFilter(9))
    return ImageChops.multiply(mask, keep)


src, out = sys.argv[1], sys.argv[2]
for name in ["AK47", "MP5", "M1911"]:
    im = Image.open("%s/shot_i_%s.png" % (src, name)).convert("RGB")
    W, H = im.size
    im = im.crop((0, int(H * 0.08), W, int(H * 0.66)))
    W, H = im.size
    # background = wide median of the sky (vertical and horizontal gradient); the weapon fills less than half of the window
    bg = im.resize((W // 8, H // 8)).filter(ImageFilter.MedianFilter(41)).resize((W, H), Image.BILINEAR)
    mask = ImageChops.difference(im, bg).convert("L").point(lambda v: 255 if v > 26 else 0)
    mask = mask.filter(ImageFilter.MaxFilter(11)).filter(ImageFilter.MinFilter(11)).filter(ImageFilter.MedianFilter(5))
    mask = largest_component(mask)
    # fill holes (shiny or sky-coloured spots inside the weapon): everything the outside flood fill does not reach
    outside = ImageOps.expand(mask, 1, 0)
    ImageDraw.floodfill(outside, (0, 0), 128)
    mask = outside.crop((1, 1, W + 1, H + 1)).point(lambda v: 0 if v == 128 else 255)
    box = mask.getbbox()
    mask = mask.crop(box)
    scale = min(470 / mask.width, 220 / mask.height)
    mask = mask.resize((int(mask.width * scale), int(mask.height * scale)), Image.LANCZOS)
    icon = Image.new("RGBA", (512, 256), (0, 0, 0, 0))  # black: the HUD slot material masks with the colour, not alpha
    icon.paste(Image.new("RGBA", mask.size, (255, 255, 255, 255)), ((512 - mask.width) // 2, (256 - mask.height) // 2), mask)
    icon.save("%s/T_UI_Icon_RangedWeapons_%s.png" % (out, name))
    print(name, box)
