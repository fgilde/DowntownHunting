"""Character select splash art (arcade comic style) from the portraits, with a local MuseForge (http://localhost:7861,
Docker container "museforge"): Flux 2 Klein image edit with the portrait as reference.
Writes Tools/Branding/Splash/T_DH_Splash_<Id>.png (768x1024). Usage: python make_splash_art.py [Id ...]"""
import json
import os
import subprocess
import sys
import time
import urllib.request

API = 'http://localhost:7861/api/v1'
WORKSPACE = 'tmp-test'  # MuseForge output folder, kept apart from the user's own work
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, 'Splash')

# Id: (pose and weapon, two background colours)
CHARACTERS = {
    'Dex': ('resting a pump shotgun on his shoulder, smirking', 'electric cyan and hot magenta'),
    'Hunter': ('aiming a pistol sideways at the viewer, cold stare', 'neon yellow and blood red'),
    'Huntress': ('holding a scoped sniper rifle upright, lowering her sunglasses with one finger', 'hot magenta and violet'),
    'Jade': ('spinning a pistol on her finger, playful wink', 'lime green and purple'),
    'Jonah': ('holding a submachine gun at the ready, calm veteran look', 'orange and teal'),
    'Kim': ('crouching with an assault rifle, fierce grin', 'neon yellow and electric blue'),
    'Lou': ('loading a magazine into an assault rifle, focused', 'cyan and orange'),
    'Mara': ('holding an assault rifle across her chest, confident cocky smile', 'neon yellow and hot magenta'),
    'Marcus': ('holding a heavy shotgun with both hands, shouting a battle cry', 'red and neon yellow'),
    'Nina': ('holding two pistols crossed in front of her, cool expression', 'electric blue and hot pink'),
    'Ray': ('holding a grenade in his raised hand and a rifle in the other, laughing', 'lime green and orange'),
    'Rosa': ('aiming a submachine gun past the viewer, determined', 'magenta and cyan'),
    'Sal': ('adjusting his tie with a pistol in the other hand, sly smile', 'gold and deep purple'),
    'Viktor': ('resting a sniper rifle on his shoulders behind his neck, confident', 'teal and neon yellow'),
}
WOMEN = {'Huntress', 'Jade', 'Kim', 'Lou', 'Mara', 'Nina', 'Rosa'}
STYLE = ('Stylized arcade hero shooter splash art of this exact {person} (same face, same hair, same skin tone, same '
         'clothes), waist-up, {pose}, cel-shaded comic illustration with bold black ink outlines, vivid {colours} '
         'graphic background with halftone dots, spray paint splatter and diagonal stripes, dramatic rim light, '
         'energetic, video game character select art, full bleed artwork without borders or frame')
RANDOM = ('Stylized arcade hero shooter splash art of a mysterious hooded hunter in full shadow, only a black '
          'silhouette, a huge glowing neon yellow question mark painted behind them, cel-shaded comic illustration '
          'with bold black ink outlines, vivid neon yellow and hot magenta graphic background with halftone dots, '
          'spray paint splatter and diagonal stripes, video game character select art')


def call(path, body=None, files=None):
    if files:
        out = subprocess.run(['curl', '-s', '-F', 'file=@' + files, API + path], capture_output=True, text=True).stdout
        return json.loads(out)
    data = json.dumps(body).encode() if body is not None else None
    request = urllib.request.Request(API + path, data, {'Content-Type': 'application/json'})
    return json.load(urllib.request.urlopen(request))


def generate(prompt, params):
    job = call('/generate', dict(params, model_type='flux2_klein_9b', prompt=prompt, workspace=WORKSPACE))['job_id']
    while True:
        status = call('/status/' + job)
        if status['status'] in ('completed', 'failed'):
            break
        time.sleep(3)
    if status['status'] != 'completed':
        raise RuntimeError(status.get('message'))
    return status['output_files'][0]


def fetch(name, target, trim_borders=True):
    # The API does not serve the files; copy from the container and take the single frame.
    clip = target + '.mp4'
    subprocess.run(['docker', 'cp', 'museforge:/workspace/app/outputs/%s/%s' % (WORKSPACE, name), clip], check=True)
    subprocess.run(['ffmpeg', '-loglevel', 'error', '-y', '-i', clip, '-frames:v', '1', target], check=True)
    os.remove(clip)
    if trim_borders:
        trim(target)


def trim(path):
    # Flux sometimes letterboxes the picture: cut near-black border rows/columns, scale back to the full size.
    from PIL import Image
    image = Image.open(path).convert('RGB')
    grey = image.convert('L')
    w, h = image.size
    rows = [y for y in range(h) if max(grey.crop((0, y, w, y + 1)).getdata()) > 40]
    cols = [x for x in range(w) if max(grey.crop((x, 0, x + 1, h)).getdata()) > 40]
    box = (cols[0], rows[0], cols[-1] + 1, rows[-1] + 1)
    if box != (0, 0, w, h):
        image.crop(box).resize((w, h), Image.LANCZOS).save(path)


def main(ids):
    os.makedirs(OUT, exist_ok=True)
    for id in ids:
        target = os.path.join(OUT, 'T_DH_Splash_%s.png' % id)
        if id == 'Random':
            name = generate(RANDOM, {'resolution': '768x1024'})
        else:
            pose, colours = CHARACTERS[id]
            portrait = os.path.join(HERE, 'Portraits', 'T_DH_Portrait_%s.png' % id)
            ref = call('/upload', files=portrait)['path']
            name = generate(STYLE.format(pose=pose, colours=colours, person='woman' if id in WOMEN else 'man'),
                            {'resolution': '768x1024', 'image_refs': [ref], 'video_prompt_type': 'KI'})
        fetch(name, target)
        print('done', target, flush=True)


if __name__ == '__main__':
    main(sys.argv[1:] or list(CHARACTERS) + ['Random'])
