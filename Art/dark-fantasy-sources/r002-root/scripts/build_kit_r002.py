"""Drowned Vigil obstacle kit, scatter set and mist / light-shaft meshes, r002 (issue #145).

Run from the repository root (one Blender process, about a minute):
    blender -b --python Art/dark-fantasy-sources/r002/scripts/build_kit_r002.py [-- name ...]

Every piece is authored in metres, +X forward, Z up, with the pivot at the centre of its cell's floor (z = 0, x = y = 0), at
true scale for a 150 cm cell (a 180 cm figure stands next to it).  Textures are r001's Vigil swatches (read from
Art/dark-fantasy-sources/r001/vigil.blend, never modified) plus small procedural ones.  Each glb carries its own small atlas
(r001 convention), vertex colour RGBA: RGB = baked ambient occlusion x height gradient x moss tint, A = glow mask (candle
flames) or, on mist and shaft cards, opacity.  Plain meshes, no Nanite needs: the largest kit piece is a few thousand triangles.

Outputs  Art/<asset-id>/r00N/<SM_name>.glb  (see ASSETS) and  Art/dark-fantasy-sources/r002/kit_metrics.json
"""
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import r002_lib as R
from r002_lib import L, bpy, np, Vector, make_bvh, tri_normals, smoothstep, subdivide, bake_ao, tri_soup, vnoise

from prims import Soup, rock

ROOT = R.ROOT
OUT_SRC = os.path.join(ROOT, 'Art', 'dark-fantasy-sources', 'r002')
PAD = 8
METRICS = {}


# ------------------------------------------------------------------ textures
def hexc(h):
    return np.array([int(h[i:i + 2], 16) / 255 for i in (1, 3, 5)])


def rgba(rgb):
    t = np.ones((*rgb.shape[:2], 4), np.float32)
    t[..., :3] = rgb
    return t


def soft_noise(n, cells, seed):
    return R.tile_noise(n, cells, seed)


def procedural_textures():
    out = {}
    n = 64
    big, mid = soft_noise(n, 4, 1), soft_noise(n, 12, 2)
    out['moss'] = rgba(np.array((.12, .23, .09)) * (.65 + .55 * big[..., None]) * (.9 + .2 * mid[..., None]))
    out['bone'] = rgba(np.array((.80, .76, .66)) * (.82 + .22 * soft_noise(n, 5, 3)[..., None]) * (.94 + .1 * np.sin(np.arange(n) / n * 40)[None, :, None]))
    out['root'] = rgba(np.array((.30, .20, .12)) * (.7 + .5 * soft_noise(n, 3, 4)[..., None]) * (.8 + .3 * np.sin(np.arange(n) / n * 62)[None, :, None]))
    yy = np.linspace(0, 1, n)[:, None]
    water = np.array((.05, .11, .14)) + np.array((.10, .16, .20)) * (1 - yy)[..., None] * (.6 + .7 * soft_noise(n, 3, 5)[..., None])
    out['water'] = rgba(np.broadcast_to(water, (n, n, 3)).copy())
    out['clay'] = rgba(np.array((.50, .28, .18)) * (.8 + .3 * soft_noise(n, 4, 6)[..., None]))
    out['wax'] = L.swatch((.86, .80, .64))
    out['flame'] = L.swatch((1.0, .62, .25))
    # mist: soft cloud density, white-blue; the card's vertex alpha supplies the edge fade
    d = .5 * soft_noise(128, 3, 7) + .3 * soft_noise(128, 8, 8) + .2 * soft_noise(128, 20, 9)
    v = np.clip(.35 + 1.2 * (d - .3), 0, 1)
    m = np.empty((128, 128, 4), np.float32)
    m[..., :3] = (np.array((.72, .78, .88)) * (.55 + .45 * v[..., None]))
    m[..., 3] = 1
    out['mist'] = m
    g = np.linspace(1, .55, 64)[:, None, None]                  # shaft: bright at the top, warm
    out['shaft'] = rgba(np.broadcast_to(np.array((.80, .88, 1.0)) * g, (64, 8, 3)).copy())
    return out


def source_textures():
    bpy.ops.wm.open_mainfile(filepath=os.path.join(R.R001, 'vigil.blend'))
    t = {m.name: L.material_texture(m) for m in bpy.data.materials if m.name in ('sstone', 'pier', 'wall', 'iron', 'timber', 'gold')}
    return t


# ------------------------------------------------------------------ geometry helpers
def Ry(deg):
    a = math.radians(deg)
    M = np.eye(4)
    M[0, 0], M[0, 2], M[2, 0], M[2, 2] = math.cos(a), math.sin(a), -math.sin(a), math.cos(a)
    return M


def add_rock(sp, mat, rng, c, r, squash=(1, 1, .7), tile=.5, detail=1, lean=0.0, sink=True):
    V, F = rock(rng, c, r, squash, detail)
    if lean:
        V[:, 0] += (V[:, 2] - c[2]) * lean
    if sink:
        V[:, 2] = np.maximum(V[:, 2], 0)
    for f in F:
        p = V[list(f)]
        n = np.cross(p[1] - p[0], p[2] - p[0])
        ax = int(np.argmax(np.abs(n)))
        a, b = [i for i in range(3) if i != ax]
        uv = [(q[a] / tile, -q[b] / tile) for q in p]
        sp.tri(mat, *[tuple(q) for q in p], *uv)


def slab(sp, mfront, mtop, x0, x1, samples, T=1.1, caps=True):
    """Wall slab of thickness x0..x1 whose outline follows samples [(y, z_low, z_high)], a polyline along Y.  Repeated y with
    a different z_high makes a vertical step.  Front is +X."""
    for (ya, la, ha), (yb, lb, hb) in zip(samples, samples[1:]):
        if abs(yb - ya) > 1e-6:
            fr = [(x1, ya, la), (x1, yb, lb), (x1, yb, hb), (x1, ya, ha)]
            sp.quad(mfront, *fr, *[(q[1] / T, -q[2] / T) for q in fr])
            bk = [(x0, yb, lb), (x0, ya, la), (x0, ya, ha), (x0, yb, hb)]
            sp.quad(mfront, *bk, *[(q[1] / T, -q[2] / T) for q in bk])
            tp = [(x0, ya, ha), (x1, ya, ha), (x1, yb, hb), (x0, yb, hb)]
            sp.quad(mtop, *tp, *[(q[0] / .6, q[1] / .6) for q in tp])
            if la > 1e-3 or lb > 1e-3:
                bt = [(x1, ya, la), (x0, ya, la), (x0, yb, lb), (x1, yb, lb)]
                sp.quad(mtop, *bt, *[(q[0] / .6, q[1] / .6) for q in bt])
        else:
            lo, hi = sorted((ha, hb))
            if hi - lo > 1e-4:
                q = [(x1, ya, lo), (x0, ya, lo), (x0, ya, hi), (x1, ya, hi)]
                if hb > ha:
                    q = [q[1], q[0], q[3], q[2]]
                sp.quad(mfront, *q, *[(p[0] / T, -p[2] / T) for p in q])
    if caps:
        for (y, lo, hi), sgn in ((samples[0], -1), (samples[-1], 1)):
            q = [(x1, y, lo), (x0, y, lo), (x0, y, hi), (x1, y, hi)]
            if sgn < 0:
                q = [q[1], q[0], q[3], q[2]]
            sp.quad(mfront, *q, *[(p[0] / T, -p[2] / T) for p in q])


def rubble(sp, rng, centres, mat='sstone', tile=.4):
    for c, r in centres:
        add_rock(sp, mat, rng, c, r, (1.2, 1.0, .65), tile, detail=0)


# ------------------------------------------------------------------ obstacle kit: pillars
def pillar_a():
    """Broken column on a plinth, jagged top: 2.9 m."""
    rng = np.random.default_rng(5)
    sp = Soup()
    sp.box('sstone', (0, 0, .15), (1.1, 1.1, .3), tile=.55)
    prof = [(.52, .3), (.44, .55), (.40, 1.2), (.38, 2.4)]
    sp.lathe('pier', (0, 0, 0), prof, 10, tile=1.1, cap=(False, False))
    sides, r0, z0 = 10, .38, 2.4
    top = [(r0 * .96 * math.cos(math.tau * j / sides - 1.57), r0 * .96 * math.sin(math.tau * j / sides - 1.57), z0 + rng.uniform(.05, .45)) for j in range(sides)]
    ring = [(r0 * math.cos(math.tau * j / sides - 1.57), r0 * math.sin(math.tau * j / sides - 1.57), z0) for j in range(sides)]
    for j in range(sides):
        a, b, c, d = ring[j], ring[(j + 1) % sides], top[(j + 1) % sides], top[j]
        sp.quad('pier', a, b, c, d, (j / sides * 2, .74), ((j + 1) / sides * 2, .74), ((j + 1) / sides * 2, .9), (j / sides * 2, .9))
    peak = (.08, -.05, z0 + .55)
    for j in range(sides):
        a, b = top[j], top[(j + 1) % sides]
        sp.tri('sstone', peak, a, b, (.5, .5), (a[0] + .5, a[1] + .5), (b[0] + .5, b[1] + .5))
    rubble(sp, rng, [((.62, .22, .05), .14), ((.5, -.45, .04), .11), ((-.58, .3, .04), .1), ((-.3, -.62, .04), .08)])
    return sp.soup(), dict(moss=.35)


def pillar_b():
    """Intact fluted column with capital, 3.4 m, base ring and a fallen capital stone."""
    rng = np.random.default_rng(6)
    sp = Soup()
    sp.box('sstone', (0, 0, .12), (1.0, 1.0, .24), tile=.5)
    sp.lathe('sstone', (0, 0, .24), [(.50, 0), (.44, .06), (.40, .14)], 12, tile=.8, cap=(False, False))
    flute = [[.93 if k % 2 else 1.0 for k in range(12)] for _ in range(4)]
    rings = [(0, 0, .38, .40, .40), (0, 0, .9, .37, .37), (0, 0, 2.4, .34, .34), (0, 0, 3.0, .35, .35)]
    sp.loft('pier', rings, 12, (0, 0, 3, 3), jitter=flute[:4], cap=(False, False))
    sp.lathe('sstone', (0, 0, 3.0), [(.35, 0), (.47, .1), (.55, .22)], 12, tile=.8, cap=(False, False))
    sp.box('sstone', (0, 0, 3.32), (1.05, 1.05, .16), tile=.5)
    add_rock(sp, 'sstone', rng, (.5, -.45, .16), .17, (1.2, 1.0, .7), .5, detail=0)
    rubble(sp, rng, [((.5, .5, .05), .09), ((-.55, -.4, .04), .08)])
    return sp.soup(), dict(moss=.25)


def pillar_c():
    """Stump with an iron band and two fallen drums, 1.7 m."""
    rng = np.random.default_rng(7)
    sp = Soup()
    sp.lathe('pier', (0, 0, 0), [(.56, 0), (.50, .5), (.47, 1.2), (.46, 1.45)], 9, tile=1.1, cap=(False, False))
    sp.lathe('iron', (0, 0, .55), [(.515, 0), (.515, .09), (.50, .09)], 9, tile=.6, cap=(False, False))
    sides, r0, z0 = 9, .46, 1.45
    top = [(r0 * .95 * math.cos(math.tau * j / sides - 1.57), r0 * .95 * math.sin(math.tau * j / sides - 1.57), z0 + rng.uniform(.0, .28)) for j in range(sides)]
    ring = [(r0 * math.cos(math.tau * j / sides - 1.57), r0 * math.sin(math.tau * j / sides - 1.57), z0) for j in range(sides)]
    for j in range(sides):
        sp.quad('pier', ring[j], ring[(j + 1) % sides], top[(j + 1) % sides], top[j], (j / sides * 2, .74), ((j + 1) / sides * 2, .74), ((j + 1) / sides * 2, .9), (j / sides * 2, .9))
        sp.tri('sstone', (.0, .0, z0 + .3), top[j], top[(j + 1) % sides], (.5, .5), (top[j][0] + .5, top[j][1] + .5), (top[(j + 1) % sides][0] + .5, top[(j + 1) % sides][1] + .5))
    d = Soup()
    d.lathe('pier', (0, 0, 0), [(.24, 0), (.24, .5)], 8, tile=1.0, cap=(True, True))
    ds = d.soup()
    ds = L.xform(ds, Ry(90) @ L.T(0, 0, -.25))
    a = L.xform(ds, L.T(.3, -.4, .24))
    b = L.xform(L.xform(ds, L.Rz(70)), L.T(-.35, .4, .24))
    rubble(sp, rng, [((-.6, -.3, .04), .1), ((-.5, .5, .04), .08)])
    return L.cat(sp.soup(), a, b), dict(moss=.5)


# ------------------------------------------------------------------ obstacle kit: walls
def wall_a():
    """Stepped broken wall segment, 1.5 m long, 0.5 m thick, up to 2.1 m."""
    rng = np.random.default_rng(3)
    sp = Soup()
    sp.box('sstone', (0, 0, .09), (.62, 1.5, .18), tile=.6, bottom=False)
    hs = [1.55, 1.95, 1.7, 1.2, .8]
    w = 1.44 / len(hs)
    samples = [(-.72, .18, .18 + hs[0])]
    for i, h in enumerate(hs):
        y0, y1 = -.72 + i * w, -.72 + (i + 1) * w
        samples += [(y0, .18, .18 + h), (y1, .18, .18 + h)]
    samples = [samples[0]] + samples[1:]
    slab(sp, 'wall', 'sstone', -.24, .24, samples)
    rubble(sp, rng, [((.5, .55, .06), .12), ((.55, .1, .05), .09), ((-.5, -.62, .05), .1)])
    return sp.soup(), dict(moss=.2)


def wall_b():
    """Wall with a window slit, lintel and a buttress, 2.3 m."""
    rng = np.random.default_rng(8)
    sp = Soup()
    sp.box('sstone', (0, 0, .09), (.66, 1.5, .18), tile=.6, bottom=False)
    z0, zw0, zw1, zt = .18, 1.2, 1.75, 2.35
    slab(sp, 'wall', 'sstone', -.22, .22, [(-.72, z0, zw0), (.72, z0, zw0)], caps=True)
    slab(sp, 'wall', 'sstone', -.22, .22, [(-.72, zw0, zt - .1), (-.1, zw0, zt - .1)])
    slab(sp, 'wall', 'sstone', -.22, .22, [(.1, zw0, zt), (.72, zw0, zt - .25)])
    slab(sp, 'wall', 'sstone', -.22, .22, [(-.1, zw1, zt - .1), (.1, zw1, zt)])
    # buttress with a sloped cap on the +X face
    b = [(.22, .5, .18), (.22, .5 + .0, .18)]
    sp.box('sstone', (.4, -.4, .75), (.36, .56, 1.2), tile=.6, bottom=False)
    wedge = [(.22, -.68, 1.35), (.58, -.68, 1.35), (.58, -.12, 1.35), (.22, -.12, 1.35)]
    sp.quad('sstone', *wedge, (0, 0), (1, 0), (1, 1), (0, 1))
    rubble(sp, rng, [((.55, .45, .06), .13), ((.45, .62, .05), .08), ((-.5, -.55, .05), .1)])
    return sp.soup(), dict(moss=.3)


def wall_c():
    """Low crumbled wall with a rubble heap, 1.1 m."""
    rng = np.random.default_rng(9)
    sp = Soup()
    samples = [(-.72, 0, .35), (-.55, 0, .35), (-.55, 0, .75), (-.2, 0, 1.05), (-.2, 0, .6), (.1, 0, .6), (.1, 0, .9), (.4, 0, .9), (.4, 0, .45), (.72, 0, .45)]
    slab(sp, 'wall', 'sstone', -.2, .2, samples, T=1.0)
    rubble(sp, rng, [((.38, .35, .12), .2), ((.5, -.1, .08), .15), ((.3, -.5, .06), .12), ((-.4, .5, .08), .14), ((.6, .55, .05), .09), ((-.55, -.5, .06), .12)])
    add_rock(sp, 'wall', rng, (.45, -.1, .2), .22, (1.4, .9, .6), .8, detail=0)
    return sp.soup(), dict(moss=.45)


# ------------------------------------------------------------------ obstacle kit: stones
def stones_a():
    """Split menhir pair with small stones, 1.4 m."""
    rng = np.random.default_rng(11)
    sp = Soup()
    for c, r, sq, side in [((.05, .2, .62), .62, (1.0, .62, 1.0), 'neg'), ((.05, -.2, .62), .62, (1.0, .62, 1.0), 'pos')]:
        V, F = rock(rng, c, r, sq, 1)
        V[:, 1] = np.maximum(V[:, 1], c[1] - .1) if side == 'neg' else np.minimum(V[:, 1], c[1] + .1)
        V[:, 2] = np.maximum(V[:, 2], 0)
        for f in F:
            p = V[list(f)]
            n = np.cross(p[1] - p[0], p[2] - p[0])
            ax = int(np.argmax(np.abs(n)))
            a, b = [i for i in range(3) if i != ax]
            sp.tri('menhir', *[tuple(q) for q in p], *[(q[a] / .6, -q[b] / .6) for q in p])
    for c, r in [((-.5, .5, .16), .22), ((.55, -.55, .12), .17), ((-.55, -.4, .1), .14)]:
        add_rock(sp, 'menhir', rng, c, r, (1, .9, .7), .5, detail=1)
    return sp.soup(), dict(moss=.3)


def stones_b():
    """Three leaning standing stones, up to 2.0 m."""
    rng = np.random.default_rng(12)
    sp = Soup()
    for c, r, sq, lean in [((-.1, .1, .95), .62, (.38, .3, 1.55), .12), ((.3, -.35, .75), .5, (.42, .3, 1.5), -.18), ((-.25, -.5, .55), .38, (.5, .35, 1.4), .25)]:
        add_rock(sp, 'menhir', rng, c, r, sq, .6, detail=1, lean=lean)
    rubble(sp, rng, [((.5, .4, .06), .1), ((-.55, .4, .05), .09)], mat='menhir')
    return sp.soup(), dict(moss=.4)


def stones_c():
    """Mossy boulder cluster, 1.0 m."""
    rng = np.random.default_rng(13)
    sp = Soup()
    for c, r, sq in [((0, 0, .3), .5, (1.1, .95, .75)), ((.38, .33, .2), .3, (1, 1, .7)), ((-.4, .3, .18), .28, (1, .9, .7)), ((.1, -.48, .16), .24, (1.1, .9, .6)), ((-.48, -.35, .1), .18, (1, 1, .6))]:
        add_rock(sp, 'menhir', rng, c, r, sq, .55, detail=1)
    return sp.soup(), dict(moss=.75)


# ------------------------------------------------------------------ 2-cell wall / arch (spans two adjacent cells along Y)
def arch_2cell():
    """3.0 m x 0.6 m blocked gate arch: two piers, a pointed arch with an iron portcullis and rubble, jagged top at ~3 m.
    Pivot at the centre of the pair's floor (the shared cell edge); long axis along Y, rotate 90 degrees for X-adjacent pairs."""
    rng = np.random.default_rng(21)
    sp = Soup()
    sp.box('sstone', (0, 0, .09), (.78, 3.0, .18), tile=.6, bottom=False)
    zs, a, R_ = .95, 1.0, 2.0                        # springline height, half span, arch radius
    def intr(y):
        y = abs(y)
        return zs + math.sqrt(max(R_ ** 2 - (y + a) ** 2, 0)) if y < a else zs
    top_noise = lambda y: 3.05 + .22 * math.sin(y * 3.1) - (.35 if y > 0.9 else 0) + .18 * vnoise(y + 3, 0.5, 2.3, 4)
    ys = list(np.arange(-1.5, 1.5 + 1e-6, .25))
    samples = []
    for y in ys:
        lo = intr(y) if abs(y) < a else .18
        samples.append((float(y), float(lo), float(max(top_noise(y), lo + .5))))
    # outer pier faces step in a little at both ends
    slab(sp, 'wall', 'sstone', -.27, .27, samples, T=1.1)
    # portcullis: vertical bars and two cross bars, set back in the opening
    for y in np.arange(-.9, .91, .2):
        zt = intr(y) - .04
        sp.tube('iron', [(.04, float(y), .18), (.04, float(y), float(zt))], [.022, .022], 4, tile=.5)
    for z in (.5, 1.1):
        sp.tube('iron', [(.04, -.99, z), (.04, .99, z)], [.025, .025], 4, tile=.5, cap=(True, True))
    rubble(sp, rng, [((.48, .0, .14), .24), ((.5, -.5, .1), .18), ((.5, .45, .09), .16), ((-.5, .15, .1), .18), ((-.48, -.4, .07), .14), ((.45, 1.1, .06), .12)])
    add_rock(sp, 'wall', rng, (.4, 0.2, .3), .24, (1.2, 1.0, .7), .8, detail=1)
    return sp.soup(), dict(moss=.2, edge=.9)


# ------------------------------------------------------------------ scatter
def sc_rubble():
    rng = np.random.default_rng(31)
    sp = Soup()
    for c, r, sq in [((0, 0, .06), .24, (1.2, 1, .55)), ((.3, .2, .04), .16, (1, 1, .6)), ((-.25, .22, .03), .13, (1.1, .9, .6)), ((.1, -.3, .03), .15, (1, 1.1, .5)), ((-.3, -.18, .02), .1, (1, 1, .6)), ((.38, -.14, .02), .09, (1, 1, .6))]:
        add_rock(sp, 'sstone', rng, c, r, sq, .4, detail=0)
    return sp.soup(), dict(moss=.15)


def sc_candles():
    rng = np.random.default_rng(32)
    sp = Soup()
    sp.lathe('sstone', (0, 0, 0), [(.30, 0), (.28, .03), (.2, .045)], 9, tile=.5, cap=(True, True))
    for (x, y, h) in [(.0, .0, .22), (.12, .1, .14), (-.1, .12, .1)]:
        sp.lathe('wax', (x, y, .04), [(.032, 0), (.034, h * .5), (.03, h)], 6, tile=.1, cap=(True, True))
        sp.lathe('wax', (x, y, .04 + h), [(.03, 0), (.008, .004)], 6, cap=(False, False))
        fl = Soup()
        fl.loft('flame', [(x, y, .04 + h + .005, .002, .002), (x, y, .04 + h + .02, .018, .018), (x, y, .04 + h + .05, .012, .012), (x, y, .04 + h + .085, .001, .001)], 5, (0, 0, 1, 1), cap=(True, True))
        sp.P += fl.P; sp.UV += fl.UV; sp.M += fl.M
    for c, r in [((.2, -.1, .015), .05), ((-.2, -.15, .01), .04)]:
        add_rock(sp, 'sstone', rng, c, r, (1, 1, .5), .3, detail=0)
    s = sp.soup()
    s['UV'][s['mat'] == 'flame'] = .5
    return s, dict(glow=('flame',))


def sc_bones():
    rng = np.random.default_rng(33)
    sp = Soup()
    add_rock(sp, 'bone', rng, (.18, .12, .09), .1, (1.0, .85, .85), .25, detail=1)                       # skull
    add_rock(sp, 'bone', rng, (.28, .12, .05), .05, (1.1, .8, .5), .25, detail=0)                         # jaw
    for (p0, p1) in [((-.3, -.2, .03), (.1, -.25, .03)), ((-.25, .05, .03), (.05, .25, .03)), ((-.1, -.05, .025), (.2, -.12, .03))]:
        sp.tube('bone', [p0, ((p0[0] + p1[0]) / 2, (p0[1] + p1[1]) / 2, .028), p1], [.03, .017, .03], 6, tile=.3, cap=(True, True))
    for k in range(5):                                                                                       # ribs, half arcs lying down
        y = -.1 + k * .06
        pts = [(-.35 + .0, y, .02 + .1 * math.sin(t)) for t in np.linspace(0, math.pi * .85, 6)]
        pts = [(-.5 + .25 * (1 - math.cos(t)) * .6, y + .05 * math.sin(t), .015 + .12 * math.sin(t)) for t in np.linspace(0, math.pi, 7)]
        sp.tube('bone', pts, [.012] * 7, 4, tile=.3, cap=(True, True))
    return sp.soup(), dict()


def sc_roots():
    rng = np.random.default_rng(34)
    sp = Soup()
    def branch(p0, ang, length, r0, depth):
        pts, rad = [], []
        x, y, a = p0[0], p0[1], ang
        n = 7
        for i in range(n):
            t = i / (n - 1)
            pts.append((x, y, max(.02, r0 * (1 - t) + .02 + .04 * math.sin(t * 5 + ang))))
            rad.append(max(r0 * (1 - .85 * t), .006))
            a += rng.uniform(-.35, .35)
            x += math.cos(a) * length / (n - 1)
            y += math.sin(a) * length / (n - 1)
        sp.tube('root', pts, rad, 5, tile=.3, cap=(True, True))
        if depth:
            for k in (2, 4):
                branch((pts[k][0], pts[k][1]), a + rng.choice((-1, 1)) * .8, length * .55, rad[k] * .8, depth - 1)
    for ang in (0.4, 2.2, 3.7, 5.2):
        branch((0, 0), ang, .62, .06, 1)
    return sp.soup(), dict(moss=.1)


def sc_weapons():
    """Broken sword and a spear stub lying on the floor."""
    rng = np.random.default_rng(35)
    sp = Soup()
    add_rock(sp, 'sstone', rng, (-.1, -.25, .02), .08, (1, 1, .5), .3, detail=0)
    bl = [(-.55, -.12, .035), (-.2, -.18, .03), (.12, -.24, .026), (.3, -.27, .02)]
    sp.tube('iron', bl, [.045, .04, .028, .004], 4, tile=.4, cap=(True, True))                     # blade (diamond section)
    sp.box('iron', (-.62, -.1, .03), (.05, .22, .05), tile=.3)                                    # crossguard
    sp.tube('timber', [(-.7, -.09, .03), (-.82, -.07, .03)], [.022, .02], 6, tile=.3, cap=(True, True))
    sp.lathe('gold', (-.84, -.07, .03), [(.03, 0), (.01, .02)], 6, cap=(True, False))
    sp.tube('timber', [(-.2, .3, .035), (.35, .22, .035), (.55, .2, .04)], [.022, .022, .02], 6, tile=.3, cap=(True, True))      # spear shaft
    sp.tube('iron', [(.55, .2, .04), (.62, .19, .04), (.7, .18, .042)], [.05, .035, .004], 4, tile=.3, cap=(True, True))
    s = L.xform(sp.soup(), L.T(.1, 0, 0))
    return s, dict()


def sc_puddle():
    sp = Soup()
    seg = 20
    rng = np.random.default_rng(36)
    rad = .5 + .08 * np.sin(np.arange(seg) * 1.7) + rng.uniform(-.04, .04, seg)
    z = .012
    pts = [(rad[i] * math.cos(math.tau * i / seg) * 1.15, rad[i] * math.sin(math.tau * i / seg) * .9, z) for i in range(seg)]
    for i in range(seg):
        a, b = pts[i], pts[(i + 1) % seg]
        sp.tri('water', (0, 0, z), a, b, (.5, .5), (.5 + a[0], .5 + a[1]), (.5 + b[0], .5 + b[1]))
    s = sp.soup()
    n = tri_normals(s['P'])
    f = n[:, 2] < 0
    s['P'][f] = s['P'][f][:, [0, 2, 1]]; s['UV'][f] = s['UV'][f][:, [0, 2, 1]]
    return s, dict()


def sc_urn():
    rng = np.random.default_rng(37)
    sp = Soup()
    sp.lathe('clay', (0, 0, 0), [(.12, 0), (.2, .12), (.22, .26), (.16, .38), (.1, .44)], 8, tile=.5, cap=(True, False))
    sp.lathe('clay', (0, 0, 0), [(.095, .44), (.12, .46)], 8, cap=(False, False))
    for c, r in [((.4, .1, .02), .09), ((.3, -.3, .02), .08), ((-.35, -.2, .015), .07), ((-.2, .38, .015), .06), ((.1, .45, .01), .05)]:
        add_rock(sp, 'clay', rng, c, r, (1.3, 1, .3), .3, detail=0)
    return sp.soup(), dict()


def sc_moss():
    sp = Soup()
    rng = np.random.default_rng(38)
    seg = 18
    rad = .55 + .12 * np.sin(np.arange(seg) * 2.3) + rng.uniform(-.05, .05, seg)
    for i in range(seg):
        a0, a1 = math.tau * i / seg, math.tau * (i + 1) / seg
        p0 = (rad[i] * math.cos(a0), rad[i] * math.sin(a0), .01)
        p1 = (rad[(i + 1) % seg] * math.cos(a1), rad[(i + 1) % seg] * math.sin(a1), .01)
        sp.tri('moss', (0, 0, .03), p0, p1, (.5, .5), (.5 + p0[0], .5 + p0[1]), (.5 + p1[0], .5 + p1[1]))
    s = sp.soup()
    f = tri_normals(s['P'])[:, 2] < 0
    s['P'][f] = s['P'][f][:, [0, 2, 1]]; s['UV'][f] = s['UV'][f][:, [0, 2, 1]]
    return s, dict(moss=.0)


# ------------------------------------------------------------------ fx cards (opacity in COLOR_0.a)
def fx_cards(name):
    """Returns (soup, alpha (n,3), colour (n,3,3)).  Two-sided: each card is doubled with reversed winding."""
    parts, alphas = [], []

    def card(origin, du, dv, nu, nv, alpha_fn, uv0=(0, 0), uvs=(1, 1)):
        o, du, dv = map(lambda v: np.array(v, float), (origin, du, dv))
        pts = lambda i, j: o + du * i / nu + dv * j / nv
        tris, uvl, al = [], [], []
        for i in range(nu):
            for j in range(nv):
                q = [(i, j), (i + 1, j), (i + 1, j + 1), (i, j + 1)]
                P = [pts(*k) for k in q]
                U = [(uv0[0] + uvs[0] * k[0] / nu, uv0[1] + uvs[1] * k[1] / nv) for k in q]
                A = [alpha_fn(k[0] / nu, k[1] / nv) for k in q]
                for t in ((0, 1, 2), (0, 2, 3)):
                    tris.append([P[x] for x in t]); uvl.append([U[x] for x in t]); al.append([A[x] for x in t])
        s = dict(P=np.array(tris), UV=np.array(uvl), C=None, mat=np.array(['mist' if name.startswith('mist') else 'shaft'] * len(tris), dtype=object))
        return s, np.array(al)

    def doubled(s, al):
        f = L.flipped(s, .004)
        return L.cat(s, f), np.concatenate([al, al[:, [0, 2, 1]]])

    soft = lambda u, v, p=1.0: float(smoothstep(0, .5, min(u, 1 - u)) * smoothstep(0, .5, min(v, 1 - v))) ** p
    if name == 'mist_a':        # 3.0 x 1.1 m upright card, thickest near the floor
        s, a = card((0, -1.5, 0), (0, 3.0, 0), (0, 0, 1.1), 6, 3, lambda u, v: .55 * soft(u, v) * (1 - .5 * v), uvs=(1, 1))
        s['P'][..., 0] += 0.0
    elif name == 'mist_b':      # 2.2 x 2.2 m card tilted back 15 degrees, pivot at the base centre
        s, a = card((0, -1.1, 0), (-.57, 0, 2.12) if False else (0, 2.2, 0), (-.57, 0, 2.12), 4, 4, lambda u, v: .5 * soft(u, v), uv0=(.2, .1), uvs=(.6, .6))
    else:                       # mist_c: 3.0 x 3.0 m flat card 12 cm above the floor
        s, a = card((-1.5, -1.5, .12), (3.0, 0, 0), (0, 3.0, 0), 6, 6, lambda u, v: .6 * soft(u, v, .8), uv0=(.0, .3), uvs=(.7, .7))
        s['P'] = s['P'][:, [0, 2, 1]] if False else s['P']
    s, a = doubled(s, a)
    col = np.ones((len(s['P']), 3, 3)) * np.array((.78, .84, .94))
    return s, a, col


def fx_shaft(kind):
    """kind 'prism': 4-sided open tapered prism, 6.0 m tall, leaning +X (light travelling down toward +X); pivot at the lit
    floor patch centre.  kind 'cross': two crossing planes, 4.2 m tall.  Opacity is brightest at the top and fades to 0 at the
    floor and at the card edges."""
    tris, uvl, al = [], [], []
    if kind == 'prism':
        H, rb, rt, lean = 6.0, .75, .45, -2.6                       # top is at x = -2.6 (upper side of the window)
        ring = lambda z, r, cx: [(cx + r * math.cos(a), r * math.sin(a), z) for a in (0.785, 2.356, 3.927, 5.498)]
        nz = 6
        for k in range(4):
            for j in range(nz):
                t0, t1 = j / nz, (j + 1) / nz
                b0, b1 = ring(t0 * H, rb + (rt - rb) * t0, lean * t0), ring(t1 * H, rb + (rt - rb) * t1, lean * t1)
                P = [b0[k], b0[(k + 1) % 4], b1[(k + 1) % 4], b1[k]]
                U = [(k / 4, 1 - t0), ((k + 1) / 4, 1 - t0), ((k + 1) / 4, 1 - t1), (k / 4, 1 - t1)]
                A = [.07 * float(smoothstep(0, .35, t) * smoothstep(1.0, .8, t) * (.35 + .65 * t)) for t in (t0, t0, t1, t1)]
                for t in ((0, 1, 2), (0, 2, 3)):
                    tris.append([P[x] for x in t]); uvl.append([U[x] for x in t]); al.append([A[x] for x in t])
    else:
        H, w, lean = 4.2, .55, -1.8
        nz = 6
        for ang in (0, 90):
            ca, sa = math.cos(math.radians(ang)), math.sin(math.radians(ang))
            for j in range(nz):
                t0, t1 = j / nz, (j + 1) / nz
                for (i0, i1) in ((0, 1),):
                    xs = lambda t, s_: (lean * t + s_ * w * ca, s_ * w * sa)
                    P, U, A = [], [], []
                    for (s_, t) in ((-1, t0), (1, t0), (1, t1), (-1, t1)):
                        x, y = xs(t, s_)
                        P.append((x, y, t * H)); U.append(((s_ + 1) / 2, 1 - t))
                        A.append(.06 * float(smoothstep(0, .3, t) * smoothstep(1.0, .8, t) * (.35 + .65 * t)))
                    # fade to the card edges: only the centre line of each row carries full opacity
                    for t in ((0, 1, 2), (0, 2, 3)):
                        tris.append([P[x] for x in t]); uvl.append([U[x] for x in t]); al.append([A[x] for x in t])
    s = dict(P=np.array(tris), UV=np.array(uvl), C=None, mat=np.array(['shaft'] * len(tris), dtype=object))
    a = np.array(al)
    # two-sided
    f = L.flipped(s, .004)
    s = L.cat(s, f)
    a = np.concatenate([a, a[:, [0, 2, 1]]])
    col = np.ones((len(s['P']), 3, 3)) * np.array((.80, .88, 1.0))
    return s, a, col


# ------------------------------------------------------------------ finishing
def bake_piece(s, moss=0.0, ao_dist=1.1, edge=.45):
    """Subdivide to <= 45 cm edges (conforming), vertex colour = ambient occlusion x height gradient x moss tint."""
    s = subdivide(s, edge)
    # ground plane so the foot of every piece is occluded
    g = tri_soup([[(-4, -4, 0), (4, -4, 0), (4, 4, 0)], [(-4, -4, 0), (4, 4, 0), (-4, 4, 0)]], [[(0, 0)] * 3] * 2, None, 'g')
    bvh = make_bvh(L.cat(dict(s, C=None), dict(g, C=None))['P'])
    s['C'] = np.ones((len(s['P']), 3, 3))
    s, _ = bake_ao(s, bvh, strength=.7, dist=ao_dist, n=12)
    N = tri_normals(s['P'])
    z = s['P'][..., 2]
    grad = .80 + .20 * np.clip(z / 1.6, 0, 1)
    s['C'] = s['C'] * grad[..., None] * (.9 + .1 * np.clip(N[:, 2], 0, 1))[:, None, None]
    if moss > 0:
        xy = s['P'][..., :2]
        m = smoothstep(.35, .7, 1 - np.clip(z / 1.1, 0, 1)) * smoothstep(.3, .7, vnoise(xy[..., 0] + 5, xy[..., 1] + 9, 1.7, 3) + .25 * moss) * moss
        m = m * (.5 + .5 * np.clip(N[:, 2], 0, 1)[:, None])
        s['C'] = s['C'] * (1 + m[..., None] * (np.array((.65, 1.0, .5)) - 1))
    return s


def finish(asset, unreal, rev, name, s, texs, size, opts, repeat=None, alpha=None, bake=True, glow_mats=()):
    out_dir = os.path.join(ROOT, 'Art', asset, f'r{rev:03d}')
    if bake:
        s = bake_piece(s, opts.get('moss', 0.0), edge=opts.get('edge', .45))
    solid = [k for k in set(s['mat'].tolist()) if k in ('wax', 'flame', 'gold') and texs[k].shape[0] <= 8]
    s2, A, place = L.build_atlas(s, texs, size=size, pad=PAD, solid=solid, repeat=repeat or {}, downscale={'iron': 4 if size <= 256 else 2, 'timber': 4 if size <= 256 else 2, 'sstone': 2 if size <= 256 else 1})
    used_px = sum((w + 2 * PAD) * (h + 2 * PAD) for (_, _, w, h) in place.values())
    bpy.ops.wm.read_factory_settings(use_empty=True)
    img = L.new_image('T_' + name[3:], A)
    ob, glow_tris = R.make_object_rgba(name, s2, img, alpha=alpha, glow_mats=glow_mats)
    path = R.export_glb(ob, os.path.join(out_dir, name + '.glb'))
    st = R.glb_stats(path)
    st.update(asset_id=asset, revision=rev, unreal_package=unreal, glow_tris=glow_tris, atlas_px=size, atlas_fill=round(used_px / size / size, 3),
              file=os.path.relpath(path, ROOT))
    METRICS[name] = st
    print('KIT', name, st['triangles'], 'tris', st['size_m'], 'bbox', st['bbox_min_m'], st['bbox_max_m'])


ASSETS = {
    # key: (asset id, revision, unreal path, SM name, builder, atlas px, atlas repeat)
    'pillar_a': ('prop-pillar', 2, '/Game/DnDDiceRoll/Theme/Props/SM_Prop_Pillar', 'SM_Prop_Pillar', pillar_a, 512, {'pier': (1, 2), 'sstone': (2, 2)}),
    'pillar_b': ('prop-pillar-b', 1, '/Game/DnDDiceRoll/Theme/Props/SM_Prop_Pillar_B', 'SM_Prop_Pillar_B', pillar_b, 512, {'pier': (1, 2), 'sstone': (2, 2)}),
    'pillar_c': ('prop-pillar-c', 1, '/Game/DnDDiceRoll/Theme/Props/SM_Prop_Pillar_C', 'SM_Prop_Pillar_C', pillar_c, 512, {'pier': (1, 2), 'sstone': (2, 2)}),
    'wall_a': ('prop-wall', 2, '/Game/DnDDiceRoll/Theme/Props/SM_Prop_Wall', 'SM_Prop_Wall', wall_a, 512, {}),
    'wall_b': ('prop-wall-b', 1, '/Game/DnDDiceRoll/Theme/Props/SM_Prop_Wall_B', 'SM_Prop_Wall_B', wall_b, 512, {}),
    'wall_c': ('prop-wall-c', 1, '/Game/DnDDiceRoll/Theme/Props/SM_Prop_Wall_C', 'SM_Prop_Wall_C', wall_c, 512, {}),
    'stones_a': ('prop-stones', 2, '/Game/DnDDiceRoll/Theme/Props/SM_Prop_Stones', 'SM_Prop_Stones', stones_a, 256, {}),
    'stones_b': ('prop-stones-b', 1, '/Game/DnDDiceRoll/Theme/Props/SM_Prop_Stones_B', 'SM_Prop_Stones_B', stones_b, 256, {}),
    'stones_c': ('prop-stones-c', 1, '/Game/DnDDiceRoll/Theme/Props/SM_Prop_Stones_C', 'SM_Prop_Stones_C', stones_c, 256, {}),
    'arch2': ('prop-arch-2cell', 1, '/Game/DnDDiceRoll/Theme/Props/SM_Prop_Arch2Cell', 'SM_Prop_Arch2Cell', arch_2cell, 512, {}),
    'sc_rubble': ('scatter-rubble', 1, '/Game/DnDDiceRoll/Theme/Scatter/SM_Scatter_Rubble', 'SM_Scatter_Rubble', sc_rubble, 256, {}),
    'sc_candles': ('scatter-candles', 1, '/Game/DnDDiceRoll/Theme/Scatter/SM_Scatter_Candles', 'SM_Scatter_Candles', sc_candles, 256, {}),
    'sc_bones': ('scatter-bones', 1, '/Game/DnDDiceRoll/Theme/Scatter/SM_Scatter_Bones', 'SM_Scatter_Bones', sc_bones, 256, {}),
    'sc_roots': ('scatter-roots', 1, '/Game/DnDDiceRoll/Theme/Scatter/SM_Scatter_Roots', 'SM_Scatter_Roots', sc_roots, 256, {}),
    'sc_weapons': ('scatter-weapons', 1, '/Game/DnDDiceRoll/Theme/Scatter/SM_Scatter_Weapons', 'SM_Scatter_Weapons', sc_weapons, 256, {}),
    'sc_puddle': ('scatter-puddle', 1, '/Game/DnDDiceRoll/Theme/Scatter/SM_Scatter_Puddle', 'SM_Scatter_Puddle', sc_puddle, 256, {}),
    'sc_urn': ('scatter-urn', 1, '/Game/DnDDiceRoll/Theme/Scatter/SM_Scatter_Urn', 'SM_Scatter_Urn', sc_urn, 256, {}),
    'sc_moss': ('scatter-moss', 1, '/Game/DnDDiceRoll/Theme/Scatter/SM_Scatter_Moss', 'SM_Scatter_Moss', sc_moss, 256, {}),
}
FX = {
    'mist_a': ('fx-mist-card', 1, '/Game/DnDDiceRoll/Theme/Fx/SM_Fx_MistCard', 'SM_Fx_MistCard', lambda: fx_cards('mist_a'), 256),
    'mist_b': ('fx-mist-card-b', 1, '/Game/DnDDiceRoll/Theme/Fx/SM_Fx_MistCard_B', 'SM_Fx_MistCard_B', lambda: fx_cards('mist_b'), 256),
    'mist_c': ('fx-mist-card-c', 1, '/Game/DnDDiceRoll/Theme/Fx/SM_Fx_MistCard_C', 'SM_Fx_MistCard_C', lambda: fx_cards('mist_c'), 256),
    'shaft_a': ('fx-light-shaft', 1, '/Game/DnDDiceRoll/Theme/Fx/SM_Fx_LightShaft', 'SM_Fx_LightShaft', lambda: fx_shaft('prism'), 128),
    'shaft_b': ('fx-light-shaft-b', 1, '/Game/DnDDiceRoll/Theme/Fx/SM_Fx_LightShaft_B', 'SM_Fx_LightShaft_B', lambda: fx_shaft('cross'), 128),
}


def main():
    only = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    texs = source_textures()
    texs.update(procedural_textures())
    texs['menhir'] = texs['sstone'].copy()          # r001's menhir came from the Root theme; here the Vigil stone, tinted cooler
    texs['menhir'][..., :3] = np.clip(texs['menhir'][..., :3] * np.array((.78, .8, .84)), 0, 1)
    for key, (asset, rev, unreal, name, fn, size, rep) in ASSETS.items():
        if only and key not in only:
            continue
        s, opts = fn()
        finish(asset, unreal, rev, name, s, texs, size, opts, repeat=rep, glow_mats=opts.get('glow', ()))
    for key, (asset, rev, unreal, name, fn, size) in FX.items():
        if only and key not in only:
            continue
        s, a, col = fn()
        s['C'] = col
        finish(asset, unreal, rev, name, s, texs, size, {}, alpha=a, bake=False)
    if not only:
        with open(os.path.join(OUT_SRC, 'kit_metrics.json'), 'w') as fh:
            json.dump(METRICS, fh, indent=1)


main()
