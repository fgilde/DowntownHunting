"""Intro movie, step 2: Saved/IntroFrames -> Content/Movies/DH_Intro.mp4.
Frames: a camera flight over Downtown in a running bot match, low down a street where the bots fight, then up over
the block (python Tools/Branding/make_showcase.py rec intro -> Saved/Record/intro, DH.Intro ... flight).
"GILDE GAMES PRESENTS" over the first seconds, the tagline, the logo slams in during the climb (zoom, white flash, bass
hit). Synthesized soundtrack (drone, gunfire while low in the street, whoosh and hits). Needs ffmpeg on PATH.
Shown in the main menu at the first start (UDHIntroScreen), any input skips it."""
import math
import os
import random
import struct
import subprocess
import wave

from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
FRAMES = os.path.join(REPO, "Saved", "Record", "intro")
TMP = os.path.join(REPO, "Saved", "IntroBuild")
OUT = os.path.join(REPO, "Content", "Movies", "DH_Intro.mp4")
FPS, SECONDS = 30, 15
TOTAL = SECONDS
TITLE = (0.3, 3.2)     # studio name over the street
LOGO_AT = 10.0         # logo slams in during the climb
TAGLINE = "EVERY STREET IS A HUNTING GROUND"
STUDIO, PRESENTS = "GILDE GAMES", "PRESENTS"

os.makedirs(TMP, exist_ok=True)


def gunfire(path, rate=48000):
    """Bursts of shots (noise crack + low thump, fast decay), near and far, during the fights."""
    random.seed(3)
    samples = [0.0] * (rate * SECONDS)
    t = 0.6
    while t < 7.5:  # while the camera is low in the street
        near = random.random() < 0.5
        for k in range(random.randint(1, 4)):
            start = int((t + k * random.uniform(0.09, 0.16)) * rate)
            gain = random.uniform(0.5, 0.8) if near else random.uniform(0.12, 0.25)
            smooth = 0.0
            for i in range(int(0.35 * rate)):
                if start + i >= len(samples):
                    break
                x = i / rate
                smooth += (random.uniform(-1, 1) - smooth) * (0.6 if near else 0.15)  # far shots duller
                samples[start + i] += gain * (smooth * math.exp(-x / 0.045) + 0.8 * math.sin(2 * math.pi * 70 * x) * math.exp(-x / 0.09))
        t += random.uniform(0.35, 1.3)
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(b"".join(struct.pack("<h", int(max(-1.0, min(1.0, v)) * 32000)) for v in samples))


gunfire(os.path.join(TMP, "gunfire.wav"))


def spaced(draw, centre, text, font, spacing, fill):
    """Text with wide letter spacing, centred."""
    widths = [draw.textlength(c, font=font) for c in text]
    x = centre[0] - (sum(widths) + spacing * (len(text) - 1)) / 2
    for c, w in zip(text, widths):
        draw.text((x, centre[1]), c, font=font, fill=fill, anchor="lm")
        x += w + spacing


card = Image.new("RGBA", (1920, 1080), (0, 0, 0, 0))
band = Image.new("L", (1920, 1080), 0)
ImageDraw.Draw(band).rectangle([0, 400, 1920, 700], fill=150)
card.putalpha(band.filter(ImageFilter.GaussianBlur(60)))
d = ImageDraw.Draw(card)
spaced(d, (960, 500), STUDIO, ImageFont.truetype("C:/Windows/Fonts/impact.ttf", 130), 26, (240, 232, 220, 255))
d.rectangle([760, 582, 1160, 586], fill=(255, 107, 13, 255))  # accent line
spaced(d, (960, 640), PRESENTS, ImageFont.truetype("C:/Windows/Fonts/arial.ttf", 34), 18, (190, 182, 170, 255))
card.save(os.path.join(TMP, "card.png"))
os.makedirs(os.path.dirname(OUT), exist_ok=True)

logo = Image.open(os.path.join(HERE, "T_DH_Logo.png")).convert("RGBA")
logo.resize((1100, int(logo.height * 1100 / logo.width)), Image.LANCZOS).save(os.path.join(TMP, "logo.png"))

tag = Image.new("RGBA", (1920, 140), (0, 0, 0, 0))
d = ImageDraw.Draw(tag)
font = ImageFont.truetype("C:/Windows/Fonts/impact.ttf", 64)
d.text((963, 73), TAGLINE, font=font, fill=(0, 0, 0, 160), anchor="mm")  # drop shadow
d.text((960, 70), TAGLINE, font=font, fill=(240, 232, 220, 255), anchor="mm")
tag.save(os.path.join(TMP, "tagline.png"))

video = (
    f"[0:v]fade=t=in:st=0:d=0.8,fade=t=out:st={TOTAL - 0.8}:d=0.8[base];"
    "[1:v]format=rgba,fade=t=in:st=3.6:d=0.5:alpha=1,fade=t=out:st=6.6:d=0.6:alpha=1[tag];"
    # logo: from 2.2x down to size in 0.35 s (ease out), quick fade in
    f"[2:v]format=rgba,scale=w='iw*(1+1.2*pow(clip(1-(t-{LOGO_AT})/0.35,0,1),2))':h=-1:eval=frame,"
    f"fade=t=in:st={LOGO_AT}:d=0.12:alpha=1[logo];"
    f"[3:v]format=rgba,fade=t=in:st={TITLE[0]}:d=0.7:alpha=1,fade=t=out:st={TITLE[1] - 0.6}:d=0.6:alpha=1[card];"
    # white flash when the logo lands
    f"color=white:s=1920x1080:r={FPS}:d={TOTAL},format=rgba,colorchannelmixer=aa=0,"
    f"geq=r=255:g=255:b=255:a='255*0.85*if(between(T,{LOGO_AT + 0.3},{LOGO_AT + 0.75}),1-(T-{LOGO_AT + 0.3})/0.45,0)'[flash];"
    f"[base][card]overlay=x=0:y=0:enable='lt(t,{TITLE[1]})'[v0];"
    "[v0][tag]overlay=x=0:y=H-260:enable='between(t,3.6,7.2)'[v1];"
    f"[v1][logo]overlay=x=(W-w)/2:y=(H-h)/2-40:enable='gte(t,{LOGO_AT})'[v2];"
    "[v2][flash]overlay=x=0:y=0[v]"
)
hits = " + ".join(f"0.9*if(between(t,{at},{at + 2.5}),sin(2*PI*42*(t-{at}))*exp(-(t-{at})*2.2),0)" for at in (TITLE[0] + 0.3, LOGO_AT + 0.3))
# drone, bass hits (studio name, logo), noise whoosh before the logo, gunfire during the fights
audio = (
    f"aevalsrc='0.35*sin(2*PI*55*t)*(0.6+0.4*sin(2*PI*0.25*t))+0.18*sin(2*PI*110.5*t)':s=48000:d={TOTAL}[drone];"
    f"aevalsrc='{hits}':s=48000:d={TOTAL}[hits];"
    f"anoisesrc=d={TOTAL}:c=pink:a=0.5,lowpass=f=900,volume='if(between(t,{LOGO_AT - 3},{LOGO_AT + 0.3}),(t-{LOGO_AT - 3})/3.3,if(gt(t,{LOGO_AT + 0.3}),max(0,1-(t-{LOGO_AT + 0.3})/1.2),0))':eval=frame[whoosh];"
    "[4:a]volume=0.9[shots];"
    f"[drone][hits][whoosh][shots]amix=inputs=4:normalize=0,afade=t=in:st=0:d=0.5,afade=t=out:st={TOTAL - 1.5}:d=1.5,volume=0.8[a]"
)
cmd = [
    "ffmpeg", "-y", "-loglevel", "error",
    "-framerate", str(FPS), "-i", os.path.join(FRAMES, "%04d.png"),
    "-loop", "1", "-framerate", str(FPS), "-t", str(TOTAL), "-i", os.path.join(TMP, "tagline.png"),
    "-loop", "1", "-framerate", str(FPS), "-t", str(TOTAL), "-i", os.path.join(TMP, "logo.png"),
    "-loop", "1", "-framerate", str(FPS), "-t", str(TOTAL), "-i", os.path.join(TMP, "card.png"),
    "-i", os.path.join(TMP, "gunfire.wav"),
    "-filter_complex", video + ";" + audio,
    "-map", "[v]", "-map", "[a]", "-t", str(TOTAL),
    "-c:v", "libx264", "-pix_fmt", "yuv420p", "-crf", "20", "-preset", "slow",
    "-c:a", "aac", "-b:a", "160k", "-movflags", "+faststart", OUT,
]
subprocess.run(cmd, check=True)
print(OUT, os.path.getsize(OUT) // 1024, "KB")
