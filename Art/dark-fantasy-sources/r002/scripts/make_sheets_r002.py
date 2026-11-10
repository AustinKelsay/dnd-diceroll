"""r002 contact sheet from the preview renders (plain Python + ImageMagick 7).

  python3 Art/dark-fantasy-sources/r002/scripts/make_sheets_r002.py   ->  previews/contact_sheet.png
(kit_obstacles.png and kit_scatter.png are the kit sheets, written by render_previews_r002.py.)
"""
import os
import subprocess

HERE = os.path.dirname(os.path.abspath(__file__))
PREV = os.path.abspath(os.path.join(HERE, '..', 'previews'))
TW, TH, BAND, GAP = 800, 450, 30, 8
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
        ('A', [P('default_p45'), '-resize', f'{TW}x{TH}'], 'A  default: FOV 40, pitch 45, whole 18 x 12 board'),
        ('B', [P('close_p35'), '-resize', f'{TW}x{TH}'], 'B  close: FOV 40, pitch 35, party framing'),
        ('C', [P('far_p60'), '-resize', f'{TW}x{TH}'], 'C  far: FOV 40, pitch 60 (rune ring bottom left, off the board)'),
        ('D', [P('paving_close'), '-resize', f'{TW}x{TH}'], 'D  paving detail: irregular flagstones 35-90 cm, 180 cm figures'),
        ('E', [P('wide_p26'), '-resize', f'{TW}x{TH}'], 'E  wide architecture shot (all occluder pieces drawn)'),
        ('F', [P('occ_highlight'), '-resize', f'{TW}x{TH}'], 'F  same view, occluder mesh tinted orange'),
    ]
    outs = []
    for k, args, label in tiles:
        o = os.path.join(t, k + '.png')
        tile(o, args, label)
        outs.append(o)
    out = os.path.join(PREV, 'contact_sheet.png')
    run('montage', *outs, '-tile', '2x3', '-geometry', f'+{GAP}+{GAP}', '-background', '#0b0b0f', out)
    for o in outs:
        os.remove(o)
    os.rmdir(t)
    print('wrote', out)


main()
