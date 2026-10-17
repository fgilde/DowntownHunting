"""Turns editor viewport captures into 1024x512 menu tiles: python make_tiles.py <out_dir> name=capture.png[:x0,y0,x1,y1][+fix] ...
Adds a soft vignette; +fix also paints out thin orange editor volume lines in the sky."""
import sys

from PIL import Image, ImageDraw, ImageFilter


def remove_volume_lines(img):
    """Paints out thin orange lines (editor volume wireframes). Big orange areas (sunlit facades) stay."""
    px = img.load()
    mask = Image.new("L", img.size, 0)
    mp = mask.load()
    for y in range(int(img.size[1] * 0.55)):  # upper part only: road markings are thin and yellow too
        for x in range(img.size[0]):
            r, g, b = px[x, y][:3]
            if r > 170 and 90 < g < 200 and b < 90 and r - b > 120:
                mp[x, y] = 255
    blobs = mask.filter(ImageFilter.MinFilter(5)).filter(ImageFilter.MaxFilter(9))
    thin = Image.composite(Image.new("L", mask.size, 0), mask, blobs).filter(ImageFilter.MaxFilter(5))
    blurred = img.filter(ImageFilter.MedianFilter(9)).filter(ImageFilter.GaussianBlur(4))
    return Image.composite(blurred, img, thin)


def vignette(img, strength=0.55):
    w, h = img.size
    v = Image.new("L", (w, h), 0)
    ImageDraw.Draw(v).ellipse([-w * 0.25, -h * 0.35, w * 1.25, h * 1.35], fill=255)
    v = v.filter(ImageFilter.GaussianBlur(w // 8))
    dark = Image.new("RGB", (w, h), (8, 6, 4))
    return Image.composite(img, Image.blend(img, dark, strength), v)


def main(out_dir, specs):
    for spec in specs:
        name, src = spec.split("=", 1)
        fix = src.endswith("+fix")
        src = src[:-4] if fix else src
        box = None
        if ":" in src[2:]:
            src, crop = src.rsplit(":", 1)
            box = tuple(int(v) for v in crop.split(","))
        img = Image.open(src).convert("RGB")
        if box:
            img = img.crop(box)
        img = img.crop((0, 0, img.size[0], img.size[1] - 60))  # editor axis gizmo bottom left
        w, h = img.size  # centre crop to 2:1
        if w / h > 2:
            img = img.crop(((w - 2 * h) // 2, 0, (w + 2 * h) // 2, h))
        else:
            img = img.crop((0, (h - w // 2) // 2, w, (h + w // 2) // 2))
        img = (remove_volume_lines(img) if fix else img).resize((1024, 512), Image.LANCZOS)
        vignette(img).save("%s/%s.png" % (out_dir, name))
        print(name)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2:])
