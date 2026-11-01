"""Props (one 1.10 m cell) and board tiles, authored in metres with the source textures.
Outputs SM_Prop_{Stones,Pillar,Wall,Brazier,Flame,Banner,Cage}.glb and SM_Tile_{Vigil,Root}.glb."""
import math
import sys

sys.path.insert(0, __import__('os').path.dirname(__import__('os').path.abspath(__file__)))
import bpy
import numpy as np

import tlib as L
from prims import Soup, rock

TEX = dict(np.load(f'{L.WORK}/textures.npz'))
S = .55  # source unit -> metres (2 units per 1.10 m cell)


def ramp(stops, n):
    """Smooth colour ramp, stops: list of (t, (r,g,b)) in 0..1 display space."""
    t = np.linspace(0, 1, n)
    out = np.zeros((n, 3))
    for c in range(3):
        out[:, c] = np.interp(t, [s[0] for s in stops], [s[1][c] for s in stops])
    return out


def hexc(h):
    return tuple(int(h[i:i + 2], 16) / 255 for i in (1, 3, 5))


def tex_rgba(rgb):
    t = np.ones((*rgb.shape[:2], 4), np.float32)
    t[..., :3] = rgb
    return t


def banner_tex():
    """64x128 crimson banner: smooth folds, gold border and a gilt sunburst; no noise."""
    w, h = 64, 128
    yy, xx = np.mgrid[0:h, 0:w]
    fold = .5 + .5 * np.sin(xx / w * math.tau * 2.5 + .6)
    shade = .35 + .45 * fold - .12 * yy / h
    pal = ramp([(0, hexc('#100508')), (.35, hexc('#3d0c15')), (.7, hexc('#7c2632')), (1, hexc('#ad6451'))], 256)
    rgb = pal[np.clip((shade * 255).astype(int), 0, 255)]
    gold = ramp([(0, hexc('#553009')), (.5, hexc('#c89c40')), (1, hexc('#efdb9e'))], 256)
    border = (xx < 4) | (xx >= w - 4) | (yy < 5)
    rgb[border] = gold[np.clip(((.45 + .4 * fold[border]) * 255).astype(int), 0, 255)]
    inner = ((xx == 6) | (xx == w - 7)) & (yy > 7)
    rgb[inner] = gold[120]
    # sunburst emblem
    cx, cy = w / 2 - .5, 50
    r = np.hypot(xx - cx, yy - cy)
    a = np.arctan2(yy - cy, xx - cx)
    rays = (r < 9 + 7 * (np.cos(a * 8) > .55)) & (r > 3)
    rgb[rays] = gold[200]
    rgb[r <= 3] = hexc('#efdb9e')
    return tex_rgba(rgb)


def flame_tex():
    """8x64, row 0 = tip: deep red -> orange -> yellow -> hot white core."""
    pal = ramp([(0, hexc('#b8240c')), (.25, hexc('#ff5a10')), (.5, hexc('#ffa030')), (.75, hexc('#ffe080')), (1, hexc('#fffff0'))], 64)
    rgb = np.repeat(pal[:, None, :], 8, 1)
    return tex_rgba(rgb)


def ember_tex():
    """16x16 glowing coals."""
    yy, xx = np.mgrid[0:16, 0:16]
    v = .5 + .25 * np.sin(xx * 1.3 + yy * .7) * np.sin(yy * 1.1 - xx * .4)
    pal = ramp([(0, hexc('#200604')), (.4, hexc('#6a1206')), (.7, hexc('#d0480e')), (1, hexc('#ffb040'))], 256)
    return tex_rgba(pal[np.clip((v * 255).astype(int), 0, 255)])


def gilt_tex():
    return TEX['mission/gilt']


def box_uv(V, F, tile):
    """Per-face planar projection along the dominant normal axis (world-scale UVs)."""
    out = []
    for f in F:
        p = V[list(f)]
        n = np.cross(p[1] - p[0], p[2] - p[0])
        ax = int(np.argmax(np.abs(n)))
        a, b = [i for i in range(3) if i != ax]
        out.append([(q[a] / tile, -q[b] / tile) for q in p])
    return out


def add_mesh(soup, mat, V, F, tile):
    uvs = box_uv(V, F, tile)
    for f, uv in zip(F, uvs):
        soup.tri(mat, *[tuple(V[i]) for i in f], *uv)


def finish(name, s, texs, size=512, **atlas):
    s, A, _ = L.build_atlas(s, texs, size=size, pad=4, **atlas)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    ob = L.make_object(name, s, A)
    L.export_glb(ob)
    lo, hi = L.bbox(s)
    return lo, hi


# ---------------------------------------------------------------- stones
def stones():
    rng = np.random.default_rng(11)
    sp = Soup()
    for (c, r, sq, flat) in [((.04, .12, .30), .36, (1.0, .62, .9), 'neg'), ((.04, -.12, .30), .36, (1.0, .62, .9), 'pos'),
                             ((-.3, .32, .1), .16, (1, .9, .7), None), ((.33, -.34, .08), .13, (1, .85, .65), None),
                             ((-.32, -.28, .07), .1, (1, 1, .7), None)]:
        V, F = rock(rng, c, r, sq, 1)
        if flat == 'neg':          # split face of the boulder halves, gap between them
            V[:, 1] = np.maximum(V[:, 1], c[1] - .09)
        elif flat == 'pos':
            V[:, 1] = np.minimum(V[:, 1], c[1] + .09)
        V[:, 2] = np.maximum(V[:, 2], 0)
        add_mesh(sp, 'menhir', V, F, .55)
    s = sp.soup()
    # lean the halves apart slightly
    return finish('SM_Prop_Stones', s, {'menhir': TEX['root/menhir']}, tints={'menhir': (.78, .78, .8)})


# ---------------------------------------------------------------- pillar
def pillar():
    rng = np.random.default_rng(5)
    sp = Soup()
    k = .5  # source column-stump profile (worlds_pass.vigil) at 0.5 m per unit -> 1.0 m wide
    prof = [(1.0 * k, 0), (1.0 * k, .4 * k), (.7 * k, .6 * k), (.65 * k, 2.2 * k)]
    sp.lathe('pier', (0, 0, 0), prof, 8, tile=1.5, cap=(True, False))
    # jagged break on top: ring of random heights, then a fan to an off-centre peak
    sides = 8
    r0, z0 = .65 * k, 2.2 * k
    top = []
    for j in range(sides):
        a = math.radians(-90) + math.tau * j / sides
        z = z0 + rng.uniform(.04, .26)
        top.append((r0 * .97 * math.cos(a), r0 * .97 * math.sin(a), z))
    ring = [(r0 * math.cos(math.radians(-90) + math.tau * j / sides), r0 * math.sin(math.radians(-90) + math.tau * j / sides), z0) for j in range(sides)]
    for j in range(sides):
        a, b = ring[j], ring[(j + 1) % sides]
        c, d = top[(j + 1) % sides], top[j]
        u0, u1 = j / sides * 2, (j + 1) / sides * 2
        sp.quad('pier', a, b, c, d, (u0, .74), (u1, .74), (u1, .9), (u0, .9))
    peak = (.06, -.04, z0 + .22)
    for j in range(sides):
        a, b = top[j], top[(j + 1) % sides]
        sp.tri('sstone', peak, a, b, (.5, .5), (a[0] + .5, a[1] + .5), (b[0] + .5, b[1] + .5))
    # rubble at the foot
    for c, r in [((.42, .3, .05), .09), ((.36, -.38, .04), .07), ((-.45, .2, .04), .06)]:
        V, F = rock(rng, c, r, (1, 1, .7), 0)
        V[:, 2] = np.maximum(V[:, 2], 0)
        add_mesh(sp, 'sstone', V, F, .4)
    return finish('SM_Prop_Pillar', sp.soup(), {'pier': TEX['vigil/pier'], 'sstone': TEX['vigil/sstone']}, repeat={'pier': (2, 1)})


# ---------------------------------------------------------------- wall
def wall():
    sp = Soup()
    T = 1.1  # brick texture repeat (metres); source wall used 4 units = 2.2 m, bricks read larger at prop scale
    L_, th = 1.08, .38
    heights = [1.42, 1.62, 1.3, .98, .66]
    n = len(heights)
    w = L_ / n
    sp.box('sstone', (0, 0, .09), (th + .1, L_, .18), tile=T, bottom=False)
    for i, h in enumerate(heights):
        y0 = -L_ / 2 + i * w
        c = (0, y0 + w / 2, .18 + h / 2)
        sx, sy, sz = th / 2, w / 2, h / 2
        cx, cy, cz = c
        # front (+X) and back (-X) faces, top, and only the end faces that are exposed
        sp.grid('wall', (cx + sx, cy - sy, cz - sz), (0, 2 * sy, 0), (0, 0, 2 * sz), 1, 1, (2 * sy / T, 2 * sz / T), uvo=((cy - sy) / T, (cz - sz) / T))
        sp.grid('wall', (cx - sx, cy + sy, cz - sz), (0, -2 * sy, 0), (0, 0, 2 * sz), 1, 1, (2 * sy / T, 2 * sz / T), uvo=((cy - sy) / T, (cz - sz) / T))
        sp.grid('wall', (cx - sx, cy - sy, cz + sz), (2 * sx, 0, 0), (0, 2 * sy, 0), 1, 1, (2 * sx / T, 2 * sy / T))
        left = heights[i - 1] if i > 0 else 0
        right = heights[i + 1] if i < n - 1 else 0
        if left < h:   # -Y end face, from the neighbour's top up
            zb = .18 + left
            sp.grid('wall', (cx - sx, cy - sy, zb), (2 * sx, 0, 0), (0, 0, cz + sz - zb), 1, 1, (2 * sx / T, (cz + sz - zb) / T), uvo=(0, zb / T))
        if right < h:
            zb = .18 + right
            sp.grid('wall', (cx + sx, cy + sy, zb), (-2 * sx, 0, 0), (0, 0, cz + sz - zb), 1, 1, (2 * sx / T, (cz + sz - zb) / T), uvo=(0, zb / T))
    # fallen blocks
    rng = np.random.default_rng(3)
    for c, r in [((.36, .3, .07), .1), ((.4, .05, .05), .07), ((-.36, -.32, .06), .08)]:
        V, F = rock(rng, c, r, (1.2, .9, .7), 0)
        V[:, 2] = np.maximum(V[:, 2], 0)
        add_mesh(sp, 'sstone', V, F, .4)
    return finish('SM_Prop_Wall', sp.soup(), {'wall': TEX['vigil/wall'], 'sstone': TEX['vigil/sstone']})


# ---------------------------------------------------------------- brazier
def brazier():
    sp = Soup()
    prof = [(r * S, z * S) for r, z in [(.65, 0), (.4, .22), (.17, .5), (.17, 1.4), (.62, 1.7), (.75, 1.9)]]
    sp.lathe('iron', (0, 0, 0), prof, 8, tile=.8, cap=(True, False))
    rim = [(.75 * S, 1.9 * S), (.66 * S, 1.9 * S), (.6 * S, 1.8 * S)]
    sp.lathe('iron', (0, 0, 0), rim, 8, tile=.8, cap=(False, False))
    # coal bed, slightly domed and lumpy
    rng = np.random.default_rng(2)
    jit = [[1.0] * 8, list(rng.uniform(.7, 1.0, 8)), [1.0] * 8]
    sp.lathe('ember', (0, 0, 0), [(.6 * S, 1.8 * S), (.4 * S, 1.86 * S), (.02 * S, 1.9 * S)], 8, patch=(0, 0, 1, 1), cap=(False, True), jitter=jit)
    # three feet
    for j in range(3):
        a = math.tau * j / 3
        c = (math.cos(a) * .3, math.sin(a) * .3, .03)
        sp.box('iron', c, (.09, .09, .06), tile=.8)
    return finish('SM_Prop_Brazier', sp.soup(), {'iron': TEX['vigil/iron'], 'ember': ember_tex()})


# ---------------------------------------------------------------- flame
def flame():
    sp = Soup()
    H = .3
    def tongue(c, h, r, phase, twist):
        prof = [(0.001, 0), (r * .7, h * .06), (r, h * .22), (r * .8, h * .45), (r * .45, h * .7), (r * .15, h * .9), (.001, h)]
        rings = []
        for i, (rr, z) in enumerate(prof):
            dx = math.sin(z / h * 2.4 + phase) * r * .25 * (z / h)
            rings.append((c[0] + dx, c[1], c[2] + z, rr, rr * .85))
        sp.loft('flame', rings, 6, (0, 0, 1, 1), phase=twist, cap=(True, True))
    tongue((0, 0, 0), H, .075, 0, 0)
    tongue((.035, .03, 0), H * .62, .045, 1.5, 30)
    tongue((-.03, -.035, 0), H * .55, .04, 2.6, 15)
    s = sp.soup()
    # gradient by height: v = 1 - z/H (top-down texture, row 0 = tip)
    s['UV'][..., 0] = .5
    s['UV'][..., 1] = np.clip(1 - s['P'][..., 2] / H, .01, .99)
    return finish('SM_Prop_Flame', s, {'flame': flame_tex()}, size=256)


# ---------------------------------------------------------------- banner
def banner():
    sp = Soup()
    Hp = 2.2
    sp.lathe('iron', (0, 0, 0), [(.13, 0), (.12, .05), (.05, .08), (.03, .12)], 8, tile=.8, cap=(True, False))
    sp.tube('iron', [(0, 0, .1), (0, 0, Hp - .08)], [.026, .026], 6, tile=.8)
    sp.lathe('gilt', (0, 0, Hp - .1), [(.04, 0), (.05, .03), (.02, .07), (.001, .1)], 6, tile=.3, cap=(True, False))
    # crossbar along Y at the top
    sp.tube('iron', [(0.035, -.36, Hp - .2), (0.035, .36, Hp - .2)], [.018, .018], 6, tile=.8, cap=(True, True))
    for y in (-.36, .36):
        sp.lathe('gilt', (0.035, y, Hp - .23), [(.03, 0), (.03, .06)], 6, tile=.3)
    # cloth: hangs from the bar, faces +X, gentle wave, swallowtail hem
    top, bottom, half = Hp - .22, .78, .31
    nu, nv = 4, 6
    front = Soup()
    def P(i, j):
        y = -half + 2 * half * i / nu
        t = j / nv
        z = top - (top - bottom) * t
        if j == nv:
            z += .22 * (1 - abs(i / nu * 2 - 1))     # V-cut hem
        x = .06 + .025 * math.sin(y * 9 + t * 3) * t
        return (x, y, z)
    def UV(i, j):
        p = P(i, j)
        return (i / nu, (top - p[2]) / (top - bottom))
    for i in range(nu):
        for j in range(nv):
            front.quad('banner', P(i, j), P(i + 1, j), P(i + 1, j + 1), P(i, j + 1), UV(i, j), UV(i + 1, j), UV(i + 1, j + 1), UV(i, j + 1))
    f = front.soup()
    # winding faces +X?
    n = np.cross(f['P'][:, 1] - f['P'][:, 0], f['P'][:, 2] - f['P'][:, 0])
    if n[:, 0].mean() < 0:
        f = L.flipped(f)
    s = L.cat(sp.soup(), f, L.flipped(f, .006))
    return finish('SM_Prop_Banner', s, {'iron': TEX['vigil/iron'], 'gilt': gilt_tex(), 'banner': banner_tex()})


# ---------------------------------------------------------------- cage
def cage():
    sp = Soup()
    k = .53
    R = .85 * k
    # bottom band (closed ring) and floor plate
    sp.lathe('iron', (0, 0, 0), [(.8 * k, 0), (R, 0), (R, .12 * k), (.8 * k, .12 * k), (.8 * k, 0)], 10, tile=.6, cap=(False, False))
    sp.lathe('iron', (0, 0, .02), [(.8 * k, 0), (.8 * k, .04)], 10, tile=.6, cap=(True, True))
    # dome (outside + inside)
    dome = Soup()
    dome.lathe('iron', (0, 0, 1.9 * k), [(R, 0), (R, .1 * k), (.3 * k, .65 * k), (.03 * k, .8 * k)], 10, tile=.6, cap=(False, True))
    d = dome.soup()
    for i in range(10):
        a = i * math.tau / 10
        x, y = math.cos(a) * .82 * k, math.sin(a) * .82 * k
        sp.tube('iron', [(x, y, .1 * k), (x, y, 2.0 * k)], [.04 * k, .04 * k], 4, tile=.6)
    # hook rod and ring
    zt = 2.7 * k
    sp.tube('iron', [(0, 0, zt - .02), (0, 0, zt + .2)], [.025, .025], 5, tile=.6)
    ring = [(0, .06 * math.sin(t), zt + .26 + .06 * math.cos(t)) for t in np.linspace(math.pi, -math.pi, 9)]
    sp.tube('iron', ring, [.015] * len(ring), 5, tile=.6)
    s = L.cat(sp.soup(), d, L.flipped(d, .004))
    top = s['P'][..., 2].max()
    s = L.xform(s, L.T(0, 0, -top))     # pivot at the top of the hook; cage hangs below
    return finish('SM_Prop_Cage', s, {'iron': TEX['vigil/iron']})


# ---------------------------------------------------------------- board tiles
def tile(name, top_tex, side_tex):
    sp = Soup()
    a, ch, h = .54, .025, .16
    b = a - ch
    # top (one cell of the world floor), chamfer, sides; bottom closed
    sp.quad('top', (-b, -b, h), (b, -b, h), (b, b, h), (-b, b, h), (0, 1), (1, 1), (1, 0), (0, 0))
    ring_t = [(-b, -b, h), (b, -b, h), (b, b, h), (-b, b, h)]
    ring_m = [(-a, -a, h - ch), (a, -a, h - ch), (a, a, h - ch), (-a, a, h - ch)]
    ring_b = [(-a, -a, 0), (a, -a, 0), (a, a, 0), (-a, a, 0)]
    for i in range(4):
        j = (i + 1) % 4
        sp.quad('side', ring_m[i], ring_m[j], ring_t[j], ring_t[i], (0, .9), (1, .9), (1, .95), (0, .95))
        sp.quad('side', ring_b[i], ring_b[j], ring_m[j], ring_m[i], (0, .6), (1, .6), (1, .9), (0, .9))
    sp.quad('side', (-a, a, 0), (a, a, 0), (a, -a, 0), (-a, -a, 0), (0, 0), (1, 0), (1, 1), (0, 1))
    s = sp.soup()
    # make sure every face points outward
    c = s['P'].mean(1) - np.array([0, 0, h / 2])
    n = np.cross(s['P'][:, 1] - s['P'][:, 0], s['P'][:, 2] - s['P'][:, 0])
    bad = (n * c).sum(1) < 0
    if bad.any():
        fixed = L.flipped(L.sub(s, bad))
        s = L.cat(L.sub(s, ~bad), fixed)
    return finish(name, s, {'top': top_tex, 'side': side_tex}, size=512)


if __name__ == '__main__':
    which = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else ['all']
    jobs = dict(stones=stones, pillar=pillar, wall=wall, brazier=brazier, flame=flame, banner=banner, cage=cage,
                tiles=lambda: (tile('SM_Tile_Vigil', TEX['vigil/floor'][128:256, 128:256], TEX['vigil/sstone']),
                               tile('SM_Tile_Root', TEX['root/floor'][256:384, 0:128], TEX['root/menhir'])))
    for k, f in jobs.items():
        if 'all' in which or k in which:
            f()
