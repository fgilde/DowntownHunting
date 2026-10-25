"""Music with a local MuseForge (http://localhost:7861, ACE-Step 1.5 with its 4B LM): menu loops and match cues, written to
Tools/Branding/Music/DH_Music_<Id>.wav (loudness normalised, 44.1 kHz stereo, short fades so the loop point is quiet).
Usage: python make_music.py [Id ...]"""
import json
import os
import subprocess
import sys
import time
import urllib.request

API = 'http://localhost:7861/api/v1'
WORKSPACE = 'tmp-test'  # MuseForge output folder, kept apart from the user's own work
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'Music')
MODEL = 'ace_step_v1_5_turbo_lm_4b'  # with the 4B planning LM: real song structure, hooks, builds and drops

# Id: (seconds, style). Menu tracks loop in the main menu; the match ones play in warmup, the last minute and at the end.
TRACKS = {
    'Menu1': (110, 'high energy K-pop dance anthem, punchy four-on-the-floor kick, bright supersaw synth hook, catchy vocal '
                   'chops, hand claps, big build up and euphoric drop, 128 bpm, glossy modern production, instrumental'),
    'Menu2': (110, 'girl crush K-pop banger, hard-hitting trap drums and booming 808, brass stabs, swagger, catchy synth lead, '
                   'sudden beat switch, 140 bpm, glossy modern production, instrumental'),
    'Menu3': (110, 'upbeat K-pop electro pop, funky slap bass, sparkling synths, groovy tight drums, catchy whistle hook, '
                   '120 bpm, feel-good party energy, glossy modern production, instrumental'),
    'Warmup': (40, 'tense pre-battle countdown music, pulsing synth bass, ticking percussion, rising risers and drums '
                   'building tension, cinematic, 120 bpm, instrumental'),
    'LastMinute': (70, 'frantic final minute action music, racing drums, alarm-like synth stabs, distorted bass, rising '
                       'intensity, chase scene, 165 bpm, instrumental'),
    'Victory': (14, 'short triumphant victory fanfare, heroic brass and big drums, punchy ending, instrumental'),
    'Defeat': (12, 'short dramatic game over sting for an action game, heavy distorted bass hit, falling synth, loud low brass '
                   'and big drums, punchy, instrumental'),
}


def call(method, path, body=None):
    data = json.dumps(body).encode() if body is not None else None
    request = urllib.request.Request(API + path, data, {'Content-Type': 'application/json'}, method=method)
    return json.load(urllib.request.urlopen(request))


def quiet_share(path):
    """Share of 2 s windows far quieter than the track's typical level, or silent (a track that falls apart halfway
    is no use for a loop)."""
    out = subprocess.run(['ffmpeg', '-i', path, '-af', 'asetnsamples=n=88200,astats=metadata=1:reset=1,ametadata=print:key=lavfi.astats.Overall.RMS_level',
                          '-f', 'null', '-'], capture_output=True, text=True).stderr
    levels = [float(line.split('=')[1]) for line in out.splitlines() if 'RMS_level=' in line and '-inf' not in line]
    typical = sorted(levels)[len(levels) // 2] if levels else -100.0
    return sum(1 for level in levels if level < max(-40.0, typical - 12.0) or level < -40.0) / max(1, len(levels))


def main(ids):
    os.makedirs(OUT, exist_ok=True)
    for id in ids:
        seconds, style = TRACKS[id]
        raw = os.path.join(OUT, id + '.raw.wav')
        for attempt in range(4):
            job = call('POST', '/generate', {'model_type': MODEL, 'prompt': '[instrumental]', 'alt_prompt': style,
                                             'duration_seconds': seconds, 'workspace': WORKSPACE})['job_id']
            while True:
                status = call('GET', '/status/' + job)
                if status['status'] in ('completed', 'failed'):
                    break
                time.sleep(3)
            if status['status'] != 'completed':
                raise RuntimeError(status.get('message'))
            subprocess.run(['docker', 'cp', 'museforge:/workspace/app/outputs/%s/%s' % (WORKSPACE, status['output_files'][0]), raw], check=True)
            share = quiet_share(raw)
            if share < (0.15 if seconds >= 40 else 0.6):  # stings may ring out
                break
            print('too quiet (%.0f %%), again: %s' % (share * 100, id), flush=True)
        target = os.path.join(OUT, 'DH_Music_%s.wav' % id)
        subprocess.run(['ffmpeg', '-loglevel', 'error', '-y', '-i', raw, '-af',
                        'loudnorm=I=-14:TP=-1.0,aresample=44100,afade=t=in:d=0.5,areverse,afade=t=in:d=2,areverse',
                        '-ac', '2', target], check=True)
        os.remove(raw)
        print('done', target, flush=True)


if __name__ == '__main__':
    main(sys.argv[1:] or list(TRACKS))
