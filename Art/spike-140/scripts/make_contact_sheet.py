"""#140 spike: labelled contact sheet from the preview renders (plain Python + ImageMagick 7).

  python3 Art/spike-140/scripts/make_contact_sheet.py        -> Art/spike-140/previews/contact_sheet.png
"""
import os
import subprocess

HERE = os.path.dirname(os.path.abspath(__file__))
PREV = os.path.abspath(os.path.join(HERE, '..', 'previews'))
TW, TH, BAND, GAP = 640, 360, 30, 8
FONT = 'Noto-Sans-Bold'


def run(*a):
    subprocess.run(['magick', *a], check=True)


def tile(out, src_args, label):
    run(*src_args, '-background', '#14141a', '-gravity', 'center', '-extent', f'{TW}x{TH}',
        '-gravity', 'north', '-splice', f'0x{BAND}', '-fill', '#14141a', '-draw', f'rectangle 0,0 {TW},{BAND}',
        '-fill', '#f2d9a0', '-font', FONT, '-pointsize', '15', '-annotate', '+0+6', label, out)


def main():
    t = os.path.join(PREV, '_tiles')
    os.makedirs(t, exist_ok=True)
    P = lambda n: os.path.join(PREV, n + '.png')
    tiles = [
        ('A', [P('A_fov50_pitch58_board'), '-resize', f'{TW}x{TH}'], 'A  today: FOV 50, pitch 58 deg, whole board'),
        ('B', [P('B_fov40_pitch45_board'), '-resize', f'{TW}x{TH}'], 'B  FOV 40, pitch 45 deg, whole board'),
        ('C', [P('C_fov40_pitch35_party'), '-resize', f'{TW}x{TH}'], 'C  FOV 40, pitch 35 deg, party framing'),
        ('D', [P('grid_off'), '-resize', f'{TW}x{TH}'], 'no grid at rest (FOV 40, 45 deg)'),
        ('E', [P('grid_on'), '-resize', f'{TW}x{TH}'], 'mock contextual grid: 4-cell reach, path, 56 cm ring'),
        ('F', ['(', P('rig_bind'), '-crop', '640x1080+160+0', '+repage', '-resize', 'x360', ')',
               '(', P('rig_walk_mid'), '-crop', '640x1080+160+0', '+repage', '-resize', 'x360', ')',
               '(', P('rig_idle_mid'), '-crop', '640x1080+160+0', '+repage', '-resize', 'x360', ')', '+append'],
         'rigged fighter (25 bones): bind / mid-walk / mid-idle'),
        ('G', [P('filter_closest'), '-crop', f'{TW}x{TH}+560+330', '+repage'], 'texture sampling: Closest (1:1 crop)'),
        ('H', [P('filter_linear'), '-crop', f'{TW}x{TH}+560+330', '+repage'], 'texture sampling: Linear + 16x aniso (same crop)'),
        ('I', [P('occluders_highlight'), '-resize', f'{TW}x{TH}'], 'occluder mesh (orange) split from the kept architecture'),
    ]
    outs = []
    for k, args, label in tiles:
        o = os.path.join(t, k + '.png')
        tile(o, args, label)
        outs.append(o)
    out = os.path.join(PREV, 'contact_sheet.png')
    run('montage', *outs, '-tile', '3x3', '-geometry', f'+{GAP}+{GAP}', '-background', '#0b0b0f', out)
    for o in outs:
        os.remove(o)
    os.rmdir(t)
    print('wrote', out)


main()
