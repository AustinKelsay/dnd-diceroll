"""Blender preview of the subclass palettes before Unreal (#150 batch 4): the shader's recolour applied to a copy of each class atlas.

    blender -b --python-exit-code 1 --python Art/rig-pilot/cast/scripts/palette_preview.py -- [class ...]

Writes .local/palettes/preview_<class>.png: one 3/4 view per build of the class, the class default first, in Idle frame 0. The maths is the
material's (Content/Python/import_dark_fantasy_theme.py, palette block): a masked pixel becomes target x min(luminance(pixel) / reference, 1.7)
in linear light. The in-game contact sheet (gallery) is the evidence; this is the cheap loop used while designing palettes.
"""
import json
import math
import os
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import cast_batch4            # noqa: E402,F401
_namespace = globals()
_REVIEW4 = os.path.join(HERE, 'review4.py')
_args = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
exec(compile(open(_REVIEW4).read().split('\nmode = _args[0]')[0], _REVIEW4, 'exec'), _namespace)

import bpy                    # noqa: E402,F811
import numpy as np            # noqa: E402,F811
from mathutils import Vector  # noqa: E402,F811

PAL = os.path.join(R, 'Art', 'rig-pilot', 'cast', 'palettes')
RES = json.load(open(os.path.join(PAL, 'palettes.resolved.json')))
OUT = os.path.join(R, '.local', 'palettes')
os.makedirs(OUT, exist_ok=True)


def to_lin(c):
    return np.where(c <= .04045, c / 12.92, ((c + .055) / 1.055) ** 2.4)


def to_srgb(c):
    return np.where(c <= .0031308, c * 12.92, 1.055 * np.maximum(c, 1e-9) ** (1 / 2.4) - .055)


def recolour(base, cls, slots, accent=None):
    """base: float32 (1024, 1024, 4) top row first, sRGB-encoded. slots: {'A': linear rgb, ...}; accent: linear rgb for X."""
    img = base.copy()
    lin = to_lin(base[..., :3])
    lum = lin @ np.array([.2126, .7152, .0722], np.float32)
    ty, tx = np.mgrid[0:1024, 0:1024]
    tile = (ty // 256) * 4 + (tx // 256)
    order = list(slots.items()) + ([('X', accent)] if accent is not None else [])
    for s, target in order:
        mask = cls['slots'][s]['mask']
        sel = ((mask >> tile) & 1).astype(bool)
        ref = max(cls['slots'][s]['reference'], .02)
        out = np.array(target, np.float32)[None, None, :] * np.minimum(lum / ref, 1.7)[..., None]
        lin = np.where(sel[..., None], out, lin)
    img[..., :3] = to_srgb(np.clip(lin, 0, 1))
    return img


def render_class(key, accent_slot=None):
    cls = RES['classes'][key]
    builds = [('default', cls['default']['slots'])] + [(b['name'], b['slots']) for bid, b in RES['builds'].items() if bid.startswith(key + '.')]
    tiles = []
    tmp = tempfile.mkdtemp(prefix='pal150_', dir=os.path.join(R, '.local', 'scratch'))
    for n, (name, slots) in enumerate(builds):
        fresh()
        setup_render(300, 440)
        ao, ms = load(glb(key) if key not in LEGACY_REV2 else os.path.join(R, 'Art', CH[key][0], 'r001', f'{CH[key][1]}.glb'))
        for m in ms:
            unlit(m)
            if not m.material_slots:
                continue
            img = next(nd.image for nd in m.material_slots[0].material.node_tree.nodes if nd.type == 'TEX_IMAGE')
            px = np.array(img.pixels[:], np.float32).reshape(1024, 1024, 4)[::-1]
            acc = cls['accent'][accent_slot]['linear'] if accent_slot is not None else None
            new = recolour(px, cls, {s: v['linear'] for s, v in slots.items()}, acc)
            img.pixels = new[::-1].reshape(-1).tolist()
            img.update()
        set_pose(ao, 'Idle', 0)
        cam = ortho_cam(2.3, (0, 0, .98), (1, 0, 0))
        d = VIEW
        cam.location = Vector((0, 0, .98)) + d * 8
        cam.rotation_euler = (-d).to_track_quat('-Z', 'Y').to_euler()
        p = f'{tmp}/{n:02d}.png'
        render(p)
        magick(p, '-background', '#2a2d36', '-flatten', '-gravity', 'south', '-fill', 'white', '-pointsize', '18', '-annotate', '+0+4', name, p)
        tiles.append(p)
    cols = 6
    rows = []
    for i in range(0, len(tiles), cols):
        row = f'{tmp}/row_{i}.png'
        magick(*tiles[i:i + cols], '-background', '#2a2d36', '+append', row)
        rows.append(row)
    suffix = '' if accent_slot is None else f'_accent{accent_slot + 1}'
    magick(*rows, '-background', '#2a2d36', '-append', os.path.join(OUT, f'preview_{key}{suffix}.png'))


for k in [a for a in _args if a in RES['classes']] or list(RES['classes']):
    render_class(k)
