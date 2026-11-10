"""Root of the World obstacle kit, scatter set and mist / light-shaft meshes, r002 (issue #149).

Run from the repository root (one Blender process, about a minute):
    blender -b --python Art/dark-fantasy-sources/r002-root/scripts/build_kit_root_r002.py [-- name ...]

Root-themed variants of the three obstacle recipes the Root encounters place (pillar -> root spire / hollow stump / mossy stump,
wall -> root curtain / fallen log with root ball / low root tangle, stones -> mossy boulders / root-wrapped standing stones / fungus
boulders), a 2-cell root arch for adjacent blocked pairs, eight scatter props, three mist cards and two light shafts.  Same
conventions and finishing as the #145 Vigil kit (its mist / shaft cards, ambient-occlusion bake and glb finishing are copied
below): metres, +X forward, Z up, pivot at the centre of the cell floor (the arch: the shared edge of two Y-adjacent cells), true
scale for a 150 cm cell, one small atlas per glb, COLOR_0 RGBA with RGB = AO x height gradient x moss tint and A = glow mask (fungus
caps) or, on cards, opacity.  Textures are procedural (root_lib.py); no outside content.
Outputs  Art/<asset-id>/r001/<SM_name>.glb  and  Art/dark-fantasy-sources/r002-root/kit_metrics.json
"""
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import r002_lib as R
from r002_lib import L, bpy, np, Vector, make_bvh, tri_normals, smoothstep, subdivide, bake_ao, tri_soup, vnoise
import root_lib as RL
from root_lib import add_rock, tube_path

from prims import Soup, rock

ROOT = R.ROOT
OUT_SRC = os.path.join(ROOT, 'Art', 'dark-fantasy-sources', 'r002-root')
PAD = 8
METRICS = {}
GROUND = lambda x, y: 0.0
GLOW = ('glowcap', 'glowmoss')


def rgba(rgb):
    t = np.ones((*rgb.shape[:2], 4), np.float32)
    t[..., :3] = rgb
    return t


def soft_noise(n, cells, seed):
    return R.tile_noise(n, cells, seed)


def kit_textures():
    t = {}
    t['bark'] = RL.bark_tile(192, 1.0, seed=83)             # 192 px per 1 m: 192 texels/m (two of them fit a 512 atlas with padding)
    t['stone'] = RL.stone_tile(192, seed=93)
    t['leaf'] = RL.leaf_tile(96, seed=103)
    t['heart'] = RL.heart_tile(64)
    t['cap'] = RL.cap_tile(64)
    t['fern'] = RL.fern_tile()
    t['stem'] = L.swatch((.80, .74, .60))
    t['glowcap'] = L.swatch((.34, .96, .86))
    t['glowmoss'] = L.swatch((.42, .92, .62))
    n = 64
    big, mid = soft_noise(n, 4, 1), soft_noise(n, 12, 2)
    t['moss'] = rgba(np.array((.14, .26, .09)) * (.65 + .55 * big[..., None]) * (.9 + .2 * mid[..., None]))
    t['litter'] = rgba(np.array((.30, .21, .10)) * (.7 + .6 * soft_noise(n, 6, 7)[..., None]) * (.85 + .3 * soft_noise(n, 17, 8)[..., None]))
    t['twig'] = rgba(np.array((.26, .17, .10)) * (.7 + .5 * soft_noise(n, 3, 4)[..., None]))
    t['husk'] = rgba(np.array((.42, .27, .12)) * (.7 + .5 * soft_noise(n, 5, 5)[..., None]) * (.9 + .15 * np.sin(np.arange(n) / n * 30)[None, :, None]))
    # mist: soft cloud density, pale green-white; the card's vertex alpha supplies the edge fade
    d = .5 * soft_noise(128, 3, 7) + .3 * soft_noise(128, 8, 8) + .2 * soft_noise(128, 20, 9)
    v = np.clip(.35 + 1.2 * (d - .3), 0, 1)
    m = np.empty((128, 128, 4), np.float32)
    m[..., :3] = (np.array((.74, .84, .78)) * (.55 + .45 * v[..., None]))
    m[..., 3] = 1
    t['mist'] = m
    g = np.linspace(1, .6, 64)[:, None, None]                  # shaft: bright at the top, warm gold
    t['shaft'] = rgba(np.broadcast_to(np.array((1.0, .90, .62)) * g, (64, 8, 3)).copy())
    return t


# ------------------------------------------------------------------ helpers
def helix_strand(sp, rng, a0, turns, h, rho0, r0, rho_end=.05, sides=5, n=12, mat='bark', tile=1.0, lean=(0, 0), c0=(0, 0), z0=0.0):
    pts, rad = [], []
    for i in range(n):
        t = i / (n - 1)
        rho = rho0 * (1 - t) ** .8 + rho_end * t
        a = a0 + turns * math.tau * t
        pts.append((c0[0] + rho * math.cos(a) + lean[0] * t, c0[1] + rho * math.sin(a) + lean[1] * t, z0 + h * t + (r0 * .5 if i == 0 else 0)))
        rad.append(max(r0 * (1 - .75 * t), .03))
    tube_path(sp, mat, pts, rad, sides, tile, cap=(False, True))
    return pts


def mushrooms(sp, rng, spots, glow=True, scale=1.0):
    for (x, y, z) in spots:
        RL.fungus_cluster(sp, rng, (x, y, z), n=int(rng.integers(2, 4)), scale=scale, glow=glow)


# ------------------------------------------------------------------ obstacle kit: pillars
def pillar_a():
    """Root spire: four root strands braided into a twisting column, 2.9 m, flared into buttress roots."""
    rng = np.random.default_rng(5)
    sp = Soup()
    for k in range(4):
        helix_strand(sp, rng, k * math.pi / 2 + rng.uniform(-.2, .2), 1.4, 2.9 - .15 * k, .40, .22, sides=6, n=14)
    sp.tube('bark', [(0, 0, 0), (0, 0, 1.4), (.06, -.04, 2.7)], [.22, .16, .06], 6, tile=1.0, patch=(0, 0, 1, 2), cap=(False, True))
    RL.buttress_roots(sp, rng, (0, 0), .36, .5, 6, GROUND, reach=2.1, sides=5, tile=1.0)
    mushrooms(sp, rng, [(.5, .3, .02), (-.45, -.4, .02)], glow=True, scale=.8)
    return sp.soup(), dict(moss=.4)


def pillar_b():
    """Hollow stump 3.2 m: a thick trunk snapped into a jagged crown, glowing fungus shelves, buttress roots."""
    rng = np.random.default_rng(6)
    sp = Soup()
    RL.trunk(sp, rng, (0, 0), 2.6, .40, .34, sides=10, tile=1.0, broken=True, flare=1.5, rings_per_m=2.0, jag=.35)
    RL.buttress_roots(sp, rng, (0, 0), .40, .8, 5, GROUND, reach=1.8, sides=5, tile=1.0)
    for z in (1.0, 1.6):                              # shelf fungus
        a = rng.uniform(0, math.tau)
        x, y = .40 * math.cos(a), .40 * math.sin(a)
        sp.lathe('glowcap', (x, y, z), [(.0, 0), (.30, .0), (.26, .05), (.05, .08)], 7, cap=(False, True))
    mushrooms(sp, rng, [(.6, -.3, .02), (-.55, .45, .02)], glow=False, scale=.9)
    return sp.soup(), dict(moss=.45)


def pillar_c():
    """Mossy stump 1.7 m: sawn-off, with two branch stubs and a ring of toadstools."""
    rng = np.random.default_rng(7)
    sp = Soup()
    RL.trunk(sp, rng, (0, 0), 1.35, .46, .40, sides=9, tile=1.0, broken=False, flat_top=True, flare=1.35, rings_per_m=3.0)
    for (a, z, ln) in ((.8, .8, .5), (3.9, 1.05, .42)):
        c, s_ = math.cos(a), math.sin(a)
        tube_path(sp, 'bark', [(.3 * c, .3 * s_, z), (.55 * c, .55 * s_, z + .12), ((.3 + ln) * c, (.3 + ln) * s_, z + .22)], [.11, .09, .05], 5, 1.0, cap=(False, True))
    RL.buttress_roots(sp, rng, (0, 0), .46, .4, 5, GROUND, reach=1.8, sides=5, tile=1.0)
    mushrooms(sp, rng, [(.55, .1, .02), (-.3, .5, .02), (.15, -.58, .02), (-.5, -.3, .02)], glow=False, scale=.8)
    return sp.soup(), dict(moss=.6)


# ------------------------------------------------------------------ obstacle kit: walls
def wall_a():
    """Root curtain: five root strands rising and arching together, two woven cross bands, 2.2 m."""
    rng = np.random.default_rng(3)
    sp = Soup()
    ys = np.linspace(-.6, .6, 5)
    for i, y in enumerate(ys):
        h = [1.5, 2.1, 1.8, 2.2, 1.4][i]
        pts, rad = [], []
        n = 10
        for j in range(n):
            t = j / (n - 1)
            pts.append((.12 * math.sin(t * 5 + i), y + .06 * math.sin(t * 7 + i * 2) + .12 * t * (-1 if i % 2 else 1), .04 + (h - .04) * t))
            rad.append(max(.17 * (1 - .65 * t) * (1 + .15 * math.sin(i + t * 9)), .035))
        tube_path(sp, 'bark', pts, rad, 6, 1.0, cap=(False, True))
    for z, sx in ((.7, 1), (1.3, -1)):
        pts = [(.1 * sx * math.sin(k * 1.3), -.72 + k * .24, z + .08 * math.sin(k * 2.0)) for k in range(7)]
        tube_path(sp, 'bark', pts, [.07] * 7, 5, 1.0, cap=(True, True))
    RL.buttress_roots(sp, rng, (0, 0), .55, .35, 4, GROUND, reach=1.4, sides=5, tile=1.0)
    for c, r in [((.5, .5, .1), .13), ((.55, -.3, .08), .1), ((-.5, -.55, .08), .11)]:
        add_rock(sp, 'stone', rng, c, r, (1.2, 1.0, .65), .4, detail=0)
    return sp.soup(), dict(moss=.45)


def wall_b():
    """Fallen log across the cell with an upturned root ball, 2.4 m."""
    rng = np.random.default_rng(8)
    sp = Soup()
    sides, n = 9, 7
    rings, jit = [], []
    for i in range(n + 1):
        t = i / n
        rr = .40 - .06 * t
        rings.append((0.0, 0.0, -.55 + 1.1 * t, rr, rr))
        jit.append([1 + .07 * math.sin(k * 2.3 + i) + rng.uniform(-.03, .03) for k in range(sides)])
    sp.loft('bark', rings, sides, (0, 0, 3, 1.1), 0.0, (False, True), jit)
    # lie the log along Y and raise it
    lg = L.xform(sp.soup(), L.T(0, 0, .40) @ RL.Rx(90) @ L.T(0, 0, 0))
    lg = L.xform(lg, L.T(0, .15, 0))
    rb = Soup()
    add_rock(rb, 'bark', rng, (0, -.7, .85), .62, (.9, .22, 1.3), .8, detail=1, sink=False)       # the upturned root plate (soil and tangled roots)
    for k in range(11):                                # roots fanning out of the plate in the XZ plane
        a = math.pi * (k + .5) / 11 + rng.uniform(-.08, .08)
        L_ = rng.uniform(.28, .5)
        pts = [(.45 * math.cos(a) * .6, -.7 - .12, .85 + .6 * math.sin(a) * .6)]
        for j in range(1, 5):
            t = j / 4
            pts.append((math.cos(a) * (.27 + L_ * t) + rng.normal(0, .02), -.82 - .08 * t, .85 + math.sin(a) * (.36 + L_ * t * 1.05)))
        tube_path(rb, 'bark', pts, [.07, .06, .045, .03, .015], 5, 1.0, cap=(False, True))
    s = L.cat(lg, rb.soup())
    mush = Soup()
    mushrooms(mush, rng, [(.2, .4, .75), (.25, .6, .72)], glow=True, scale=.8)
    return L.cat(s, mush.soup()), dict(moss=.5)


def wall_c():
    """Low root tangle with rubble, 1.0 m."""
    rng = np.random.default_rng(9)
    sp = Soup()
    for i in range(7):
        y0 = rng.uniform(-.55, .15)
        pts, rad = [], []
        for j in range(8):
            t = j / 7
            pts.append((rng.normal(0, .08) + .25 * math.sin(t * 6 + i), y0 + t * rng.uniform(.35, .7), .18 + .22 * abs(math.sin(t * 3.3 + i)) + .1 * (i % 3)))
            rad.append(.12 * (1 - .5 * t))
        tube_path(sp, 'bark', pts, rad, 5, 1.0, cap=(True, True))
    for c, r in [((.4, .35, .12), .22), ((.5, -.1, .08), .16), ((-.4, .5, .1), .18), ((.0, -.55, .08), .15)]:
        add_rock(sp, 'stone', rng, c, r, (1.2, 1.0, .65), .5, detail=0)
    mush = Soup()
    mushrooms(mush, rng, [(-.45, -.1, .02), (.2, .6, .02)], glow=False, scale=.8)
    return L.cat(sp.soup(), mush.soup()), dict(moss=.6)


# ------------------------------------------------------------------ obstacle kit: stones
def stones_a():
    """Mossy boulder cluster, 1.3 m."""
    rng = np.random.default_rng(11)
    sp = Soup()
    for c, r, sq in [((0, 0, .38), .58, (1.1, .95, .8)), ((.4, .34, .2), .34, (1, 1, .7)), ((-.4, .3, .2), .3, (1, .9, .7)), ((.1, -.5, .17), .28, (1.1, .9, .65)), ((-.5, -.4, .1), .2, (1, 1, .6))]:
        add_rock(sp, 'stone', rng, c, r, sq, .6, detail=1)
    return sp.soup(), dict(moss=.8)


def stones_b():
    """Three leaning standing stones wrapped in climbing roots, up to 2.1 m."""
    rng = np.random.default_rng(12)
    sp = Soup()
    for c, r, sq, lean in [((-.1, .1, 1.0), .62, (.38, .3, 1.6), .1), ((.3, -.35, .8), .5, (.42, .3, 1.5), -.16), ((-.25, -.5, .6), .38, (.5, .35, 1.4), .22)]:
        add_rock(sp, 'stone', rng, c, r, sq, .7, detail=1, lean=lean)
        for k in range(2):
            a = rng.uniform(0, math.tau)
            helix_strand(sp, rng, a, rng.uniform(.8, 1.3), c[2] * 1.3, r * .62, .05, rho_end=r * .45, sides=5, n=8, lean=(lean * c[2] * 1.2, 0), c0=(c[0], c[1]))
    mush = Soup()
    mushrooms(mush, rng, [(.5, .45, .02)], glow=True, scale=.8)
    return L.cat(sp.soup(), mush.soup()), dict(moss=.45)


def stones_c():
    """Low boulders carrying glowing fungus, 1.0 m."""
    rng = np.random.default_rng(13)
    sp = Soup()
    for c, r, sq in [((0, 0, .26), .5, (1.2, .95, .65)), ((.45, .35, .14), .28, (1, 1, .6)), ((-.42, .32, .14), .26, (1, .9, .6)), ((.1, -.5, .12), .24, (1.1, .9, .55))]:
        add_rock(sp, 'stone', rng, c, r, sq, .6, detail=1)
    mush = Soup()
    mushrooms(mush, rng, [(.1, .1, .5), (-.25, .05, .42), (.3, -.1, .4), (.5, .3, .3), (.0, -.4, .25)], glow=True, scale=1.0)
    return L.cat(sp.soup(), mush.soup()), dict(moss=.7)


# ------------------------------------------------------------------ 2-cell root arch (spans two adjacent cells along Y)
def arch_2cell():
    """3.0 m long x 0.9 m deep gate arch of living roots: two root piers joined by a pointed arch, hanging tendrils and fungus.
    Pivot at the centre of the pair's floor (the shared cell edge); long axis along Y, rotate 90 degrees for X-adjacent pairs."""
    rng = np.random.default_rng(21)
    sp = Soup()
    span, apex = 1.02, 2.9
    for side in (-1, 1):                               # piers: bundles of three strands
        for k in range(3):
            ox, oy = rng.normal(0, .12), side * (span + rng.normal(0, .08))
            pts, rad = [], []
            n = 9
            for j in range(n):
                t = j / (n - 1)
                pts.append((ox * (1 - t) + .08 * math.sin(t * 6 + k), oy * (1 - .35 * t * t), .05 + (1.7 + .15 * k) * t))
                rad.append(.22 * (1 - .3 * t))
            tube_path(sp, 'bark', pts, rad, 7, 1.0, cap=(False, False))
    # the arch: two strands following a pointed arch with a slight twist, thick at the springs
    for k, off in enumerate((-.16, .0, .16)):
        pts, rad = [], []
        n = 15
        for j in range(n):
            t = j / (n - 1)
            y = -span * (1 - 2 * t) * 1.02
            z = 1.55 + (apex - 1.55) * (1 - abs(1 - 2 * t) ** 1.6)
            pts.append((off + .08 * math.sin(t * 9 + k), y, z))
            rad.append(.2 * (.7 + .3 * abs(1 - 2 * t)) * (1 - .12 * k))
        tube_path(sp, 'bark', pts, rad, 7, 1.0, cap=(False, False))
    for side in (-1, 1):
        RL.buttress_roots(sp, rng, (0.0, side * span), .32, .5, 5, GROUND, reach=1.3, sides=5, tile=1.0)
    for k in range(5):                                 # hanging tendrils
        y = rng.uniform(-.7, .7)
        zt = 1.55 + (apex - 1.55) * (1 - abs(y / span) ** 1.6) - .15
        tube_path(sp, 'bark', [(rng.normal(0, .08), y, zt), (rng.normal(0, .1), y + rng.normal(0, .05), zt - .35), (rng.normal(0, .1), y + rng.normal(0, .08), zt - .6)], [.05, .035, .012], 5, 1.0, cap=(False, True))
    mush = Soup()
    mushrooms(mush, rng, [(.5, 1.0, .02), (.45, -1.05, .02), (-.5, .85, .02)], glow=True, scale=.8)
    sp2 = L.cat(sp.soup(), mush.soup())
    return sp2, dict(moss=.45, edge=.9)


# ------------------------------------------------------------------ scatter
def sc_mushrooms():
    rng = np.random.default_rng(31)
    sp = Soup()
    mushrooms(sp, rng, [(0, 0, 0), (.35, .2, 0), (-.3, .25, 0), (.1, -.35, 0), (-.35, -.3, 0)], glow=True, scale=1.0)
    for c, r in [((0, 0, 0), .3), ((.3, .2, 0), .2)]:
        sp.lathe('moss', c, [(r, .0), (r * .8, .03), (r * .2, .05)], 8, cap=(False, True))
    return sp.soup(), dict(moss=.0)


def sc_leaves():
    rng = np.random.default_rng(32)
    sp = Soup()
    for i in range(34):
        x, y = rng.normal(0, .2), rng.normal(0, .2)
        a = rng.uniform(0, math.tau)
        w, l = rng.uniform(.06, .11), rng.uniform(.10, .18)
        c, s_ = math.cos(a), math.sin(a)
        z = .006 + .002 * i % 7 * .01 + rng.uniform(0, .03)
        p = [(x + (-w * c + l * s_ * 0), y + (-w * s_), z), (x + w * c, y + w * s_, z), (x + (w * c + l * (-s_)), y + (w * s_ + l * c), z + .01), (x + (-w * c - l * s_), y + (-w * s_ + l * c), z + .01)]
        sp.quad('litter', p[0], p[1], p[2], p[3], (rng.uniform(0, .5), rng.uniform(0, .5)), (rng.uniform(.5, 1), rng.uniform(0, .5)), (rng.uniform(.5, 1), rng.uniform(.5, 1)), (rng.uniform(0, .5), rng.uniform(.5, 1)))
    s = sp.soup()
    f = tri_normals(s['P'])[:, 2] < 0
    s['P'][f] = s['P'][f][:, [0, 2, 1]]; s['UV'][f] = s['UV'][f][:, [0, 2, 1]]
    return s, dict(moss=.0)


def sc_twigs():
    rng = np.random.default_rng(33)
    sp = Soup()
    for i in range(6):
        a = rng.uniform(0, math.tau)
        x, y = rng.normal(0, .15), rng.normal(0, .15)
        L_ = rng.uniform(.5, .9)
        pts = [(x + math.cos(a) * L_ * t, y + math.sin(a) * L_ * t, .025 + .02 * math.sin(t * 4)) for t in np.linspace(-.5, .5, 5)]
        tube_path(sp, 'twig', pts, [.018, .018, .014, .01, .006], 4, .3, cap=(True, True))
        mid = pts[2]
        b = a + rng.choice((-1, 1)) * .7
        tube_path(sp, 'twig', [mid, (mid[0] + math.cos(b) * .12, mid[1] + math.sin(b) * .12, .045), (mid[0] + math.cos(b) * .25, mid[1] + math.sin(b) * .25, .035)], [.011, .008, .004], 4, .3, cap=(True, True))
    return sp.soup(), dict(moss=.1)


def sc_tendrils():
    rng = np.random.default_rng(34)
    sp = Soup()
    for ang in (0.4, 2.2, 3.7, 5.2):
        RL.root_system(sp, rng, (0, 0), ang, .62, .06, GROUND, depth=1, sides=5, tile=.4, wander=.4, hug=.55, n=7, mat='bark')
    return sp.soup(), dict(moss=.1)


def sc_ferns():
    rng = np.random.default_rng(35)
    parts = [RL.fern_clump(rng, c, h=rng.uniform(.5, .8), w=rng.uniform(.6, .9), blades=6, ground_z=0.0) for c in ((0, 0), (.45, .3), (-.4, .35))]
    s = L.cat(*parts)
    return s, dict(moss=.0)


def sc_pebbles():
    rng = np.random.default_rng(36)
    sp = Soup()
    for c, r, sq in [((0, 0, .04), .13, (1.2, 1, .5)), ((.3, .15, .03), .09, (1, 1, .55)), ((-.25, .22, .03), .1, (1.1, .9, .5)), ((.12, -.3, .025), .08, (1, 1.1, .5)), ((-.3, -.18, .02), .07, (1, 1, .5)), ((.4, -.2, .02), .06, (1, 1, .5))]:
        add_rock(sp, 'stone', rng, c, r, sq, .3, detail=0)
    sp.lathe('moss', (.0, .0, 0), [(.34, .0), (.28, .02), (.1, .035)], 8, cap=(False, True))
    return sp.soup(), dict(moss=.3)


def sc_glowmoss():
    sp = Soup()
    rng = np.random.default_rng(37)
    seg = 18
    rad = .5 + .1 * np.sin(np.arange(seg) * 2.3) + rng.uniform(-.05, .05, seg)
    for i in range(seg):
        a0, a1 = math.tau * i / seg, math.tau * (i + 1) / seg
        p0 = (rad[i] * math.cos(a0), rad[i] * math.sin(a0), .012)
        p1 = (rad[(i + 1) % seg] * math.cos(a1), rad[(i + 1) % seg] * math.sin(a1), .012)
        sp.tri('moss', (0, 0, .035), p0, p1, (.5, .5), (.5 + p0[0], .5 + p0[1]), (.5 + p1[0], .5 + p1[1]))
    for (x, y) in [(.0, .0), (.2, .12), (-.18, .1), (.08, -.2)]:             # glowing buds in the patch
        sp.lathe('glowmoss', (x, y, .03), [(.05, 0), (.045, .05), (.015, .09)], 5, cap=(False, True))
    s = sp.soup()
    n = tri_normals(s['P'])[:, 2]
    f = (n < 0) & (s['mat'] == 'moss')
    s['P'][f] = s['P'][f][:, [0, 2, 1]]; s['UV'][f] = s['UV'][f][:, [0, 2, 1]]
    return s, dict(moss=.0, glow=('glowmoss',))


def sc_husks():
    rng = np.random.default_rng(38)
    sp = Soup()
    for (x, y, a, sc) in [(0, 0, .4, 1.0), (.35, .25, 2.0, .8), (-.3, .3, 4.0, .9), (.1, -.35, 5.0, .7), (-.4, -.25, 1.2, .6)]:
        pts = [(x, y, .05 * sc), (x + .06 * sc * math.cos(a), y + .06 * sc * math.sin(a), .1 * sc), (x + .1 * sc * math.cos(a), y + .1 * sc * math.sin(a), .07 * sc)]
        sp.lathe('husk', (x, y, 0), [(.01, 0), (.07 * sc, .05 * sc), (.085 * sc, .1 * sc), (.05 * sc, .17 * sc), (.012, .2 * sc)], 6, cap=(False, False))
    return sp.soup(), dict(moss=.0)


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
    s['P'][..., 2] = np.maximum(s['P'][..., 2], -.015) if asset.startswith(('prop-', 'scatter-')) else s['P'][..., 2]       # nothing sinks more than 1.5 cm below the floor pivot
    if bake:
        s = bake_piece(s, opts.get('moss', 0.0), edge=opts.get('edge', .45))
    solid = [k for k in set(s['mat'].tolist()) if texs[k].shape[0] <= 8 and texs[k].shape[1] <= 8]
    s2, A, place = L.build_atlas(s, texs, size=size, pad=PAD, solid=solid, repeat=repeat or {}, downscale={'stone': 2, 'bark': 2} if size <= 256 else {})
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
    'pillar_a': ('prop-root-pillar', 1, '/Game/DnDDiceRoll/Theme/Props/SM_Prop_Root_Pillar', 'SM_Prop_Root_Pillar', pillar_a, 512, {}),
    'pillar_b': ('prop-root-pillar-b', 1, '/Game/DnDDiceRoll/Theme/Props/SM_Prop_Root_Pillar_B', 'SM_Prop_Root_Pillar_B', pillar_b, 512, {}),
    'pillar_c': ('prop-root-pillar-c', 1, '/Game/DnDDiceRoll/Theme/Props/SM_Prop_Root_Pillar_C', 'SM_Prop_Root_Pillar_C', pillar_c, 512, {}),
    'wall_a': ('prop-root-wall', 1, '/Game/DnDDiceRoll/Theme/Props/SM_Prop_Root_Wall', 'SM_Prop_Root_Wall', wall_a, 512, {}),
    'wall_b': ('prop-root-wall-b', 1, '/Game/DnDDiceRoll/Theme/Props/SM_Prop_Root_Wall_B', 'SM_Prop_Root_Wall_B', wall_b, 512, {}),
    'wall_c': ('prop-root-wall-c', 1, '/Game/DnDDiceRoll/Theme/Props/SM_Prop_Root_Wall_C', 'SM_Prop_Root_Wall_C', wall_c, 512, {}),
    'stones_a': ('prop-root-stones', 1, '/Game/DnDDiceRoll/Theme/Props/SM_Prop_Root_Stones', 'SM_Prop_Root_Stones', stones_a, 512, {}),
    'stones_b': ('prop-root-stones-b', 1, '/Game/DnDDiceRoll/Theme/Props/SM_Prop_Root_Stones_B', 'SM_Prop_Root_Stones_B', stones_b, 512, {}),
    'stones_c': ('prop-root-stones-c', 1, '/Game/DnDDiceRoll/Theme/Props/SM_Prop_Root_Stones_C', 'SM_Prop_Root_Stones_C', stones_c, 512, {}),
    'arch2': ('prop-root-arch-2cell', 1, '/Game/DnDDiceRoll/Theme/Props/SM_Prop_Root_Arch2Cell', 'SM_Prop_Root_Arch2Cell', arch_2cell, 512, {}),
    'sc_mushrooms': ('scatter-root-mushrooms', 1, '/Game/DnDDiceRoll/Theme/Scatter/SM_Scatter_Root_Mushrooms', 'SM_Scatter_Root_Mushrooms', sc_mushrooms, 256, {}),
    'sc_leaves': ('scatter-root-leaves', 1, '/Game/DnDDiceRoll/Theme/Scatter/SM_Scatter_Root_Leaves', 'SM_Scatter_Root_Leaves', sc_leaves, 256, {}),
    'sc_twigs': ('scatter-root-twigs', 1, '/Game/DnDDiceRoll/Theme/Scatter/SM_Scatter_Root_Twigs', 'SM_Scatter_Root_Twigs', sc_twigs, 256, {}),
    'sc_tendrils': ('scatter-root-tendrils', 1, '/Game/DnDDiceRoll/Theme/Scatter/SM_Scatter_Root_Tendrils', 'SM_Scatter_Root_Tendrils', sc_tendrils, 256, {}),
    'sc_ferns': ('scatter-root-ferns', 1, '/Game/DnDDiceRoll/Theme/Scatter/SM_Scatter_Root_Ferns', 'SM_Scatter_Root_Ferns', sc_ferns, 256, {}),
    'sc_pebbles': ('scatter-root-pebbles', 1, '/Game/DnDDiceRoll/Theme/Scatter/SM_Scatter_Root_Pebbles', 'SM_Scatter_Root_Pebbles', sc_pebbles, 256, {}),
    'sc_glowmoss': ('scatter-root-glowmoss', 1, '/Game/DnDDiceRoll/Theme/Scatter/SM_Scatter_Root_GlowMoss', 'SM_Scatter_Root_GlowMoss', sc_glowmoss, 256, {}),
    'sc_husks': ('scatter-root-husks', 1, '/Game/DnDDiceRoll/Theme/Scatter/SM_Scatter_Root_Husks', 'SM_Scatter_Root_Husks', sc_husks, 256, {}),
}
FX = {
    'mist_a': ('fx-root-mist-card', 1, '/Game/DnDDiceRoll/Theme/Fx/SM_Fx_Root_MistCard', 'SM_Fx_Root_MistCard', lambda: fx_cards('mist_a'), 256),
    'mist_b': ('fx-root-mist-card-b', 1, '/Game/DnDDiceRoll/Theme/Fx/SM_Fx_Root_MistCard_B', 'SM_Fx_Root_MistCard_B', lambda: fx_cards('mist_b'), 256),
    'mist_c': ('fx-root-mist-card-c', 1, '/Game/DnDDiceRoll/Theme/Fx/SM_Fx_Root_MistCard_C', 'SM_Fx_Root_MistCard_C', lambda: fx_cards('mist_c'), 256),
    'shaft_a': ('fx-root-light-shaft', 1, '/Game/DnDDiceRoll/Theme/Fx/SM_Fx_Root_LightShaft', 'SM_Fx_Root_LightShaft', lambda: fx_shaft('prism'), 128),
    'shaft_b': ('fx-root-light-shaft-b', 1, '/Game/DnDDiceRoll/Theme/Fx/SM_Fx_Root_LightShaft_B', 'SM_Fx_Root_LightShaft_B', lambda: fx_shaft('cross'), 128),
}


def main():
    only = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    texs = kit_textures()
    for key, (asset, rev, unreal, name, fn, size, rep) in ASSETS.items():
        if only and key not in only:
            continue
        s, opts = fn()
        finish(asset, unreal, rev, name, s, texs, size, opts, repeat=rep, glow_mats=GLOW)
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
