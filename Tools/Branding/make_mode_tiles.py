"""Menu tiles for modes that need more than a map view (1024x512, same look as make_tiles.py):
python make_mode_tiles.py <out_dir> <icon_dir> gungame NAME=capture.png | quick NAME=capture.png | teams NAME=capture.png
gungame: the weapon ladder as a row of white silhouettes (icon_dir/<Weapon>.png, 512x256 RGBA), quick: a stopwatch ring,
teams: three team colour chevrons, rounds: a row of round markers (won rounds filled).
The menu card shows only the upper middle of a tile (map name on top, title and text over the lower half, sides cut),
so the overlay sits in a dark band at about 40 % height.
Captures: in game with "DH.Shot <name> noui" (Saved/DHShots)."""
import sys

from PIL import Image, ImageDraw, ImageFilter

from make_tiles import vignette

LADDER = ["MP5", "AK47", "Rifle", "Shotgun", "M1911", "Pistol"]
ACCENT = (255, 107, 13)


def base(src):
    img = Image.open(src).convert("RGB")
    w, h = img.size
    img = img.crop((0, (h - w // 2) // 2, w, (h + w // 2) // 2)) if w / h < 2 else img
    return vignette(img.resize((1024, 512), Image.LANCZOS))


def darken_band(img, centre, half):
    """Soft dark horizontal band around 'centre' so the overlay reads."""
    band = Image.new("L", img.size, 0)
    ImageDraw.Draw(band).rectangle([0, centre - half, img.size[0], centre + half], fill=170)
    band = band.filter(ImageFilter.GaussianBlur(half // 2))
    return Image.composite(Image.new("RGB", img.size, (8, 6, 4)), img, band)


def gungame(src, icon_dir):
    y = 205
    img = darken_band(base(src), y, 48)
    slot_w = 112
    x0 = (1024 - slot_w * len(LADDER)) // 2
    d = ImageDraw.Draw(img)
    for i, name in enumerate(LADDER):
        icon = Image.open("%s/%s.png" % (icon_dir, name)).convert("RGBA")
        alpha = icon.getchannel("A") if icon.getextrema()[3][0] < 255 else icon.convert("L")  # Lyra's icons: white on black
        box = alpha.getbbox()
        alpha = alpha.crop(box)
        scale = min(88 / alpha.width, 44 / alpha.height)
        alpha = alpha.resize((int(alpha.width * scale), int(alpha.height * scale)), Image.LANCZOS)
        colour = ACCENT if i == len(LADDER) - 1 else (235, 228, 220)
        cx = x0 + i * slot_w + slot_w // 2
        img.paste(Image.new("RGB", alpha.size, colour), (cx - alpha.width // 2, y - alpha.height // 2), alpha)
        if i < len(LADDER) - 1:  # chevron to the next weapon
            ax = x0 + (i + 1) * slot_w
            d.line([(ax - 4, y - 7), (ax + 3, y), (ax - 4, y + 7)], fill=ACCENT, width=3)
    return img


def quick(src):
    cx, cy, r = 512, 200, 70
    img = darken_band(base(src), cy, 95)
    glow = Image.new("L", img.size, 0)
    ImageDraw.Draw(glow).ellipse([cx - r - 6, cy - r - 6, cx + r + 6, cy + r + 6], outline=255, width=14)
    img = Image.composite(Image.new("RGB", img.size, ACCENT), img, glow.filter(ImageFilter.GaussianBlur(10)).point(lambda v: v // 2))
    d = ImageDraw.Draw(img)
    d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=(235, 228, 220), width=8)
    d.arc([cx - r, cy - r, cx + r, cy + r], start=-90, end=30, fill=ACCENT, width=8)  # a third of the time gone
    d.rectangle([cx - 10, cy - r - 22, cx + 10, cy - r - 8], fill=(235, 228, 220))  # crown
    d.line([(cx, cy), (cx, cy - r + 22)], fill=(235, 228, 220), width=6)
    d.line([(cx, cy), (cx + 38, cy + 22)], fill=ACCENT, width=6)
    d.ellipse([cx - 8, cy - 8, cx + 8, cy + 8], fill=(235, 228, 220))
    return img


def teams(src):
    y = 205
    img = darken_band(base(src), y, 55)
    d = ImageDraw.Draw(img)
    for i, colour in enumerate([(230, 60, 40), (40, 120, 235), (60, 200, 90)]):
        x = 512 + (i - 1) * 120
        d.polygon([(x - 45, y - 40), (x + 5, y - 40), (x + 45, y), (x + 5, y + 40), (x - 45, y + 40), (x - 5, y)], fill=colour)
    return img


def rounds(src):
    y = 205
    img = darken_band(base(src), y, 45)
    d = ImageDraw.Draw(img)
    for i in range(7):
        x = 512 + (i - 3) * 62
        filled = [(40, 120, 235), (230, 60, 40), (40, 120, 235), None, None, None, None][i]
        d.ellipse([x - 20, y - 20, x + 20, y + 20], outline=(235, 228, 220), width=4, fill=filled)
    return img


KINDS = {"gungame": lambda src, icons: gungame(src, icons), "quick": lambda src, icons: quick(src),
         "teams": lambda src, icons: teams(src), "rounds": lambda src, icons: rounds(src)}


if __name__ == "__main__":
    out_dir, icon_dir = sys.argv[1], sys.argv[2]
    args = sys.argv[3:]
    for kind, spec in zip(args[::2], args[1::2]):
        name, src = spec.split("=", 1)
        KINDS[kind](src, icon_dir).save("%s/%s.png" % (out_dir, name))
        print(name)
