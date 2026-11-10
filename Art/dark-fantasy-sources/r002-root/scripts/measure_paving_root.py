"""Measure the Root paving and figure readability (issue #149 revision).

  blender -b --python Art/dark-fantasy-sources/r002-root/scripts/measure_paving_root.py -- [label]

Writes Art/dark-fantasy-sources/r002-root/paving_measure.json (label -> numbers):
  * paving luminance: area-weighted mean / std of (atlas texel x vertex light), linear, over the stone tops inside the 27 x 18 m footprint
    (read back from SM_World_Root.glb), and the share of the footprint that is NOT stone top (earth, moss, joints, flat roots);
  * figure-to-ground contrast in the default frame (FOV 40, pitch 45): the same frame is rendered with and without the r001 figures,
    the pixels that differ are the figures; contrast = |Lg - Lf| / (Lg + Lf) of their mean display luminance against the ground
    behind them, plus the mean ground luminance of the board centre.
Used before and after a paving revision; numbers are comparable only with themselves (Blender EEVEE stand-in lighting).
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import r002_lib as R
import lod_r002 as D
from r002_lib import bpy, np, ROOT

label = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else 'current'
OUT = os.path.join(ROOT, 'Art', 'dark-fantasy-sources', 'r002-root', 'paving_measure.json')
FX, FY = 13.5, 9.0


def lum(rgb):
    return rgb[..., 0] * .2126 + rgb[..., 1] * .7152 + rgb[..., 2] * .0722


def paving():
    P, UV, C, png = D.read_glb(os.path.join(ROOT, 'Art', 'world-root', 'r002', 'SM_World_Root.glb'))
    px = D.atlas_pixels(bpy.data.images.load(png))
    cen = P.mean(1)
    n = np.cross(P[:, 1] - P[:, 0], P[:, 2] - P[:, 0])
    area = np.linalg.norm(n, axis=1) / 2
    nz = n[:, 2] / np.maximum(np.linalg.norm(n, axis=1), 1e-12)
    sel = (np.abs(cen[:, 0]) < FX) & (np.abs(cen[:, 1]) < FY) & (cen[:, 2] > -.02) & (nz > .5)
    wm = json.load(open(os.path.join(ROOT, 'Art', 'dark-fantasy-sources', 'r002-root', 'world_metrics.json')))['atlas']
    size = wm['size']
    uvc = UV.mean(1)
    on_stone = np.zeros(len(P), bool)                  # stone triangles: centroid UV inside one of the four flagstone atlas regions
    for k in ('paveA', 'paveB', 'paveC', 'paveM'):
        if k in wm.get('regions_xywh', {}):
            x0, y0, w, h = wm['regions_xywh'][k]
            on_stone |= (uvc[:, 0] * size >= x0) & (uvc[:, 0] * size <= x0 + w) & ((1 - uvc[:, 1]) * size >= y0) & ((1 - uvc[:, 1]) * size <= y0 + h)
    sel &= on_stone
    tex = D.sample(px, UV[sel].mean(1))
    col = tex * C[sel][..., :3].mean(1)
    y = lum(col)
    w = area[sel]
    proj = area[sel] * nz[sel]
    mean = float((y * w).sum() / w.sum())
    std = float(np.sqrt(((y - mean) ** 2 * w).sum() / w.sum()))
    stone_cover = float(proj.sum() / (2 * FX * 2 * FY))
    return dict(paving_luminance_mean=round(mean, 4), paving_luminance_std=round(std, 4), paving_contrast_std_over_mean=round(std / mean, 3),
                paving_luminance_p5_p95=[round(float(np.percentile(y, 5)), 4), round(float(np.percentile(y, 95)), 4)],
                footprint_share_not_stone_top=round(1 - stone_cover, 3))


def frames():
    src = open(os.path.join(HERE, 'render_previews_root.py')).read().rsplit('\nmain()', 1)[0]
    g = {'__file__': os.path.join(HERE, 'render_previews_root.py'), '__name__': 'rp'}
    exec(compile(src, 'render_previews_root.py', 'exec'), g)
    world, occ = g['build_board_scene']()
    hx, hy = g['W'] * g['CELL'] / 2, g['H'] * g['CELL'] / 2
    board = [(-hx, -hy, 0), (hx, -hy, 0), (hx, hy, 0), (-hx, hy, 0)]
    g['place_camera'](40, 45, (0, 0, 0), board)
    sc = bpy.context.scene
    figs = [o for o in bpy.data.objects if o.name.startswith(('SM_Hero', 'SM_Foe'))]
    fig_set = set(figs)
    for o in figs:
        fig_set |= set(o.children_recursive)
    out = {}

    def shoot(tag):
        path = os.path.join(os.environ.get('TMPDIR', '/tmp'), f'measure_{tag}.png')
        sc.render.filepath = path
        bpy.ops.render.render(write_still=True)
        img = bpy.data.images.load(path)
        a = np.empty(img.size[0] * img.size[1] * 4, np.float32)
        img.pixels.foreach_get(a)
        shape = (img.size[1], img.size[0], 4)
        bpy.data.images.remove(img)
        return a.reshape(shape)
    meshes = [o for o in bpy.data.objects if o.type == 'MESH']
    for o in meshes:
        o.hide_render = o not in fig_set
    sc.render.film_transparent = True
    sc.render.image_settings.color_mode = 'RGBA'
    mask_img = shoot('mask')
    sc.render.film_transparent = False
    for o in meshes:
        o.hide_render = False
    out['full'] = shoot('full')[..., :3]
    for o in meshes:
        o.hide_render = o in fig_set
    out['ground'] = shoot('ground')[..., :3]
    diff = mask_img[..., 3] > .5                  # the figures' own silhouette (alpha of a figures-only render)
    h, w = diff.shape
    lf, lg = float(lum(out['full'][diff]).mean()), float(lum(out['ground'][diff]).mean())
    board_px = lum(out['ground'][h // 5: 4 * h // 5, w // 5: 4 * w // 5])
    gl = lum(out['ground'][h // 5: 4 * h // 5, w // 5: 4 * w // 5])
    fl = lum(out['full'])[diff]
    gb = lum(out['ground'])[diff]
    return dict(figure_bright_pixel_share_over_ground=round(float((fl > gb * 1.15).mean()), 3), ground_busyness_std_over_mean=round(float(gl.std() / gl.mean()), 3),
                figure_pixels=int(diff.sum()), figure_luminance=round(lf, 4), ground_behind_figures_luminance=round(lg, 4),
                figure_to_ground_contrast=round(abs(lg - lf) / (lg + lf), 3), figure_over_ground_ratio=round(lf / lg, 3), board_centre_ground_luminance=round(float(board_px.mean()), 4))


res = paving()
res.update(frames())
try:
    allres = json.load(open(OUT))
except Exception:
    allres = {}
allres[label] = res
json.dump(allres, open(OUT, 'w'), indent=1)
print('MEASURE', label, json.dumps(res))
