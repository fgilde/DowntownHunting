"""Menu music with a local MuseForge (http://localhost:7861, ACE-Step 1.5): instrumental loops written to
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
SECONDS = 96

TRACKS = {
    'Menu1': 'high energy arcade video game main menu theme, punchy electro rock, driving live drums, 128 bpm, '
             'distorted synth bass, heroic saw synth lead, electric guitar riffs, hype, instrumental, no vocals',
    'Menu2': 'aggressive neon cyberpunk drum and bass, 174 bpm, gritty reese bass, energetic breakbeat, '
             'arpeggiated synths, video game soundtrack, instrumental, no vocals',
    'Menu3': 'swaggering trap hip hop beat for an action game, heavy 808 bass, brass stabs, hard snare rolls, '
             'street attitude, 140 bpm, instrumental, no vocals',
}


def call(method, path, body=None):
    data = json.dumps(body).encode() if body is not None else None
    request = urllib.request.Request(API + path, data, {'Content-Type': 'application/json'}, method=method)
    return json.load(urllib.request.urlopen(request))


def main(ids):
    os.makedirs(OUT, exist_ok=True)
    for id in ids:
        job = call('POST', '/generate', {'model_type': 'ace_step_v1_5', 'prompt': '[instrumental]', 'alt_prompt': TRACKS[id],
                                         'duration_seconds': SECONDS, 'workspace': WORKSPACE})['job_id']
        while True:
            status = call('GET', '/status/' + job)
            if status['status'] in ('completed', 'failed'):
                break
            time.sleep(3)
        if status['status'] != 'completed':
            raise RuntimeError(status.get('message'))
        raw = os.path.join(OUT, id + '.raw.wav')
        subprocess.run(['docker', 'cp', 'museforge:/workspace/app/outputs/%s/%s' % (WORKSPACE, status['output_files'][0]), raw], check=True)
        target = os.path.join(OUT, 'DH_Music_%s.wav' % id)
        subprocess.run(['ffmpeg', '-loglevel', 'error', '-y', '-i', raw, '-af',
                        'loudnorm=I=-14:TP=-1.0,aresample=44100,afade=t=in:d=0.5,areverse,afade=t=in:d=2,areverse',
                        '-ac', '2', target], check=True)
        os.remove(raw)
        print('done', target, flush=True)


if __name__ == '__main__':
    main(sys.argv[1:] or list(TRACKS))
