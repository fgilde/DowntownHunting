"""Content/Splash/Splash.bmp (game start, before the engine is up) and EdSplash.bmp (editor start) from the menu background and logo."""
import os
from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "..", "Content", "Splash")


def splash(w, h, caption):
    bg = Image.open(os.path.join(HERE, "Loading", "T_DH_Menu_Background.png")).convert("RGB")
    scale = max(w / bg.width, h / bg.height)
    bg = bg.resize((int(bg.width * scale), int(bg.height * scale)), Image.LANCZOS)
    bg = bg.crop(((bg.width - w) // 2, (bg.height - h) // 2, (bg.width + w) // 2, (bg.height + h) // 2))
    bg = Image.blend(bg, Image.new("RGB", (w, h), (6, 5, 4)), 0.45).filter(ImageFilter.GaussianBlur(1.5))
    logo = Image.open(os.path.join(HERE, "T_DH_Logo.png")).convert("RGBA")
    lw = int(w * 0.62)
    logo = logo.resize((lw, int(logo.height * lw / logo.width)), Image.LANCZOS)
    bg.paste(logo, ((w - lw) // 2, int(h * 0.42 - logo.height / 2)), logo)
    d = ImageDraw.Draw(bg)
    font = ImageFont.truetype("C:/Windows/Fonts/consola.ttf", max(12, h // 32))
    d.rectangle([0, h - h // 90, int(w * 0.35), h], fill=(230, 120, 20))  # accent bar
    d.text((w - 20, h - 20), caption, font=font, fill=(200, 190, 175), anchor="rd")
    return bg


os.makedirs(OUT, exist_ok=True)
splash(960, 540, "loading the city ...").save(os.path.join(OUT, "Splash.bmp"))
splash(800, 450, "Downtown Hunting editor").save(os.path.join(OUT, "EdSplash.bmp"))
print("ok")
