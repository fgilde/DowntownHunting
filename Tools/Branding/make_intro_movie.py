"""Intro movie, step 2: Saved/IntroFrames (from Tools/Python/make_intro.py) -> Content/Movies/DH_Intro.mp4.
Adds fade in/out, a tagline, the logo at the end and a synthesized drone/whoosh soundtrack. Needs ffmpeg on PATH."""
import os
import subprocess

from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
FRAMES = os.path.join(REPO, "Saved", "IntroFrames")
TMP = os.path.join(REPO, "Saved", "IntroBuild")
OUT = os.path.join(REPO, "Content", "Movies", "DH_Intro.mp4")
FPS, SECONDS = 30, 20
TAGLINE = "EVERY STREET IS A HUNTING GROUND"

os.makedirs(TMP, exist_ok=True)
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
    "[0:v]fade=t=in:st=0:d=1.2,fade=t=out:st=19.2:d=0.8[base];"
    "[1:v]format=rgba,fade=t=in:st=1.8:d=0.6:alpha=1,fade=t=out:st=5.6:d=0.8:alpha=1[tag];"
    "[2:v]format=rgba,fade=t=in:st=14.5:d=1.2:alpha=1[logo];"
    "[base][tag]overlay=x=0:y=H-260:enable='between(t,1.8,6.4)'[v1];"
    "[v1][logo]overlay=x=(W-w)/2:y=(H-h)/2-40:enable='gte(t,14.5)'[v]"
)
# drone (55/110 Hz with slow swell) + filtered noise whoosh for the crane shot
audio = (
    f"aevalsrc='0.35*sin(2*PI*55*t)*(0.6+0.4*sin(2*PI*0.25*t))+0.18*sin(2*PI*110.5*t)':s=48000:d={SECONDS}[drone];"
    f"anoisesrc=d={SECONDS}:c=pink:a=0.5,lowpass=f=900,volume='if(between(t,8,14),(t-8)/6,if(gt(t,14),max(0,1-(t-14)/2),0))':eval=frame[whoosh];"
    "[drone][whoosh]amix=inputs=2:normalize=0,afade=t=in:st=0:d=1.5,afade=t=out:st=18.5:d=1.5,volume=0.8[a]"
)
cmd = [
    "ffmpeg", "-y", "-loglevel", "error",
    "-framerate", str(FPS), "-i", os.path.join(FRAMES, "%04d.png"),
    "-loop", "1", "-framerate", str(FPS), "-t", str(SECONDS), "-i", os.path.join(TMP, "tagline.png"),
    "-loop", "1", "-framerate", str(FPS), "-t", str(SECONDS), "-i", os.path.join(TMP, "logo.png"),
    "-filter_complex", video + ";" + audio,
    "-map", "[v]", "-map", "[a]", "-t", str(SECONDS),
    "-c:v", "libx264", "-pix_fmt", "yuv420p", "-crf", "20", "-preset", "slow",
    "-c:a", "aac", "-b:a", "160k", "-movflags", "+faststart", OUT,
]
subprocess.run(cmd, check=True)
print(OUT, os.path.getsize(OUT) // 1024, "KB")
