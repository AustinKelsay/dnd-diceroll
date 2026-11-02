"""mission.blend -> SM_Tabletop.glb, SM_Diorama_Vigil.glb, SM_Diorama_Root.glb.

Source facts (ps1-src table.py, HD build): table grid 184 x 92 units at TZ=-2.2, viewer on -Y.
Trays: v at (0,0), r at (-33,5); hx,hy = 13.3, 12.3. Render = texture x baked light x tint."""
import math
import sys

sys.path.insert(0, __import__('os').path.dirname(__import__('os').path.abspath(__file__)))
import bpy
import numpy as np

import tlib as L
from prims import Soup, rock
import build_props as BP

TZ = -2.2
TEX = BP.TEX
TINT = {'table': (1.0, .9, .84), 'gilt': (1.2, 1.15, 1.0), 'parch': (.92, .84, .72), 'd20': (2.2, 2.0, 2.0), 'd8': (2.0, 2.0, 2.0),
        'd6': (2.0, 1.9, 1.8), 'steel': (1.9, 1.9, 2.0), 'books': (2.3, 2.2, 2.1), 'shelf': (1.8, 1.7, 1.6),
        'brickroom': (1.5, 1.4, 1.3), 'coin': (1.4, 1.35, 1.2)}


def alpha_at(tex, uv):
    h, w = tex.shape[:2]
    c = (np.mod(uv[..., 0], 1) * w).astype(int).clip(0, w - 1)
    r = (np.mod(uv[..., 1], 1) * h).astype(int).clip(0, h - 1)
    return tex[r, c, 3]


def refine_cutout(s, tex, depth=2, mat='parch'):
    """Replace alpha-cutout triangles by opaque sub-triangles that follow the texture's alpha edge."""
    keep_tris, mats = [], []
    bary = np.array([[1, 0, 0], [0, 1, 0], [0, 0, 1], [1 / 3, 1 / 3, 1 / 3], [.5, .5, 0], [0, .5, .5], [.5, 0, .5],
                     [.6, .2, .2], [.2, .6, .2], [.2, .2, .6]])
    bary[:3] = [[.9, .05, .05], [.05, .9, .05], [.05, .05, .9]]

    def rec(T, d):
        uv = bary @ T[:, 3:5]
        a = alpha_at(tex, uv) >= .5
        if a.all():
            keep_tris.append(T); return
        if not a.any() and d > 0:
            return
        if d == depth:
            if a.mean() >= .5:
                keep_tris.append(T)
            return
        m01, m12, m20 = (T[0] + T[1]) / 2, (T[1] + T[2]) / 2, (T[2] + T[0]) / 2
        for t in ((T[0], m01, m20), (m01, T[1], m12), (m20, m12, T[2]), (m01, m12, m20)):
            rec(np.array(t), d + 1)

    for i in range(len(s['P'])):
        rec(L._pack_tri(s, i).astype(float), 0)
    out = L._unpack(keep_tris, s['C'] is not None, [mat] * len(keep_tris))
    return out


def flames_from_cards(s):
    """candleflame cards -> small closed opaque flames at full brightness."""
    cen = s['P'].mean(1)
    groups = []
    for i, c in enumerate(cen):
        for g in groups:
            if np.hypot(*(g[0] - c[:2])) < 2:
                g[1].append(i); break
        else:
            groups.append([c[:2].copy(), [i]])
    sp = Soup()
    for g in groups:
        Q = s['P'][g[1]].reshape(-1, 3)
        x, y = Q[:, 0].mean(), Q[:, 1].mean()
        z0, z1 = Q[:, 2].min(), Q[:, 2].max()
        h = (z1 - z0) * .9
        r = np.ptp(Q[:, 0]) * .28
        prof = [(.001, 0), (r * .7, h * .06), (r, h * .22), (r * .8, h * .45), (r * .45, h * .7), (r * .15, h * .9), (.001, h)]
        sp.lathe('flame', (x, y, z0 + (z1 - z0) * .05), prof, 6, cap=(True, True))
    f = sp.soup()
    f['C'] = np.ones_like(f['P'])
    return f


def flame_uv(f):
    """v from each flame's own height (row 0 = tip)."""
    cen = f['P'].mean(1)
    uv = f['UV'].copy()
    uv[..., 0] = .5
    for k in np.unique(np.round(cen[:, :2] / 3)[:, 0] + 1000 * np.round(cen[:, :2] / 3)[:, 1]):
        m = (np.round(cen[:, :2] / 3)[:, 0] + 1000 * np.round(cen[:, :2] / 3)[:, 1]) == k
        z = f['P'][m][..., 2]
        uv[m, :, 1] = np.clip(1 - (z - z.min()) / (z.max() - z.min() + 1e-9), .01, .99)
    f['UV'] = uv
    return f


def tabletop(s, texs):
    mat = s['mat']
    cen = s['P'].mean(1)
    keep_mats = ['shelf', 'books', 'd20', 'd8', 'd6', 'coin', 'brass', 'wax', 'steel', 'leatherg', 'waxseal']
    k = np.isin(mat, keep_mats)
    sword_gilt = (mat == 'gilt') & (cen[:, 0] > 25) & (cen[:, 0] < 45) & (cen[:, 1] < -25)
    back = (mat == 'brickroom') & (cen[:, 1] > 70)
    keep = L.sub(s, k | sword_gilt)
    # long shelf boards: stretch the wood along their length (fewer texture repeats -> fewer atlas cuts)
    sh = keep['mat'] == 'shelf'
    span = np.ptp(keep['UV'][sh], axis=1)
    uvs = keep['UV'][sh]
    long_u = span[:, 0] > 1.5
    uvs[long_u, :, 0] /= 4
    long_v = span[:, 1] > 1.5
    uvs[long_v, :, 1] /= 4
    keep['UV'][sh] = uvs
    parts = [keep, L.clip_rect(L.sub(s, back), -100, 100, -1e9, 1e9, keep_inside=True)]
    # table top: same plane re-gridded on texture-tile boundaries (4 cells per 24.53-unit tile), light resampled
    tb = L.sub(s, mat == 'table')
    TP, TC = tb['P'].reshape(-1, 3), tb['C'].reshape(-1, 3)
    nx, ny = 30, 15
    xs, ys = np.linspace(-92, 92, nx + 1), np.linspace(-46, 46, ny + 1)
    gx, gy = np.meshgrid(xs, ys, indexing='ij')
    q = np.stack([gx.ravel(), gy.ravel()], 1)
    d = np.linalg.norm(TP[None, :, :2] - q[:, None, :], axis=2)
    idx = np.argsort(d, 1)[:, :8]
    w = 1 / (np.take_along_axis(d, idx, 1) + .8)
    col = ((TC[idx] * w[..., None]).sum(1) / w.sum(1, keepdims=True)).reshape(nx + 1, ny + 1, 3)
    tt = []
    for i in range(nx):
        for j in range(ny):
            pp = [(xs[i], ys[j], TZ), (xs[i + 1], ys[j], TZ), (xs[i + 1], ys[j + 1], TZ), (xs[i], ys[j + 1], TZ)]
            uv = [((x + 92) / (184 / 7.5), (y + 46) / (184 / 7.5)) for x, y, _ in pp]
            cc = [col[i, j], col[i + 1, j], col[i + 1, j + 1], col[i, j + 1]]
            for a_, b_, c_ in ((0, 1, 2), (0, 2, 3)):
                tt.append(dict(P=np.array([[pp[a_], pp[b_], pp[c_]]]), UV=np.array([[uv[a_], uv[b_], uv[c_]]]),
                               C=np.array([[cc[a_], cc[b_], cc[c_]]]), mat=np.array(['table'], dtype=object)))
    parts.append(L.cat(*tt))
    parch = refine_cutout(L.sub(s, mat == 'parch'), texs['parch'])
    print('parch', int((mat == 'parch').sum()), '->', len(parch['P']))
    parts.append(parch)
    t = L.cat(*parts)
    t = L.clip_z(t, TZ - 26)          # nothing far below the table top is ever seen
    # light x tint -> COLOR_0 (soft shoulder)
    tint = np.ones((len(t['P']), 1, 3))
    for m_, v in TINT.items():
        tint[t['mat'] == m_] = v
    t['C'] = L.knee(t['C'] * tint)
    f = flame_uv(flames_from_cards(L.sub(s, mat == 'candleflame')))
    t = L.cat(t, f)
    texs = dict(texs)
    texs['flame'] = BP.flame_tex()
    # table edge apron so the top is not paper-thin from the viewer side
    ap = Soup()
    ap.grid('table', (-92, -46, TZ - 3), (184, 0, 0), (0, 0, 3), 46, 1, (7.5, .12))
    ap.grid('table', (92, -46, TZ - 3), (0, 92, 0), (0, 0, 3), 23, 1, (3.75, .12))
    ap.grid('table', (-92, 46, TZ - 3), (0, -92, 0), (0, 0, 3), 23, 1, (3.75, .12))
    a = ap.soup()
    edge_light = np.median(L.sub(t, t['mat'] == 'table')['C'].reshape(-1, 3), 0) * .55
    a['C'] = np.ones_like(a['P']) * edge_light
    t = L.cat(t, a)
    # orientation: viewer -Y -> +X, origin at table-top centre, 184 units -> 2.4 m
    t = L.xform(t, L.S(2.4 / 184) @ L.Rz(90) @ L.T(0, 0, -TZ))
    t, A, _ = L.build_atlas(t, texs, size=1024, pad=4, downscale={'parch': 2, 'd20': 2, 'd6': 2, 'd8': 2, 'books': 2},
                            repeat={'brass': (4, 1), 'waxseal': (2, 2), 'shelf': (2, 1)})
    bpy.ops.wm.read_factory_settings(use_empty=True)
    ob = L.make_object('SM_Tabletop', t, A, color=True)
    L.export_glb(ob, color=True)
    L.save_png(A, f'{L.WORK}/SM_Tabletop_atlas.png')


def diorama(s, texs, key, ox, oy, name):
    """Frame rebuilt from the table.py tray recipe with a shallower depth; contents uniform."""
    HX, HY = 12.4, 11.4
    q = .785                                   # depth squash -> about 0.6 x 0.45 m overall
    hx, hy = HX + .9, (HY + .9) * q
    sp = Soup()
    sp.box('gilt', (0, 0, TZ + .25), (2 * hx + 2.4, 2 * hy + 2.4, .5), tile=4)
    rail = .8
    for cx, cy, sx, sy in ((0, hy, 2 * hx + 2 * rail, 1.5), (0, -hy, 2 * hx + 2 * rail, 1.5), (hx, 0, 1.5, 2 * hy), (-hx, 0, 1.5, 2 * hy)):
        sp.box('gilt', (cx, cy, (TZ + .5 + .18) / 2), (sx, sy, .18 - TZ - .5), tile=4, bottom=False)
        sp.box('giltorn', (cx, cy, .3), (sx * .8 if sx > sy else sx * .55, sy * .55 if sx > sy else sy * .8, .22), tile=6, bottom=False)
    sp.box('walnut', (0, 0, -.82), (2 * HX + .4, 2 * hy - 1.4, .1), tile=8)
    sp.grid('runeband', (-hx * .78, -hy - .76, TZ + .55), (hx * 1.56, 0, 0), (0, 0, 1.2), 2, 1, (1, 1), uvo=(0, 1))
    pl = 'plaque' + key
    w_, h_ = 13, 1.7
    yv = -hy - .95
    sp.quad(pl, (-w_ / 2, yv, .85 - h_ / 2), (w_ / 2, yv, .85 - h_ / 2), (w_ / 2, yv, .85 + h_ / 2), (-w_ / 2, yv, .85 + h_ / 2), (0, 1), (1, 1), (1, 0), (0, 0))
    for sx in (-1, 1):
        for sy in (-1, 1):
            sp.lathe('gilt', (sx * (hx + .1), sy * (hy + .1), TZ + .4), [(1.2, 0), (.8, 1.3), (1.1, 2.0), (.6, 2.8), (.04, 3.8)], 6, tile=3)
    frame = sp.soup()
    # contents
    cm = np.array([m.startswith(key + ':') for m in s['mat']])
    c = L.xform(L.sub(s, cm), L.S(.85) @ L.T(-ox, -oy, 0))
    c['C'] = None
    two = c['mat'] == key + ':rose'
    if two.any():
        import glass
        texs = dict(texs)
        texs[key + ':rose'] = glass.rose_window()
        rz = refine_cutout(L.sub(c, two), texs[key + ':rose'], depth=4, mat=key + ':rose')
        c = L.cat(L.sub(c, ~two), rz, L.flipped(rz, .02))
    extra = []
    if key == 'r':
        # canopies for the two preview trees (their leaf cards were translucent and never baked)
        rng = np.random.default_rng(9)
        cs = Soup()
        for (x, y, h, r) in ((5, 6, 10, 3.4), (-7, 7, 7, 2.5)):
            for dx, dy, dz, rr in ((0, 0, 0, 1.0), (r * .45, -r * .2, -r * .35, .7), (-r * .4, r * .25, -r * .25, .65)):
                V, F = rock(rng, ((x + dx) * .85, (y - 5 + dy) * .85, (h - .6 + dz) * .85), r * rr * .85, (1, 1, .78), 1)
                BP.add_mesh(cs, 'leaf', V, F, 2.0)
        extra.append(cs.soup())
    d = L.cat(frame, c, *extra)
    d['C'] = None
    d = L.xform(d, L.T(0, 0, -TZ))
    lo, hi = L.bbox(d)
    sc = .6 / (hi[0] - lo[0])
    d = L.xform(d, L.S(sc) @ L.Rz(90))
    lo, hi = L.bbox(d)
    print(name, 'size m', (hi - lo).round(3), 'scale', round(sc, 5))
    tx = {k_: texs[k_] for k_ in set(d['mat']) if k_ in texs}
    if 'leaf' in set(d['mat']):
        leaf = TEX['root/moss'].copy()
        leaf[..., :3] = np.clip(leaf[..., :3] * np.array([1.25, 1.45, .9]), 0, 1)
        tx['leaf'] = leaf
    tints = {k_: v for k_, v in TINT.items() if k_ in tx}
    d, A, _ = L.build_atlas(d, tx, size=1024, pad=4, tints=tints, repeat={'gilt': (4, 4), 'giltorn': (2, 2), 'v:pier': (2, 2), 'r:bark': (2, 4), 'v:wall': (2, 2), 'leaf': (1, 1)})
    bpy.ops.wm.read_factory_settings(use_empty=True)
    ob = L.make_object(name, d, A)
    L.export_glb(ob)


if __name__ == '__main__':
    L.open_blend('mission')
    s, texs = L.soup_from_object(bpy.data.objects['world'], light=L.bake_light(1.05, .58))
    tabletop(s, texs)
    diorama(s, texs, 'v', 0, 0, 'SM_Diorama_Vigil')
    diorama(s, texs, 'r', -33, 5, 'SM_Diorama_Root')
