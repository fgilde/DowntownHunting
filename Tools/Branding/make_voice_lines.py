"""Character select voice lines with a local MuseForge (http://localhost:7861, Docker container "museforge"): one designed
voice per character (Qwen3 TTS Voice Design), one line each. Writes Tools/Branding/Voices/DH_Voice_<Id>.wav (loudness
normalised). Usage: python make_voice_lines.py [Id ...]"""
import json
import os
import subprocess
import sys
import time
import urllib.request

API = 'http://localhost:7861/api/v1'
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


def voice(id, description):
    name = 'DH ' + id
    for existing in call('GET', '/voices')['voices']:
        if existing['name'] == name:
            return existing['id']
    created = call('POST', '/voices', {'name': name, 'model_type': 'qwen3_tts_voicedesign', 'language': 'English',
                                       'description': description, 'params': {'voice_description': description}})
    call('PUT', '/voices/' + created['id'], {'params': {'voice_description': description}})
    return created['id']


def main(ids):
    os.makedirs(OUT, exist_ok=True)
    for id in ids:
        description, line = LINES[id]
        job = call('POST', '/voices/%s/speak' % voice(id, description), {'text': line})['job_id']
        while True:
            status = call('GET', '/status/' + job)
            if status['status'] in ('completed', 'failed'):
                break
            time.sleep(2)
        if status['status'] != 'completed':
            raise RuntimeError(status.get('message'))
        raw = os.path.join(OUT, id + '.raw.wav')
        subprocess.run(['docker', 'cp', 'museforge:/workspace/app/outputs/tmp-test/' + status['output_files'][0], raw], check=True)
        target = os.path.join(OUT, 'DH_Voice_%s.wav' % id)
        subprocess.run(['ffmpeg', '-loglevel', 'error', '-y', '-i', raw, '-af', 'loudnorm=I=-16:TP=-1.5,aresample=44100',
                        '-ac', '1', target], check=True)
        os.remove(raw)
        print('done', target, flush=True)


if __name__ == '__main__':
    main(sys.argv[1:] or list(LINES))
