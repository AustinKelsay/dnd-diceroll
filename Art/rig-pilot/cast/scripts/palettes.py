#!/usr/bin/env python3
"""Palette generator for #150 batch 4 (stdlib only): palettes.json + tiles.json -> resolved colours, a review page and the C++ table.

    python3 Art/rig-pilot/cast/scripts/palettes.py            # write the three outputs
    python3 Art/rig-pilot/cast/scripts/palettes.py --check    # exit 1 if a committed output differs from what the inputs generate

Inputs (Art/rig-pilot/cast/palettes/):
    palettes.json  the authored table: per class which atlas tiles each slot recolours, per build a [hue, saturation, luminance ratio] per slot
    tiles.json     measured by palette_tiles.py (Blender): mean colour and surface-area share of each of the 13 atlas tiles, per class
Outputs:
    palettes.resolved.json                                          every slot as hex and linear RGB, the masks, the reference luminances
    swatches.html                                                   the review page (one row per build)
    Source/DnDDiceRoll/Private/Presentation/ExpeditionPaletteTable.inl   the table compiled into the game

The mechanism (Content/Python/import_dark_fantasy_theme.py, M_DiceRollTheme): the atlas has 13 tiles on a 4x4 grid, index = row * 4 + column, row 0 at
the top, in rigkit.REGIONS order. A slot has a bit mask over those tiles, a target colour and a reference luminance (the class's own colour of
that slot). The shader replaces a masked pixel's colour by target x min(luminance(pixel) / reference, 1.7), so the tile keeps its folds, bands
and highlights while the hue changes. No mesh and no atlas changes; the colours are material-instance parameters set per figure from the build id.
"""
import colorsys
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
DIR = ROOT / 'Art' / 'rig-pilot' / 'cast' / 'palettes'
INL = ROOT / 'Source' / 'DnDDiceRoll' / 'Private' / 'Presentation' / 'ExpeditionPaletteTable.inl'
REGIONS = ['plate', 'trim', 'cloth', 'leather', 'blade', 'glow', 'rune', 'skin', 'bone', 'accent', 'dark', 'emblem', 'emblem2']
SLOTS = ('A', 'B', 'C', 'G')          # build slots; X (the accent) follows
ROLE = {'fighter': 'Fighter', 'rogue': 'Rogue', 'cleric': 'Cleric', 'wizard': 'Wizard', 'barbarian': 'Barbarian', 'monk': 'Monk', 'paladin': 'Paladin',
        'ranger': 'Ranger', 'sorcerer': 'Sorcerer', 'warlock': 'Warlock', 'druid': 'Druid', 'bard': 'Bard', 'artificer': 'Artificer'}


def lin(c):
    return c / 12.92 if c <= .04045 else ((c + .055) / 1.055) ** 2.4


def luma(rgb):                       # perceptual (encoded) luminance, Rec. 601
    return .299 * rgb[0] + .587 * rgb[1] + .114 * rgb[2]


def luma_lin(rgb):                   # what the shader measures (linear Rec. 709)
    r, g, b = (lin(c) for c in rgb)
    return .2126 * r + .7152 * g + .0722 * b


def hexof(rgb):
    return '#%02x%02x%02x' % tuple(round(max(0, min(1, c)) * 255) for c in rgb)


def fit(h, s, target):
    """The colour of hue h (degrees) and saturation s whose encoded luminance is `target`; too bright for the hue even at full value mixes in white."""
    def at(v):
        return colorsys.hsv_to_rgb((h % 360) / 360, s, v)
    if luma(at(1.0)) >= target:
        lo, hi = 0.0, 1.0
        for _ in range(40):
            mid = (lo + hi) / 2
            lo, hi = (mid, hi) if luma(at(mid)) < target else (lo, mid)
        return at((lo + hi) / 2)
    c = at(1.0)
    lo, hi = 0.0, 1.0
    for _ in range(40):
        mid = (lo + hi) / 2
        mixed = tuple(x + (1 - x) * mid for x in c)
        lo, hi = (mid, hi) if luma(mixed) < target else (lo, mid)
    t = (lo + hi) / 2
    return tuple(x + (1 - x) * t for x in c)


def tile_mask(tiles):
    m = 0
    for t in tiles:
        m |= 1 << REGIONS.index(t)
    return m


def slot_default(tiles_json, names):
    """Area-weighted mean encoded colour of the tiles of a slot on one class (the plain mean when none of them is used)."""
    w = [max(tiles_json[n]['area_share'], 0.0) for n in names]
    if sum(w) <= 1e-9:
        w = [1.0] * len(names)
    return tuple(sum(wi * tiles_json[n]['rgb'][k] for wi, n in zip(w, names)) / sum(w) for k in range(3))


def resolve():
    src = json.loads((DIR / 'palettes.json').read_text())
    tiles = json.loads((DIR / 'tiles.json').read_text())
    acc = src['accent']
    out = {'version': src['version'], 'classes': {}, 'builds': {}, 'accent': acc}
    for key, c in src['classes'].items():
        t = tiles[key]
        slots = {}
        for s in SLOTS + ('X',):
            # glow: the first tile (the flat glow colour) is the reference, so a rune tile stays darker than the glow it frames
            d = tuple(t[c['tiles'][s][0]]['rgb']) if s == 'G' else slot_default(t, c['tiles'][s])
            slots[s] = dict(tiles=c['tiles'][s], mask=tile_mask(c['tiles'][s]), default_hex=hexof(d), default_luma=round(luma(d), 4),
                            reference=round(luma_lin(d), 5))
        # A class may override the accent's luminance range and saturation (a class whose accent tiles are small and sit on a pale or a dark figure needs a darker or
        # a brighter, more saturated colour to separate at the default zoom); the hues are always the party slots'.
        over = c.get('accent', {})
        lo, hi = over.get('luminance', [acc['luminance_min'], acc['luminance_max']])
        sat = over.get('saturation', acc['saturation'])
        xl = min(max(slots['X']['default_luma'], lo), hi)
        accents = []
        for h in acc['hues']:
            rgb = fit(h, sat, xl)
            accents.append(dict(hex=hexof(rgb), linear=[round(lin(x), 5) for x in rgb]))
        out['classes'][key] = dict(role=ROLE[key], slots=slots, accent=accents, accent_override=over, default=c['default'])
    for bid, b in src['builds'].items():
        key = bid.split('.')[0]
        cl = out['classes'][key]
        entry = dict(name=b['name'], note=b['note'], slots={})
        for s in SLOTS:
            if s not in b:
                continue
            h, sat, ratio = b[s]
            target = cl['slots'][s]['default_luma'] * ratio
            rgb = fit(h, sat, target)
            entry['slots'][s] = dict(spec=[h, sat, ratio], hex=hexof(rgb), linear=[round(lin(x), 5) for x in rgb], luma=round(luma(rgb), 4),
                                     luma_ratio=round(luma(rgb) / cl['slots'][s]['default_luma'], 3))
        out['builds'][bid] = entry
    # the class default: used when a build id is unknown. Identity (no slot) unless the class says otherwise (the fighter's corrected steel).
    for key, cl in out['classes'].items():
        entry = dict(slots={})
        for s in SLOTS:
            if s in cl['default']:
                h, sat, ratio = cl['default'][s]
                rgb = fit(h, sat, cl['slots'][s]['default_luma'] * ratio)
                entry['slots'][s] = dict(spec=[h, sat, ratio], hex=hexof(rgb), linear=[round(lin(x), 5) for x in rgb])
        cl['default'] = entry
    return out


def inl(res):
    f = lambda v: f'{v:.5f}f'
    rows = ['// Generated by Art/rig-pilot/cast/scripts/palettes.py from palettes.json and tiles.json. Do not edit; run the script.',
            '// Slot order A, B, C, G (builds) and A, B, C, G, X (class masks). Colours are linear RGB.', '']
    rows.append('static const FPaletteClassRow PaletteClassRows[] = {')
    for key, cl in res['classes'].items():
        masks = ', '.join('0x%04xu' % cl['slots'][s]['mask'] for s in SLOTS + ('X',))
        refs = ', '.join(f(cl['slots'][s]['reference']) for s in SLOTS + ('X',))
        acc = ', '.join('{%s, %s, %s}' % tuple(f(x) for x in a['linear']) for a in cl['accent'])
        present, cols = 0, []
        for i, s in enumerate(SLOTS):
            if s in cl['default']['slots']:
                present |= 1 << i
                cols.append('{%s, %s, %s}' % tuple(f(x) for x in cl['default']['slots'][s]['linear']))
            else:
                cols.append('{0.f, 0.f, 0.f}')
        rows.append(f'    {{TEXT("{key}"), {{{masks}}}, {{{refs}}}, {{{acc}}}, {present}, {{{", ".join(cols)}}}}},')
    rows.append('};')
    rows.append('')
    rows.append('static const FPaletteBuildRow PaletteBuildRows[] = {')
    for bid, b in res['builds'].items():
        present, cols = 0, []
        for i, s in enumerate(SLOTS):
            if s in b['slots']:
                present |= 1 << i
                cols.append('{%s, %s, %s}' % tuple(f(x) for x in b['slots'][s]['linear']))
            else:
                cols.append('{0.f, 0.f, 0.f}')
        rows.append(f'    {{TEXT("{bid}"), {present}, {{{", ".join(cols)}}}}},')
    rows.append('};')
    return '\n'.join(rows) + '\n'


def html(res):
    h = ['<!doctype html><meta charset="utf-8"><title>Subclass palettes (#150 batch 4)</title>',
         '<style>body{font:13px system-ui;background:#16161b;color:#e8e4da;margin:20px}table{border-collapse:collapse;margin:0 0 24px}td,th{padding:3px 8px;text-align:left}',
         'th{color:#c9b27c}.s{display:inline-block;width:46px;height:20px;border:1px solid #0008;vertical-align:middle;margin-right:2px}.n{color:#9a9588}</style>',
         '<h1>Subclass palettes</h1><p class="n">Generated by palettes.py. Per row: slots A primary garment, B secondary garment, C trim, G glow as the build sets them (a faded swatch is the class colour kept). '
         'After the divider: the duplicate-identity accents for party slots 1 to 4.</p>']
    for key, cl in res['classes'].items():
        h.append(f'<h2>{key}</h2><table><tr><th>build</th><th>A</th><th>B</th><th>C</th><th>G</th><th>accent 1..4</th><th>look</th></tr>')
        def row(name, slots, note):
            cells = ''
            for s in SLOTS:
                kept = s not in slots
                col = cl['slots'][s]['default_hex'] if kept else slots[s]['hex']
                cells += f'<td><span class="s" style="background:{col};opacity:{.35 if kept else 1}" title="{s}"></span></td>'
            ac = ''.join(f'<span class="s" style="background:{a["hex"]}"></span>' for a in cl['accent'])
            return f'<tr><td>{name}</td>{cells}<td>{ac}</td><td class="n">{note}</td></tr>'
        h.append(row('<i>class default</i>', cl['default']['slots'], 'used for an unknown build'))
        for bid, b in res['builds'].items():
            if bid.startswith(key + '.'):
                h.append(row(f'{b["name"]} <span class="n">{bid}</span>', b['slots'], b['note']))
        h.append('</table>')
    return '\n'.join(h) + '\n'


def main():
    res = resolve()
    outputs = {DIR / 'palettes.resolved.json': json.dumps(res, indent=1) + '\n', DIR / 'swatches.html': html(res), INL: inl(res)}
    if '--check' in sys.argv:
        bad = [str(p.relative_to(ROOT)) for p, text in outputs.items() if not p.exists() or p.read_text() != text]
        if bad:
            sys.stderr.write('palettes.py --check: out of date: ' + ', '.join(bad) + '\n')
            sys.exit(1)
        print('palettes.py --check: up to date')
        return
    for p, text in outputs.items():
        p.write_text(text)
        print('wrote', p.relative_to(ROOT))
    ratios = [s['luma_ratio'] for b in res['builds'].values() for s in b['slots'].values()]
    print('builds', len(res['builds']), 'slot luminance ratios', min(ratios), '..', max(ratios))


if __name__ == '__main__':
    main()
