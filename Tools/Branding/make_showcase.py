"""Showcase video: records clips in the game and cuts them to Build/Showcase/DH_Showcase.mp4.
  python Tools/Branding/make_showcase.py            record missing clips, then cut
  python Tools/Branding/make_showcase.py cut        only cut (clips in Saved/Record)
  python Tools/Branding/make_showcase.py rec NAME   record one clip again
Every clip runs the uncooked game with a fixed time step (-UseFixedTimeStep -FPS=30): each frame is 1/30 s of game time
however long the PC needs for it, so the video is smooth on slow machines too (recording just takes longer).
DH.Record films with the HUD (menus, the player's view), DH.Intro films bot fights with a director camera.
Menu background videos follow the game time while recording (UDHMenuBackground). Needs ffmpeg on PATH."""
import math
import os
import random
import shutil
import struct
import subprocess
import sys
import time
import wave

from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
RECORD = os.path.join(REPO, "Saved", "Record")
OUT_DIR = os.path.join(REPO, "Build", "Showcase")
UE = r"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
FPS = 30
W, H = 1920, 1080
IMPACT = "C:/Windows/Fonts/impact.ttf"
ARIAL = "C:/Windows/Fonts/arial.ttf"
ACCENT = (255, 107, 13)

MENU_STEPS = ("wait 12; DH.Record 7 m_main; wait 1.6; DH.Key Gamepad_DPad_Down; wait 1.6; DH.Key Gamepad_DPad_Down; wait 1.6; "
              "DH.Key Gamepad_DPad_Down; wait 1.6; DH.Key Gamepad_DPad_Up; wait 1.2; DH.Key Gamepad_DPad_Up; DH.Key Gamepad_DPad_Up; wait 0.6; "
              "DH.Key Gamepad_FaceButton_Bottom; wait 1.5; DH.Record 9 m_play; wait 1; DH.Key Gamepad_DPad_Right; wait 1; DH.Key Gamepad_DPad_Right; "
              "wait 1; DH.Key Gamepad_DPad_Right; wait 1; DH.Key Gamepad_DPad_Down; wait 1; DH.Key Gamepad_DPad_Right; wait 1; DH.Key Gamepad_DPad_Down; "
              "wait 1; DH.Key Gamepad_DPad_Right; wait 2.5; DH.Key Escape; wait 1.5; DH.OpenScreen Character; wait 1.5; DH.Record 6 m_character; "
              "wait 1.2; DH.Key Gamepad_DPad_Right; wait 1.2; DH.Key Gamepad_DPad_Right; wait 1.2; DH.Key Gamepad_DPad_Right; wait 2.6; DH.Key Escape; "
              "wait 1.5; DH.OpenScreen Loadout; wait 1.5; DH.Record 6 m_loadout; wait 1.2; DH.Key Gamepad_DPad_Right; wait 1.2; DH.Key Gamepad_DPad_Right; "
              "wait 1.2; DH.Key Gamepad_DPad_Down; wait 2.6; DH.Key Escape; wait 1.5; DH.OpenScreen Settings; wait 2; DH.Record 7 m_options; wait 2; "
              "DH.Key Gamepad_RightShoulder; wait 2; DH.Key Gamepad_RightShoulder; wait 2; DH.Key Gamepad_RightShoulder")
MOVES_STEPS = ("wait 38; DH.Teleport -58600 34000 200 0; wait 1.5; DH.Record 18 p_moves; DH.Key W:2.6; wait 1.3; DH.Key LeftControl; wait 1.6; DH.Key LeftControl; wait 0.6; "
               "DH.Key V; wait 2.5; DH.Key V; wait 0.6; DH.Streak 3; wait 3; DH.Key Tab:2.6; wait 3.2; DH.Streak 2; wait 4")

# name: (URL or None for the front end, DH.Run steps, seconds of the clip, clips this run records)
SESSIONS = {
    "menus": (None, MENU_STEPS, 0, ["m_main", "m_play", "m_character", "m_loadout", "m_options"]),
    "p_moves": ("/Game/DH/Maps/DH_Block?Experience=B_DH_TDM?NumBots=7?Perspective=Choice", MOVES_STEPS, 0, ["p_moves"]),
    "p_gungame": ("/DowntownCore/Maps/DH_Depot?Experience=B_DH_FFA?NumBots=7?GunGame=MP5,AK47,Rifle,Shotgun,M1911,Pistol",
                  "wait 40; DH.Record 6 p_gungame", 0, ["p_gungame"]),
    "p_end": ("/Game/DH/Maps/DH_Block?Experience=B_DH_FFA?NumBots=7?TimeLimit=1", "wait 88; DH.Record 7 p_end", 0, ["p_end"]),
}
CINEMA = [  # director clips: name, map, experience, extra options, caption
    ("c_tdm", "/Game/DH/Maps/DH_Block", "B_DH_TDM", "?NumBots=10", "TEAM DEATHMATCH  ·  DOWNTOWN"),
    ("c_roofs", "/Game/DH/Maps/DH_Roofs", "B_DH_TDM", "?NumBots=8", "ROOFTOPS  ·  FALL AND YOU DIE"),
    ("c_ctf", "/Game/DH/Maps/DH_Gridlock", "B_DH_CTF", "?NumBots=10", "CAPTURE THE FLAG  ·  GRIDLOCK"),
    ("c_koth", "/Game/DH/Maps/DH_Freeway", "B_DH_Hill", "?NumBots=10", "KING OF THE HILL  ·  OVERPASS"),
    ("c_dom", "/Game/DH/Maps/DH_Park", "B_DH_Domination", "?NumBots=10", "DOMINATION  ·  WATERFRONT"),
    ("c_infected", "/Game/DH/Maps/DH_Block", "B_DH_Infected", "?NumBots=9", "INFECTED  ·  DOWNTOWN"),
    ("c_ffa", "/DowntownCore/Maps/DH_Depot", "B_DH_FFA", "?NumBots=7", "FREE-FOR-ALL  ·  DEPOT"),
    ("c_snd", "/Game/DH/Maps/DH_Freeway", "B_DH_SearchAndDestroy", "?NumBots=9", "SEARCH & DESTROY  ·  OVERPASS"),
]
CINEMA_SECONDS = 6
for name, map_, exp, opts, _ in CINEMA:
    SESSIONS[name] = (f"{map_}?Experience={exp}{opts}", f"wait 40; DH.Intro {CINEMA_SECONDS} {name} nocrane", CINEMA_SECONDS, [name])

# The cut: (clip, caption); "#Title|Subtitle" is a title card.
CUT = [
    ("#INTRO", None),
    ("#THE MENUS|Character, loadout, options, every mode a click away", None),
    ("m_main", "MAIN MENU  ·  LEVEL AND XP"), ("m_play", "PLAY  ·  ALL MODES AND MAPS, OFFLINE, LAN OR ONLINE"),
    ("m_character", "PICK YOUR HUNTER"), ("m_loadout", "LOADOUT  ·  WEAPONS AND ATTACHMENTS UNLOCK WITH YOUR LEVEL"),
    ("m_options", "OPTIONS  ·  VIDEO, AUDIO, CONTROLS, GAMEPAD"),
    ("#ON THE STREET|Slide, wall jump, killstreaks", None),
    ("p_moves", "SLIDE  ·  FIRST OR THIRD PERSON  ·  RADAR  ·  SCOREBOARD  ·  AIRSTRIKE"),
    ("p_gungame", "GUN GAME  ·  EVERY KILL THE NEXT WEAPON"),
    ("p_end", "MATCH END  ·  XP AND LEVEL UP"),
    ("#MODES AND MAPS|Eleven modes on six maps", None),
] + [(name, caption) for name, _, _, _, caption in CINEMA] + [("#OUTRO", None)]


def frames(name):
    folder = os.path.join(RECORD, name)
    return sorted(f for f in os.listdir(folder) if f.endswith(".png")) if os.path.isdir(folder) else []


def record(session):
    url, steps, seconds, clips = SESSIONS[session]
    subprocess.run(["powershell", "-NoProfile", "-Command", "Get-Process UnrealEditor -ErrorAction SilentlyContinue | Stop-Process -Force"])
    time.sleep(2)
    for clip in clips:
        shutil.rmtree(os.path.join(RECORD, clip), ignore_errors=True)
    args = [UE, os.path.join(REPO, "DowntownHunting.uproject")] + ([url] if url else []) + [
        "-game", "-nomovie", "-windowed", f"-ResX={W}", f"-ResY={H}", "-UseFixedTimeStep", f"-FPS={FPS}",
        f"-ExecCmds=DisableAllScreenMessages, DH.Run {steps}", f"-log=Showcase_{session}.log"]
    game = subprocess.Popen(args)
    start = time.time()

    def done():
        if seconds:  # director clip: frame count reaches its length
            return len(frames(clips[-1])) >= seconds * FPS
        return os.path.exists(os.path.join(RECORD, clips[-1], "times.txt"))

    while not done() and time.time() - start < 3600 and game.poll() is None:
        time.sleep(5)
    time.sleep(3)
    game.kill()
    subprocess.run(["powershell", "-NoProfile", "-Command", "Get-Process UnrealEditor -ErrorAction SilentlyContinue | Stop-Process -Force"])
    print(f"{session}: {', '.join(f'{c} {len(frames(c))} frames' for c in clips)} in {time.time() - start:.0f} s", flush=True)


# ---- cut ----

def card_image(path, title, subtitle):
    img = Image.new("RGB", (W, H), (6, 6, 8))
    d = ImageDraw.Draw(img)
    d.text((W / 2, 500), title, font=ImageFont.truetype(IMPACT, 110), fill=(240, 232, 220), anchor="mm")
    d.rectangle([W / 2 - 160, 572, W / 2 + 160, 576], fill=ACCENT)
    if subtitle:
        d.text((W / 2, 625), subtitle, font=ImageFont.truetype(ARIAL, 36), fill=(190, 182, 170), anchor="mm")
    img.save(path)


def caption_image(path, text):
    img = Image.new("RGBA", (W, 140), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    font = ImageFont.truetype(IMPACT, 50)
    width = d.textlength(text, font=font)
    d.rectangle([80, 30, 80 + width + 56, 110], fill=(8, 8, 10, 190))
    d.rectangle([80, 30, 88, 110], fill=ACCENT + (255,))
    d.text((112, 70), text, font=font, fill=(240, 232, 220, 255), anchor="lm")
    img.save(path)


def ff(*args):
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", *args], check=True)


def encode(out, inputs, vf_extra=""):
    ff(*inputs, "-vf", f"scale={W}:{H},fps={FPS},format=yuv420p{vf_extra}", "-an", "-c:v", "libx264", "-crf", "18", "-preset", "medium", out)


def music(path, seconds, rate=44100):
    """Driving backing track: kick on the beat, offbeat hats, a bass line, slow pad. Synthesized, no licences."""
    random.seed(5)
    n = int(seconds * rate)
    out = [0.0] * n
    beat = 60 / 112
    bass = [41.2, 41.2, 49.0, 36.7]  # E1 E1 G1 D1, one per bar
    t = 0.0
    i_beat = 0
    while t < seconds:
        start = int(t * rate)
        for k in range(int(0.25 * rate)):  # kick
            if start + k >= n:
                break
            x = k / rate
            out[start + k] += 0.55 * math.sin(2 * math.pi * (50 + 90 * math.exp(-x * 30)) * x) * math.exp(-x * 9)
        hat = int((t + beat / 2) * rate)
        for k in range(int(0.05 * rate)):  # hi-hat
            if hat + k >= n:
                break
            out[hat + k] += 0.06 * random.uniform(-1, 1) * math.exp(-k / rate * 70)
        f = bass[(i_beat // 4) % len(bass)]
        for k in range(int(beat * rate)):  # bass, pulsing
            if start + k >= n:
                break
            x = k / rate
            out[start + k] += 0.22 * math.sin(2 * math.pi * f * x) * min(1.0, x * 40) * math.exp(-x * 2.5)
        t += beat
        i_beat += 1
    for k in range(n):  # pad
        x = k / rate
        out[k] += 0.05 * (math.sin(2 * math.pi * 164.8 * x) + math.sin(2 * math.pi * 196 * x)) * (0.6 + 0.4 * math.sin(2 * math.pi * 0.1 * x))
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(b"".join(struct.pack("<h", int(max(-1.0, min(1.0, v)) * 30000)) for v in out))


def cut():
    tmp = os.path.join(OUT_DIR, "tmp")
    os.makedirs(tmp, exist_ok=True)
    parts = []
    intro_seconds = 0.0
    for index, (clip, caption) in enumerate(CUT):
        out = os.path.join(tmp, f"{index:02d}.mp4")
        if clip == "#INTRO":
            intro = os.path.join(REPO, "Content", "Movies", "DH_Intro.mp4")
            encode(out, ["-i", intro])
            intro_seconds = float(subprocess.run(["ffprobe", "-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0", out],
                                                 capture_output=True, text=True).stdout)
        elif clip.startswith("#"):
            title, _, subtitle = clip[1:].partition("|")
            png = os.path.join(tmp, f"{index:02d}.png")
            if title == "OUTRO":
                outro = Image.new("RGB", (W, H), (6, 6, 8))
                logo = Image.open(os.path.join(HERE, "T_DH_Logo.png")).convert("RGBA")
                logo = logo.resize((1100, int(logo.height * 1100 / logo.width)), Image.LANCZOS)
                outro.paste(logo, ((W - logo.width) // 2, 300), logo)
                d = ImageDraw.Draw(outro)
                d.text((W / 2, 820), "PLAY ONLINE WITH YOUR FRIENDS", font=ImageFont.truetype(IMPACT, 54), fill=(240, 232, 220), anchor="mm")
                d.text((W / 2, 890), "A GILDE GAMES PRODUCTION", font=ImageFont.truetype(ARIAL, 30), fill=(190, 182, 170), anchor="mm")
                outro.save(png)
                seconds = 5
            else:
                card_image(png, title, subtitle)
                seconds = 2.2
            encode(out, ["-loop", "1", "-framerate", str(FPS), "-t", str(seconds), "-i", png],
                   f",fade=t=in:st=0:d=0.4,fade=t=out:st={seconds - 0.4}:d=0.4")
        else:
            if not frames(clip):
                print("missing clip", clip)
                continue
            seconds = len(frames(clip)) / FPS
            cap = os.path.join(tmp, f"{index:02d}_cap.png")
            caption_image(cap, caption)
            ff("-framerate", str(FPS), "-i", os.path.join(RECORD, clip, "%04d.png"),
               "-loop", "1", "-framerate", str(FPS), "-t", str(seconds), "-i", cap,
               "-filter_complex",
               f"[0:v]scale={W}:{H},fps={FPS},fade=t=in:st=0:d=0.3,fade=t=out:st={seconds - 0.3}:d=0.3[v];"
               f"[1:v]format=rgba,fade=t=in:st=0.3:d=0.4:alpha=1,fade=t=out:st={seconds - 0.7}:d=0.4:alpha=1[c];"
               f"[v][c]overlay=x=0:y=H-200,format=yuv420p",
               "-an", "-c:v", "libx264", "-crf", "18", "-preset", "medium", out)
        parts.append(out)
    listing = os.path.join(tmp, "parts.txt")
    with open(listing, "w") as f:
        f.writelines(f"file '{p}'\n" for p in parts)
    video = os.path.join(tmp, "video.mp4")
    ff("-f", "concat", "-safe", "0", "-i", listing, "-c", "copy", video)
    total = float(subprocess.run(["ffprobe", "-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0", video],
                                 capture_output=True, text=True).stdout)
    track = os.path.join(tmp, "music.wav")
    music(track, total - intro_seconds + 1)
    out = os.path.join(OUT_DIR, "DH_Showcase.mp4")
    # The intro keeps its own sound, the music starts after it.
    ff("-i", video, "-i", os.path.join(REPO, "Content", "Movies", "DH_Intro.mp4"), "-i", track,
       "-filter_complex",
       f"[1:a]atrim=0:{intro_seconds}[ia];[2:a]afade=t=in:st=0:d=1,afade=t=out:st={total - intro_seconds - 3}:d=3,volume=0.9[m];"
       "[ia][m]concat=n=2:v=0:a=1[a]",
       "-map", "0:v", "-map", "[a]", "-c:v", "copy", "-c:a", "aac", "-b:a", "192k", "-shortest", "-movflags", "+faststart", out)
    print(out, f"{total:.1f} s", os.path.getsize(out) // (1024 * 1024), "MB")


if __name__ == "__main__":
    if len(sys.argv) > 2 and sys.argv[1] == "rec":
        record(sys.argv[2])
    elif sys.argv[1:] != ["cut"]:
        for session, (_, _, _, clips) in SESSIONS.items():
            if not all(frames(c) for c in clips):
                record(session)
    if sys.argv[1:2] != ["rec"]:
        cut()
