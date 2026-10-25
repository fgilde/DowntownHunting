"""Match announcer with a local MuseForge (http://localhost:7861): one voice designed once (Qwen3 Voice Design, a reference
sentence), then every line spoken in that voice (IndexTTS2 voice cloning), so all lines sound like the same person.
Writes Tools/Branding/Announcer/DH_Ann_<Id>.wav (loudness normalised, short silence at the end).
Usage: python make_announcer.py [Id ...]   (ref: only the reference voice)"""
import difflib
import json
import os
import subprocess
import sys
import time
import urllib.request

from make_voice_lines import tail_level

API = 'http://localhost:7861/api/v1'
WORKSPACE = 'tmp-test'  # MuseForge output folder, kept apart from the user's own work
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'Announcer')
REFERENCE = os.path.join(OUT, 'reference.wav')

VOICE = ('Male military shooter game announcer in his forties, deep powerful commanding voice, clear American accent, '
         'slightly gritty radio tone, intense and energetic, speaks fast and punchy')
REFERENCE_TEXT = ('Attention, hunters! The match is live. Capture the objectives, hold your ground, and take out the enemy '
                  'team. Move, move, move!')

# Id: line. "Our"/"Enemy" variants: the game picks by the listener's team.
LINES = {
    'MatchStart': 'The hunt is on!',
    'Warmup': 'Get ready. The match starts in a moment.',
    'OneMinute': 'One minute remaining!',
    'Victory': 'Victory!',
    'Defeat': 'Defeat.',
    'LeadTaken': "We've taken the lead!",
    'LeadLost': "We've lost the lead!",
    'FirstBlood': 'First blood!',
    'DoubleKill': 'Double kill!',
    'TripleKill': 'Triple kill!',
    'MultiKill': 'Unstoppable!',
    'Headshot': 'Headshot!',
    'RoundWon': 'Round won!',
    'RoundLost': 'Round lost.',
    'NewRound': 'New round. Get ready!',
    'LastAlive': "You're the last one standing!",
    'OurCaptureA': 'We captured Alpha!', 'OurCaptureB': 'We captured Bravo!', 'OurCaptureC': 'We captured Charlie!',
    'EnemyCaptureA': "We've lost Alpha!", 'EnemyCaptureB': "We've lost Bravo!", 'EnemyCaptureC': "We've lost Charlie!",
    'BombPlantedAttack': 'Bomb planted! Defend it!',
    'BombPlantedDefend': 'The bomb has been planted! Defuse it!',
    'BombDefused': 'Bomb defused!',
    'BombExploded': 'Target destroyed!',
    'OurFlagTaken': 'Our flag has been taken!',
    'EnemyFlagTaken': "We've got their flag! Bring it home!",
    'OurFlagCaptured': 'We captured the flag!',
    'EnemyFlagCaptured': 'The enemy captured our flag!',
    'FlagReturned': 'Flag returned.',
    'HillMoved': 'The hill has moved!',
    'HillOurs': 'We control the hill!',
    'HillEnemy': 'The enemy controls the hill!',
    'Radar': 'Radar online!',
    'Airstrike': 'Airstrike inbound!',
    'EnemyAirstrike': 'Enemy airstrike! Take cover!',
    'HunterMode': 'Hunter mode activated!',
    'Infected': "You've been infected! Hunt them down!",
    'LastSurvivor': "You're the last survivor! Stay alive!",
    'FinalWeapon': 'Final weapon!',
}


_whisper = None


def heard_right(path, text):
    """Speech recognition (faster-whisper, if installed) agrees with the line: catches mumbled or swallowed words."""
    global _whisper
    try:
        from faster_whisper import WhisperModel
    except ImportError:
        return True
    _whisper = _whisper or WhisperModel('base.en', device='cpu', compute_type='int8')
    words = lambda t: ''.join(c for c in t.lower() if c.isalpha() or c == ' ').split()
    heard = words(' '.join(s.text for s in _whisper.transcribe(path)[0]))
    return difflib.SequenceMatcher(None, words(text), heard).ratio() >= 0.75


def call(method, path, body=None):
    data = json.dumps(body).encode() if body is not None else None
    request = urllib.request.Request(API + path, data, {'Content-Type': 'application/json'}, method=method)
    return json.load(urllib.request.urlopen(request))


def run(body):
    job = call('POST', '/generate', dict(body, workspace=WORKSPACE))['job_id']
    while True:
        status = call('GET', '/status/' + job)
        if status['status'] in ('completed', 'failed'):
            break
        time.sleep(2)
    if status['status'] != 'completed':
        raise RuntimeError(status.get('message'))
    return status['output_files'][0]


def fetch(name, target):
    subprocess.run(['docker', 'cp', 'museforge:/workspace/app/outputs/%s/%s' % (WORKSPACE, name), target], check=True)


def upload(path):
    out = subprocess.run(['curl', '-s', '-F', 'file=@' + path, API + '/upload'], capture_output=True, text=True).stdout
    return json.loads(out)['path']


def make_reference():
    for _ in range(4):
        fetch(run({'model_type': 'qwen3_tts_voicedesign', 'prompt': REFERENCE_TEXT, 'alt_prompt': VOICE,
                   'model_mode': 'english', 'duration_seconds': 30}), REFERENCE)
        if tail_level(REFERENCE) < 0.05:
            return
    raise RuntimeError('reference voice keeps getting cut off')


def main(ids):
    os.makedirs(OUT, exist_ok=True)
    if not os.path.exists(REFERENCE) or ids == ['ref']:
        make_reference()
        print('reference', REFERENCE, flush=True)
        if ids == ['ref']:
            return
    reference = upload(REFERENCE)
    for id in ids:
        raw = os.path.join(OUT, id + '.raw.wav')
        for _ in range(4):
            fetch(run({'model_type': 'index_tts2', 'prompt': LINES[id], 'audio_prompt_type': 'A', 'audio_guide': reference,
                       'duration_seconds': 15}), raw)
            if tail_level(raw) < 0.05 and heard_right(raw, LINES[id]):
                break
            print('again:', id, flush=True)
        target = os.path.join(OUT, 'DH_Ann_%s.wav' % id)
        # Radio punch: a little high-pass and compression, then loudness and 0.25 s of air at the end.
        subprocess.run(['ffmpeg', '-loglevel', 'error', '-y', '-i', raw, '-af',
                        'highpass=f=90,acompressor=threshold=-18dB:ratio=3,loudnorm=I=-14:TP=-1,aresample=44100,'
                        'silenceremove=start_periods=1:start_threshold=-45dB,apad=pad_dur=0.25', '-ac', '1', target], check=True)
        os.remove(raw)
        print('done', target, flush=True)


if __name__ == '__main__':
    main(sys.argv[1:] or list(LINES))
