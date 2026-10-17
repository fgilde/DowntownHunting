"""Renders the Downtown Hunting logo (transparent PNG) from system fonts: python make_logo.py <out.png>."""
import random
import sys

from PIL import Image, ImageDraw, ImageFilter, ImageFont

W, H = 2048, 1024
AMBER, ORANGE, DEEP = (255, 186, 40), (255, 112, 32), (24, 14, 10)
FONTS = "C:/Windows/Fonts/"


def gradient_text(size, text, font, top, bottom):
    """Text mask filled with a vertical gradient."""
    mask = Image.new("L", size, 0)
    ImageDraw.Draw(mask).text((size[0] // 2, size[1] // 2), text, font=font, fill=255, anchor="mm")
    grad = Image.new("RGBA", size)
    gd = ImageDraw.Draw(grad)
    for y in range(size[1]):
        t = y / size[1]
        gd.line([(0, y), (size[0], y)], fill=tuple(int(a + (b - a) * t) for a, b in zip(top, bottom)) + (255,))
    grad.putalpha(mask)
    return grad, mask


def outline(mask, width, color):
    grown = mask.filter(ImageFilter.MaxFilter(width * 2 + 1))
    layer = Image.new("RGBA", mask.size, color + (255,))
    layer.putalpha(grown)
    return layer


def skyline(width, height, seed=7):
    """Dark city silhouette with a few lit windows."""
    rnd = random.Random(seed)
    img = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    x = 0
    while x < width:
        w = rnd.randint(50, 140)
        h = rnd.randint(int(height * 0.25), height)
        d.rectangle([x, height - h, x + w, height], fill=DEEP + (255,))
        if rnd.random() < 0.3:  # water tank / antenna
            d.rectangle([x + w // 3, height - h - 30, x + w // 3 + 18, height - h], fill=DEEP + (255,))
        for wy in range(height - h + 14, height - 10, 22):
            for wx in range(x + 8, x + w - 10, 18):
                if rnd.random() < 0.18:
                    d.rectangle([wx, wy, wx + 7, wy + 10], fill=AMBER + (200,))
        x += w + rnd.randint(2, 10)
    # fade to transparent at both ends
    fade = Image.new("L", (width, height), 255)
    fd = ImageDraw.Draw(fade)
    for i in range(300):
        a = int(255 * i / 300)
        fd.line([(i, 0), (i, height)], fill=a)
        fd.line([(width - 1 - i, 0), (width - 1 - i, height)], fill=a)
    img.putalpha(Image.composite(img.getchannel("A"), Image.new("L", img.size, 0), fade))
    return img


def crosshair(size, color):
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    c, r, w = size // 2, size // 2 - 12, 16
    d.ellipse([c - r, c - r, c + r, c + r], outline=color + (255,), width=w)
    gap = r // 3
    for a, b in [((c, 0), (c, c - gap)), ((c, c + gap), (c, size)), ((0, c), (c - gap, c)), ((c + gap, c), (size, c))]:
        d.line([a, b], fill=color + (255,), width=w)
    d.ellipse([c - 10, c - 10, c + 10, c + 10], fill=color + (255,))
    return img


def main(out):
    logo = Image.new("RGBA", (W, H), (0, 0, 0, 0))

    city = skyline(W, 330)
    logo.alpha_composite(city, (0, 560))

    big = ImageFont.truetype(FONTS + "impact.ttf", 330)
    downtown, dmask = gradient_text((W, 420), "DOWNTOWN", big, AMBER, ORANGE)
    glow = outline(dmask, 22, ORANGE).filter(ImageFilter.GaussianBlur(28))
    logo.alpha_composite(glow, (0, 110))
    logo.alpha_composite(outline(dmask, 9, DEEP), (0, 110))
    logo.alpha_composite(downtown, (0, 110))

    # The first "O" becomes a scope: crosshair centred on it.
    left = (W - big.getlength("DOWNTOWN")) / 2
    ox = int(left + big.getlength("D") + big.getlength("O") / 2)
    top, bottom = big.getbbox("O", anchor="mm")[1], big.getbbox("O", anchor="mm")[3]
    oy = 110 + 210 + int((top + bottom) / 2)
    size = int((bottom - top) * 1.08)
    scope = crosshair(size, (255, 245, 230))
    shadow = outline(scope.getchannel("A"), 6, DEEP)
    logo.alpha_composite(shadow, (ox - size // 2, oy - size // 2))
    logo.alpha_composite(scope, (ox - size // 2, oy - size // 2))

    small = ImageFont.truetype(FONTS + "STENCIL.TTF", 190)
    hunting, hmask = gradient_text((W, 260), "H U N T I N G", small, (255, 240, 220), AMBER)
    logo.alpha_composite(outline(hmask, 8, DEEP), (0, 560))
    logo.alpha_composite(hunting, (0, 560))

    logo.save(out)
    print("saved", out)


if __name__ == "__main__":
    main(sys.argv[1])
