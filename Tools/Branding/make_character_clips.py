"""Character select clips with a local MuseForge (LTX-2 image to video from the splash art), played forward and back as a
seamless loop: Content/Movies/Characters/DH_Char_<Id>.mp4. Keep weapons still in the prompts: moving guns come out
warped. Usage: python make_character_clips.py <Id> ..."""
import json, os, subprocess, sys, time, urllib.request
API = 'http://localhost:7861/api/v1'
WORKSPACE = 'tmp-test'  # MuseForge output folder, kept apart from the user's own work
HERE = os.path.dirname(os.path.abspath(__file__))
SPLASH = os.path.join(HERE, 'Splash')
OUT = os.path.join(HERE, '..', '..', 'Content', 'Movies', 'Characters')
TMP = os.environ.get('TEMP', HERE)
END = ' Bold cel-shaded comic style, black ink outlines, static camera, slow subtle push in.'
ACTIONS = {
 'Jade': 'The young woman winks playfully, tilts her head and laughs, holding her pistol still and upright beside her face, the gun does not move. The lime green and purple background pulses.',
 'Kim': 'The crouching woman springs up from her crouch, swings her assault rifle into position and grins fiercely at the camera. The yellow and blue striped background pulses.',
 'Lou': 'The woman slaps a magazine into her assault rifle, racks it and looks up at the camera with a focused stare. The cyan and orange background pulses.',
 'Marcus': 'The big man roars a battle cry, pumps his heavy shotgun and leans toward the camera. The red and yellow burst background pulses.',
 'Nina': 'The woman uncrosses her two pistols and points them both toward the camera with a cool unimpressed look, her short hair moving slightly. The blue and pink background pulses.',
 'Random': 'The shadowy hooded silhouette slowly turns its head toward the camera, the giant neon yellow question mark behind it flickers and glows brighter, paint splatters shimmer.',
 'Ray': 'The laughing bald man tosses the grenade up and catches it, laughing loudly. The lime and orange background pulses.',
 'Rosa': 'The determined woman fires a short burst from her submachine gun past the camera, muzzle flashes and shell casings flying, then lowers it with a hard look. The magenta and cyan background pulses.',
 'Sal': 'The man in the suit straightens his tie, smooths his jacket, then raises his pistol beside his face with a sly smile. The gold and purple background pulses.',
 'Viktor': 'The bearded man swings the sniper rifle down from behind his neck, shoulders it and looks through the scope toward the camera, then smirks. The teal and yellow background pulses.',
}

def post(path, body):
    r = urllib.request.Request(API + path, json.dumps(body).encode(), {'Content-Type': 'application/json'})
    return json.load(urllib.request.urlopen(r))

for id in sys.argv[1:] or ACTIONS:
    ref = json.loads(subprocess.run(['curl', '-s', '-F', 'file=@' + os.path.join(SPLASH, 'T_DH_Splash_%s.png' % id), API + '/upload'],
                                    capture_output=True, text=True).stdout)['path']
    job = post('/generate', {'model_type': 'ltx2_22B_distilled_1_1', 'prompt': 'The comic book illustration comes alive. ' + ACTIONS[id] + END,
                             'resolution': '576x768', 'video_length': 97, 'image_start': ref, 'image_prompt_type': 'S', 'workspace': WORKSPACE})['job_id']
    while True:
        status = json.load(urllib.request.urlopen(API + '/status/' + job))
        if status['status'] in ('completed', 'failed'):
            break
        time.sleep(5)
    if status['status'] != 'completed':
        print('failed', id, status.get('message'), flush=True)
        continue
    raw = os.path.join(TMP, id + '_raw.mp4')
    subprocess.run(['docker', 'cp', 'museforge:/workspace/app/outputs/%s/%s' % (WORKSPACE, status['output_files'][0]), raw], check=True)
    subprocess.run(['ffmpeg', '-loglevel', 'error', '-y', '-i', raw, '-filter_complex',
                    '[0:v]split[a][b];[b]reverse[r];[a][r]concat=n=2:v=1:a=0,format=yuv420p[v]', '-map', '[v]', '-an',
                    '-c:v', 'libx264', '-crf', '20', '-movflags', '+faststart', os.path.join(OUT, 'DH_Char_%s.mp4' % id)], check=True)
    print('done', id, flush=True)
