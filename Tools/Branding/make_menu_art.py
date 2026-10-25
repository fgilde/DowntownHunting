"""Main menu showcase art (arcade comic style) with a local MuseForge (Flux 2 Klein, text to image): one picture per
loadout weapon and one for the controller entry (T_DH_Art_<Id>.png, 1024x576), plus ability icons, in
Tools/Branding/MenuArt.
Usage: python make_menu_art.py [Id ...]"""
import os
import sys

from make_splash_art import fetch, generate

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'MenuArt')

SUBJECTS = {
    'Rifle': ('a sleek futuristic sci-fi assault rifle with glowing accents', 'neon yellow and hot magenta'),
    'Shotgun': ('a chunky futuristic sci-fi combat shotgun with a big pump grip', 'red and electric cyan'),
    'Pistol': ('a compact futuristic sci-fi handgun', 'lime green and purple'),
    'AK47': ('an AK-47 assault rifle with wooden stock and curved magazine', 'orange and teal'),
    'MP5': ('an MP5 submachine gun', 'electric blue and hot pink'),
    'Sniper': ('a bolt-action sniper rifle with a long scope', 'teal and neon yellow'),
    'M1911': ('a classic M1911 pistol with wooden grips', 'gold and deep purple'),
    'Controller': ('a modern game controller (gamepad) with glowing buttons, seen from the front at a slight angle', 'hot magenta and electric cyan'),
}
STYLE = ('Arcade hero shooter loadout art: {subject}, large in the centre, dynamic tilted angle, seen whole from the side, '
         'cel-shaded comic illustration with bold black ink outlines, vivid {colours} graphic background with halftone '
         'dots, spray paint splatter, speed lines and diagonal stripes, dramatic rim light, energetic, video game art, '
         'full bleed artwork without borders, no text, no hands, no people')


# Ability icons for the HUD and the loadout screen (T_DH_Ability_<Id>.png, 512x512).
ICONS = {
    'Dash': 'a running figure bursting forward with speed lines',
    'Leap': 'a figure leaping high upwards with a big arrow up',
    'Adrenaline': 'a lightning bolt over a beating heart',
    'Recon': 'a radar screen with a sweeping pulse and red enemy dots',
    'Cloak': 'a fading half-transparent silhouette of a person dissolving into pixels',
}
ICON_STYLE = ('Bold arcade video game ability icon: {subject}, simple centred emblem filling the picture, thick black ink '
              'outlines, flat neon yellow, white and hot magenta shapes on a pure black background, comic style, '
              'high contrast, no text, no border')


def main(ids):
    os.makedirs(OUT, exist_ok=True)
    for id in ids:
        if id in ICONS:
            target = os.path.join(OUT, 'T_DH_Ability_%s.png' % id)
            fetch(generate(ICON_STYLE.format(subject=ICONS[id]), {'resolution': '512x512'}), target, trim_borders=False)  # black is the background
        else:
            subject, colours = SUBJECTS[id]
            target = os.path.join(OUT, 'T_DH_Art_%s.png' % id)
            fetch(generate(STYLE.format(subject=subject, colours=colours), {'resolution': '1024x576'}), target)
        print('done', target, flush=True)


if __name__ == '__main__':
    main(sys.argv[1:] or list(SUBJECTS) + list(ICONS))
