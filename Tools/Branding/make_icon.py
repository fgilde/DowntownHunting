"""Renders the Windows app icon (Build/Windows/Application.ico): python make_icon.py <out.ico> [preview.png].

Neon-yellow slanted tile with a black "DH" in Impact, matching the arcade menus."""
import sys

from PIL import Image, ImageDraw, ImageFont

from make_logo import FONTS

S = 512
YELLOW, MAGENTA, INK = (255, 209, 0), (255, 16, 107), (12, 6, 18)


def render():
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    # slanted tile with a hard magenta drop shadow, like the menu cards
    tile = [(70, 24), (S - 8, 24), (S - 70, S - 40), (8, S - 40)]
    d.polygon([(x + 18, y + 22) for x, y in tile], fill=MAGENTA + (255,))
    d.polygon(tile, fill=YELLOW + (255,))
    font = ImageFont.truetype(FONTS + "impact.ttf", 300)
    text = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    td = ImageDraw.Draw(text)
    td.text((S // 2 + 4, S // 2 - 8), "DH", font=font, anchor="mm", fill=INK + (255,), stroke_width=14, stroke_fill=YELLOW + (255,))
    img.alpha_composite(text)
    return img


def main(out, preview=None):
    img = render()
    if preview:
        img.save(preview)
    img.save(out, sizes=[(256, 256), (128, 128), (64, 64), (48, 48), (32, 32), (24, 24), (16, 16)])
    print("saved", out)


if __name__ == "__main__":
    main(*sys.argv[1:])
