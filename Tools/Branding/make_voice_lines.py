"""Character select voice lines with a local MuseForge (http://localhost:7861, Docker container "museforge"): one designed
voice per character (Qwen3 TTS Voice Design, described anew for every line), one line each. Writes Tools/Branding/Voices/DH_Voice_<Id>.wav (loudness
normalised). Usage: python make_voice_lines.py [Id ...]"""
import array
import json
import math
import os
import subprocess
import sys
import time
import urllib.request
import wave

API = 'http://localhost:7861/api/v1'
WORKSPACE = 'tmp-test'  # MuseForge output folder, kept apart from the user's own work
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'Voices')

LINES = {
    'Dex': ('Young man, laid-back drawl, cheeky and amused', "Relax. I brought the big gun."),
    'Hunter': ('Man in his forties, deep, cold and calm, stoic professional killer', 'Target acquired. Nobody walks away.'),
    'Huntress': ('Woman in her thirties, cool sultry low voice, confident and slightly menacing', "You won't even see me coming."),
    'Jade': ('Bubbly energetic young woman, playful and excited', "Ooh, this is gonna be fun!"),
    'Jonah': ('Older man in his sixties, deep gravelly voice, calm veteran authority', 'Stay sharp. This city bites back.'),
    'Kim': ('Fierce young woman, energetic and aggressive, shouting with a grin', "Let's light 'em up!"),
    'Lou': ('Woman in her thirties, focused, calm and crisp professional', "Locked and loaded. Let's work."),
    'Mara': ('Confident young woman in her late twenties, smooth low alto voice, cocky and playful, action movie heroine',
             'Downtown is my hunting ground. Try to keep up.'),
    'Marcus': ('Big man with a loud booming voice, battle hungry, roaring', 'Bring it on! All of you!'),
    'Nina': ('Cool detached young woman, quiet confidence, almost whispering', 'Two guns. Zero mercy.'),
    'Ray': ('Cheerful middle-aged man, mischievous, laughing', 'Fire in the hole! Ha ha!'),
    'Rosa': ('Determined serious woman, sharp and commanding', 'Move fast. Hit hard.'),
    'Sal': ('Smooth sly middle-aged man, mobster charm, relaxed', 'Nothing personal. Just business.'),
    'Viktor': ('Man in his thirties with a strong Eastern European accent, deep and confident', 'One shot. One kill.'),
    'Random': ('Hyped arcade game announcer, energetic male voice, big and dramatic', 'Feeling lucky?'),
}


def call(method, path, body=None):
    data = json.dumps(body).encode() if body is not None else None
    request = urllib.request.Request(API + path, data, {'Content-Type': 'application/json'}, method=method)
    return json.load(urllib.request.urlopen(request))


def tail_level(path):
    """Loudness of the last 0.1 s against the loudest 0.05 s: high means the line was cut off mid-word."""
    with wave.open(path) as w:
        rate, channels = w.getframerate(), w.getnchannels()
        samples = array.array('h', w.readframes(w.getnframes()))[::channels]
    rms = lambda part: math.sqrt(sum(x * x for x in part) / max(1, len(part)))
    peak = max(rms(samples[i:i + rate // 20]) for i in range(0, len(samples), rate // 20))
    return rms(samples[-rate // 10:]) / max(1.0, peak)


def generate(description, line):
    # Plain /generate: the voice library's /speak cut lines off after about two seconds.
    job = call('POST', '/generate', {'model_type': 'qwen3_tts_voicedesign', 'prompt': line, 'alt_prompt': description,
                                     'model_mode': 'english', 'duration_seconds': 30, 'workspace': WORKSPACE})['job_id']
    while True:
        status = call('GET', '/status/' + job)
        if status['status'] in ('completed', 'failed'):
            break
        time.sleep(2)
    if status['status'] != 'completed':
        raise RuntimeError(status.get('message'))
    return status['output_files'][0]


def main(ids):
    os.makedirs(OUT, exist_ok=True)
    for id in ids:
        description, line = LINES[id]
        for attempt in range(4):
            name = generate(description, line)
            raw = os.path.join(OUT, id + '.raw.wav')
            subprocess.run(['docker', 'cp', 'museforge:/workspace/app/outputs/%s/%s' % (WORKSPACE, name), raw], check=True)
            if tail_level(raw) < 0.05:
                break
            print('cut off, again:', id, flush=True)
        target = os.path.join(OUT, 'DH_Voice_%s.wav' % id)
        # 0.3 s of silence at the end, so nothing clicks when the sound stops
        subprocess.run(['ffmpeg', '-loglevel', 'error', '-y', '-i', raw, '-af',
                        'loudnorm=I=-16:TP=-1.5,aresample=44100,apad=pad_dur=0.3', '-ac', '1', target], check=True)
        os.remove(raw)
        print('done', target, flush=True)


if __name__ == '__main__':
    main(sys.argv[1:] or list(LINES))
