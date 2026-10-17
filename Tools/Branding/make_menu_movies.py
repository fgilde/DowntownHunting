"""Main menu backgrounds, step 2: Saved/MenuFrames (from Tools/Python/make_menu_backgrounds.py) -> one seamless 12 s
loop per menu entry, Content/Movies/Menu/DH_Menu_<Name>.mp4 (played by UDHMainMenu). Each shot is rendered 13.5 s long;
its last 1.5 s fade into its first 1.5 s, so the loop point does not jump. Needs ffmpeg on PATH."""
import os
import subprocess

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
FRAMES = os.path.join(REPO, "Saved", "MenuFrames")
OUT_DIR = os.path.join(REPO, "Content", "Movies", "Menu")
NAMES = ["Play", "Character", "Loadout", "Options", "Credits", "Quit"]  # order of SHOTS in make_menu_backgrounds.py
FPS, LOOP, BLEND = 30, 12.0, 1.5
SHOT_FRAMES = int(FPS * (LOOP + BLEND))

os.makedirs(OUT_DIR, exist_ok=True)
for index, name in enumerate(NAMES):
    out = os.path.join(OUT_DIR, "DH_Menu_%s.mp4" % name)
    graph = (
        "[0:v]split=3[a][b][c];"
        f"[a]trim=start=0:end={BLEND},setpts=PTS-STARTPTS[head];"
        f"[b]trim=start={BLEND}:end={LOOP},setpts=PTS-STARTPTS[body];"
        f"[c]trim=start={LOOP}:end={LOOP + BLEND},setpts=PTS-STARTPTS[tail];"
        f"[tail][head]xfade=transition=fade:duration={BLEND}:offset=0[seam];"
        "[seam][body]concat=n=2:v=1:a=0,format=yuv420p[v]"
    )
    subprocess.run([
        "ffmpeg", "-y", "-loglevel", "error",
        "-framerate", str(FPS), "-start_number", str(index * SHOT_FRAMES), "-i", os.path.join(FRAMES, "%04d.png"),
        "-filter_complex", graph, "-map", "[v]",
        "-c:v", "libx264", "-crf", "21", "-preset", "slow", "-g", str(FPS), "-movflags", "+faststart", "-an", out,
    ], check=True)
    print(out, os.path.getsize(out) // 1024, "KB")
