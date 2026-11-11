"""The Vault obstacle kit, scatter set and mist / light-shaft meshes, r002 (issue #149).

Run from the repository root (one Blender process, under a minute):
    blender -b --python Art/dark-fantasy-sources/r002-vault/scripts/build_kit_vault_r002.py [-- name ...]

Vault-themed variants of the three obstacle recipes the finale can place (pillar -> broken fluted column / ossuary pier / rune stele,
wall -> ashlar run / stacked sarcophagi / iron grille on a low wall, stones -> rubble heap / bone mound / toppled statue), a 2-cell crypt
arch for adjacent blocked pillar and wall pairs, eight scatter props, three mist cards and two light shafts.  Same conventions and
finishing as the Root and Vigil kits (their ambient-occlusion bake and glb finishing are copied below): metres, +X forward, Z up, pivot at
the centre of the cell floor (the arch: the shared edge of two Y-adjacent cells), true scale for a 150 cm cell, one small atlas per glb,
COLOR_0 RGBA with RGB = AO x height gradient and A = glow mask (the cold-blue rune and crystal materials) or, on cards, opacity.
Textures are procedural (vault_lib.py); no outside content.
Outputs  Art/<asset-id>/r001/<SM_name>.glb  and  Art/dark-fantasy-sources/r002-vault/kit_metrics.json
"""
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import r002_lib as R
from r002_lib import L, bpy, np, Vector, make_bvh, tri_normals, smoothstep, subdivide, bake_ao, tri_soup, vnoise
import vault_lib as VL
from vault_lib import add_rock, tube_path, T, Rz, Rx, Ry

from prims import Soup, rock

ROOT = R.ROOT
OUT_SRC = os.path.join(ROOT, 'Art', 'dark-fantasy-sources', 'r002-vault')
PAD = 8
METRICS = {}
GLOW = ('rune', 'crystal')


def rgba(rgb):
    t = np.ones((*rgb.shape[:2], 4), np.float32)
    t[..., :3] = rgb
    return t


def soft_noise(n, cells, seed):
    return R.tile_noise(n, cells, seed)


def kit_textures():
    t = {}
    t['ashlar'] = VL.ashlar_tile(256, 2.0, seed=183)        # 128 texels/m on a 512 atlas with its padding
    t['stone'] = VL.stone_tile(192, seed=193)
    t['basalt'] = VL.basalt_tile(128, seed=203)
    t['bone'] = VL.bone_tile(64)
    t['iron'] = VL.iron_tile(64)
    t['wood'] = VL.wood_tile(64)
    t['cloth'] = VL.cloth_tile(64)
    t['rune'] = L.swatch((.50, .85, 1.0))
    t['crystal'] = L.swatch((.62, .92, 1.0))
    # mist: soft cloud density, cold grey-blue; the card's vertex alpha supplies the edge fade
    d = .5 * soft_noise(128, 3, 7) + .3 * soft_noise(128, 8, 8) + .2 * soft_noise(128, 20, 9)
    v = np.clip(.35 + 1.2 * (d - .3), 0, 1)
    m = np.empty((128, 128, 4), np.float32)
    m[..., :3] = (np.array((.66, .78, .92)) * (.55 + .45 * v[..., None]))
    m[..., 3] = 1
    t['mist'] = m
    g = np.linspace(1, .6, 64)[:, None, None]                  # shaft: bright at the top, cold moonlight
    t['shaft'] = rgba(np.broadcast_to(np.array((.70, .88, 1.0)) * g, (64, 8, 3)).copy())
    return t



# ------------------------------------------------------------------ helpers
def teeth(sp, rng, r, z, n=6, hmin=.12, hmax=.4, mat='ashlar', w=.2):
    """A jagged broken crown: n small gabled teeth around a circle of radius r at height z."""
    for k in range(n):
        a = math.tau * (k + rng.uniform(-.2, .2)) / n
        h = rng.uniform(hmin, hmax)
        t = Soup()
        VL.convex_prism(t, mat, [(-w, 0), (w, 0), (w * .3, h), (-w * .5, h * .85)], -w * .55, w * .55, tile=1.0, mat_cap='stone')
        VL.merge(sp, L.xform(t.soup(), T(r * math.cos(a), r * math.sin(a), z) @ Rz(math.degrees(a) + 90)))


def ring_band(sp, r, z, h=.05, sides=10, mat='rune'):
    sp.lathe(mat, (0, 0, z), [(r, 0), (r, h)], sides, cap=(False, False), tile=1.0)


def skull_row(sp, rng, y, z, xs):
    for x in xs:
        VL.merge(sp, VL.skull(rng, (x, y, z), scale=rng.uniform(.95, 1.15), yaw=rng.uniform(-25, 25) + 0.0))


def chain_hang(sp, x, y, z0, n=7):
    VL.chain_run(sp, [(x, y, z0 - .22 * i) for i in range(n)], link=.06)


# ------------------------------------------------------------------ obstacle kit: pillars
def pillar_a():
    """Broken fluted column 2.7 m: plinth, a tapering shaft with a rune ring, a jagged crown, a fallen drum at the foot."""
    rng = np.random.default_rng(5)
    sp = Soup()
    sp.box('stone', (0, 0, .16), (1.3, 1.3, .32), tile=1.0, top='stone', bottom=False)
    sp.lathe('ashlar', (0, 0, .32), [(.54, 0), (.5, .45), (.46, 1.5), (.43, 2.2)], 10, tile=1.0, cap=(False, False))
    teeth(sp, rng, .31, 2.5, n=6, hmin=.12, hmax=.42)
    sp.lathe('ashlar', (0, 0, 2.52), [(.43, 0), (.4, .06), (.2, .1)], 10, tile=1.0, cap=(False, True))
    ring_band(sp, .45, 1.42, .05)
    VL.merge(sp, VL.column_drum(rng, (.5, -.42), r=.3, h=.45, yaw=30, lean=90))
    return sp.soup(), dict(edge=.45)


def pillar_b():
    """Ossuary pier 3.2 m: a square pier whose middle courses are stacked with skulls, a corbelled capital, a hanging chain."""
    rng = np.random.default_rng(6)
    sp = Soup()
    sp.box('stone', (0, 0, .15), (1.25, 1.25, .3), tile=1.0, top='stone', bottom=False)
    VL.ashlar_block(sp, -.48, .48, -.48, .48, .3, 1.4)
    VL.ashlar_block(sp, -.48, .48, -.48, .48, 2.2, 2.85, top='stone')
    for y in (-.5, .5):                                         # skull bands on the +-Y faces
        for row in range(3):
            for k in range(4):
                VL.merge(sp, VL.skull(rng, (-.33 + k * .22 + (.11 if row % 2 else 0), y + (-.0 if y < 0 else .0), 1.42 + row * .26), scale=.95, yaw=-90 if y < 0 else 90))
    sp.box('ashlar', (0, 0, 1.4 + .39), (.9, .9, .78), tile=1.0, top='stone', bottom=False)           # the core behind the skulls
    sp.box('stone', (0, 0, 2.95), (1.15, 1.15, .22), tile=1.0, top='stone', bottom=False)               # capital
    sp.box('stone', (0, 0, 3.12), (.8, .8, .14), tile=1.0, top='stone', bottom=False)
    chain_hang(sp, .5, .0, 2.9, n=6)
    return sp.soup(), dict(edge=.45)


def pillar_c():
    """Rune stele 1.9 m: a tapered basalt slab on a stepped base with a cold-blue glyph and a few skulls at its foot."""
    rng = np.random.default_rng(7)
    sp = Soup()
    sp.box('stone', (0, 0, .1), (1.2, 1.0, .2), tile=1.0, top='stone', bottom=False)
    sp.box('stone', (0, 0, .28), (1.0, .8, .16), tile=1.0, top='stone', bottom=False)
    rings = [(0, 0, .36, .62, .42), (0, 0, 1.4, .54, .34), (0, 0, 1.78, .42, .26), (0, 0, 1.95, .05, .05)]
    sp.loft('basalt', rings, 4, patch=(0, 0, 2, 3), phase=45, cap=(True, True))
    for (cy, z0, z1, w) in ((0, .75, 1.55, .09), (0, 1.0, 1.15, .32)):
        sp.box('rune', (-.28 - .0, cy, (z0 + z1) / 2), (.03, w, z1 - z0), tile=1.0, bottom=False)
    skull_row(sp, rng, -.5, .0, [-.18, .14])
    return sp.soup(), dict(edge=.45)


# ------------------------------------------------------------------ obstacle kit: walls (span the cell along Y, so neighbours join)
def wall_a():
    """Ashlar run 2.3 m: a broken wall 0.9 m thick across the cell with a ragged top, a cornice and a rune keystone."""
    rng = np.random.default_rng(3)
    sp = Soup()
    for i, h in enumerate((2.3, 1.9, 2.1, 1.5)):
        y0 = -.75 + i * .375
        VL.ashlar_block(sp, -.45, .45, y0, y0 + .375, 0.0, h)
    sp.box('stone', (0, -.4, 2.4), (1.05, .8, .16), tile=1.0, top='stone', bottom=False)
    sp.box('rune', (-.455, -.4, 2.0), (.025, .35, .06), tile=1.0, bottom=False)
    for c, r in [((.5, .6, .1), .2), ((-.5, .45, .08), .18)]:
        add_rock(sp, 'stone', rng, c, r, (1.2, 1.0, .65), .5, detail=0)
    return sp.soup(), dict(edge=.45)


def wall_b():
    """Stacked sarcophagi 1.7 m: two stone coffins end to end across the cell, a third lying on top, one lid ajar."""
    rng = np.random.default_rng(8)
    sp = Soup()
    for (z, dy, yaw) in ((0, 0, 0), (.82, .0, 0)):
        s_ = VL.sarcophagus(rng, w=.8, l=1.45, h=.78, effigy=False)
        VL.merge(sp, L.xform(s_, T(0, dy, z) @ Rz(90)))
    top = VL.sarcophagus(rng, w=.7, l=1.3, h=.5, effigy=True)
    VL.merge(sp, L.xform(top, T(.05, .1, 1.62) @ Rz(76)))
    return sp.soup(), dict(edge=.45)


def wall_c():
    """Iron grille 1.4 m on a low ashlar wall: bars, a crossbar, chains swagged along it and a lantern shard."""
    rng = np.random.default_rng(9)
    sp = Soup()
    VL.ashlar_block(sp, -.35, .35, -.75, .75, 0.0, .6, top='stone')
    for k in range(7):
        y = -.66 + k * .22
        sp.box('iron', (0, y, .6 + .45), (.06, .06, .9), tile=.3, bottom=True)
    sp.box('iron', (0, 0, 1.52), (.08, 1.5, .08), tile=.3, bottom=True)
    sp.box('iron', (0, 0, .78), (.07, 1.4, .06), tile=.3, bottom=True)
    VL.chain_run(sp, [(.1, -.72, 1.45), (.1, -.35, 1.2), (.1, 0, 1.12), (.1, .35, 1.2), (.1, .72, 1.45)], link=.06)
    VL.merge(sp, VL.crystal_cluster(rng, (.0, .0, 1.56), h=.3, n=3))
    return sp.soup(), dict(edge=.45)


# ------------------------------------------------------------------ obstacle kit: stones
def stones_a():
    """Rubble heap 1.0 m: fallen ashlar blocks and broken stone, a cracked cornice slab."""
    rng = np.random.default_rng(11)
    sp = Soup()
    VL.rubble_heap(sp, rng, (0, 0), r=.38, h=.45, n=7)
    for c, r, sq in [((0, 0, .3), .42, (1.1, .95, .75)), ((.34, .24, .18), .25, (1, 1, .7))]:
        add_rock(sp, 'stone', rng, c, r, sq, .6, detail=1)
    return sp.soup(), dict(edge=.45)


def stones_b():
    """Bone mound 0.9 m: a heap of skulls and long bones with a rib cage, one cold crystal in the middle."""
    rng = np.random.default_rng(12)
    sp = Soup()
    add_rock(sp, 'stone', rng, (0, 0, .15), .55, (1.1, 1.0, .45), .8, detail=1)
    for k in range(18):
        a = rng.uniform(0, math.tau)
        rr = rng.uniform(0, .5)
        z = .22 * (1 - rr / .6) + .04
        VL.merge(sp, VL.skull(rng, (rr * math.cos(a), rr * math.sin(a), max(.12, z + .1)), scale=rng.uniform(.9, 1.2), yaw=rng.uniform(0, 360)))
    for k in range(10):
        a = rng.uniform(0, math.tau)
        rr = rng.uniform(.15, .6)
        VL.merge(sp, VL.bone_long(rng, (rr * math.cos(a), rr * math.sin(a), .1 + .1 * rng.random()), length=rng.uniform(.3, .5), yaw=rng.uniform(0, 360), lean=rng.uniform(-30, 30)))
    VL.merge(sp, VL.crystal_cluster(rng, (.05, -.05, .2), h=.5, n=4))
    return sp.soup(), dict(edge=.45)


def stones_c():
    """Toppled statue 0.8 m: a hooded funerary figure broken at the knees, its head and a plinth in the rubble."""
    rng = np.random.default_rng(13)
    sp = Soup()
    st = VL.hooded_statue(rng, (0, 0, 0), h=1.0, yaw=0, glow=False)
    VL.merge(sp, L.xform(st, T(-.15, -.34, .45) @ Rx(-84) @ T(0, 0, -.1)))
    sp.box('stone', (-.45, -.4, .09), (.6, .6, .18), tile=1.0, top='stone', bottom=False)
    VL.rubble_heap(sp, rng, (.15, .2), r=.35, h=.3, n=4)
    return sp.soup(), dict(edge=.45)


# ------------------------------------------------------------------ 2-cell crypt arch (spans two adjacent cells along Y)
def arch_2cell():
    """3.0 m long x 1.2 m deep crypt arch: two ashlar piers joined by a pointed arch of voussoirs with a rune keystone line and a hanging
    chain.  Pivot at the centre of the pair's floor (the shared cell edge); long axis along Y, rotate 90 degrees for X-adjacent pairs."""
    rng = np.random.default_rng(21)
    sp = Soup()
    span, springing, apex = 1.05, 2.1, 3.05
    for side in (-1, 1):
        VL.ashlar_block(sp, -.6, .6, side * span - .3 if side < 0 else side * span - .3, side * span + .3, 0.0, springing)
        sp.box('stone', (0, side * span, .15), (1.3, .75, .3), tile=1.0, top='stone', bottom=False)
        sp.box('stone', (0, side * span, springing + .06), (1.3, .75, .13), tile=1.0, top='stone', bottom=False)
    n = 7
    for side in (-1, 1):
        for i in range(n):
            t0, t1 = i / n, (i + 1) / n
            f = lambda t: (side * span * (1 - t), springing + (apex - springing - .25) * (1 - (1 - t) ** 2.1))
            (y0, z0), (y1, z1) = f(t0), f(t1)
            ln = math.hypot(y1 - y0, z1 - z0)
            blk = Soup()
            blk.box('ashlar', (0, 0, 0), (1.2, ln * 1.03, .5), tile=1.0, top='stone', bottom=True)
            ang = math.degrees(math.atan2(z1 - z0, y1 - y0))
            VL.merge(sp, L.xform(blk.soup(), T(0, (y0 + y1) / 2, (z0 + z1) / 2 + .1) @ Rx(ang)))
    VL.ashlar_block(sp, -.6, .6, -.28, .28, apex - .18, apex + .05)           # keystone
    sp.box('rune', (-.605, 0, apex - .15), (.025, .35, .05), tile=1.0, bottom=False)
    chain_hang(sp, -.64, .5, 2.6, n=7)
    return sp.soup(), dict(edge=.9)


# ------------------------------------------------------------------ scatter
def sc_bones():
    rng = np.random.default_rng(31)
    sp = Soup()
    for (x, y, yaw) in [(0, 0, 20), (.28, .12, 95), (-.25, .2, 140), (.05, -.3, 60)]:
        VL.merge(sp, VL.bone_long(rng, (x, y, .03), length=rng.uniform(.28, .46), yaw=yaw, lean=rng.uniform(-6, 6)))
    VL.merge(sp, VL.rib_cage(rng, (-.2, -.2, .02), yaw=40))
    return sp.soup(), dict()


def sc_skulls():
    rng = np.random.default_rng(32)
    sp = Soup()
    for (x, y, yaw, sc) in [(0, 0, 10, 1.0), (.3, .18, 70, .95), (-.25, .22, 150, 1.05), (.12, -.3, 210, 1.0)]:
        VL.merge(sp, VL.skull(rng, (x, y, .02), scale=sc, yaw=yaw))
    return sp.soup(), dict()


def sc_candles():
    rng = np.random.default_rng(33)
    sp = Soup()
    for (x, y, h) in [(0, 0, .18), (.16, .1, .1), (-.14, .12, .22), (.05, -.17, .07), (-.12, -.12, .13)]:
        VL.merge(sp, VL.candle_stub(rng, (x, y, 0), h=h))
    return sp.soup(), dict()


def sc_rubble():
    rng = np.random.default_rng(34)
    sp = Soup()
    VL.rubble_heap(sp, rng, (0, 0), r=.4, h=.3, n=6)
    return sp.soup(), dict()


def sc_chains():
    rng = np.random.default_rng(35)
    sp = Soup()
    pts = [(-.5 + i * .2 + rng.uniform(-.03, .03), .1 * math.sin(i * 1.3) + rng.uniform(-.04, .04), .03) for i in range(6)]
    VL.chain_run(sp, pts, link=.06)
    sp.lathe('iron', (.55, .1, 0), [(.06, 0), (.07, .03), (.06, .06)], 6, cap=(False, True), tile=.3)       # a shackle ring
    return sp.soup(), dict()


def sc_urns():
    rng = np.random.default_rng(36)
    sp = Soup()
    VL.merge(sp, VL.urn(rng, (0, 0, 0), h=.55, r=.24, yaw=0))
    VL.merge(sp, L.xform(VL.urn(rng, (0, 0, 0), h=.45, r=.2, mat='iron'), T(.4, .2, .18) @ Ry(80)))
    for k in range(5):
        a = rng.uniform(0, math.tau)
        add_rock(sp, 'stone', rng, (.5 * math.cos(a), .5 * math.sin(a), .02), rng.uniform(.04, .08), (1, 1, .4), .3, detail=0)
    return sp.soup(), dict()


def sc_relics():
    rng = np.random.default_rng(37)
    sp = Soup()
    sp.box('iron', (0, 0, .02), (.9, .06, .035), tile=.3, bottom=True)                       # a sword on the floor
    sp.box('iron', (-.2, 0, .02), (.06, .26, .04), tile=.3, bottom=True)
    sp.lathe('iron', (.45, .35, 0), [(.2, .0), (.2, .04), (.05, .08)], 8, cap=(False, True), tile=.3)    # a rusted shield boss
    sp.lathe('iron', (-.25, -.35, 0), [(.05, 0), (.045, .06), (.07, .12), (.07, .15)], 6, cap=(True, True), tile=.3)   # a goblet
    return sp.soup(), dict()


def sc_crystals():
    rng = np.random.default_rng(38)
    sp = Soup()
    for (x, y, h) in [(0, 0, .5), (.25, .1, .32), (-.2, .15, .36), (.0, -.25, .28)]:
        VL.merge(sp, VL.crystal_cluster(rng, (x, y, 0), h=h, n=3, yaw=rng.uniform(0, 360)))
    return sp.soup(), dict(glow=('crystal',))


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
    col = np.ones((len(s['P']), 3, 3)) * np.array((.45, .52, .62))
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
    col = np.ones((len(s['P']), 3, 3)) * np.array((.36, .50, .60))
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
    s2, A, place = L.build_atlas(s, texs, size=size, pad=PAD, solid=solid, repeat=repeat or {}, downscale={'stone': 2, 'ashlar': 2, 'basalt': 2} if size <= 256 else {})
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
    'pillar_a': ('prop-vault-pillar', 1, '/Game/DnDDiceRoll/Theme/Props/SM_Prop_Vault_Pillar', 'SM_Prop_Vault_Pillar', pillar_a, 512, {}),
    'pillar_b': ('prop-vault-pillar-b', 1, '/Game/DnDDiceRoll/Theme/Props/SM_Prop_Vault_Pillar_B', 'SM_Prop_Vault_Pillar_B', pillar_b, 512, {}),
    'pillar_c': ('prop-vault-pillar-c', 1, '/Game/DnDDiceRoll/Theme/Props/SM_Prop_Vault_Pillar_C', 'SM_Prop_Vault_Pillar_C', pillar_c, 512, {}),
    'wall_a': ('prop-vault-wall', 1, '/Game/DnDDiceRoll/Theme/Props/SM_Prop_Vault_Wall', 'SM_Prop_Vault_Wall', wall_a, 512, {}),
    'wall_b': ('prop-vault-wall-b', 1, '/Game/DnDDiceRoll/Theme/Props/SM_Prop_Vault_Wall_B', 'SM_Prop_Vault_Wall_B', wall_b, 512, {}),
    'wall_c': ('prop-vault-wall-c', 1, '/Game/DnDDiceRoll/Theme/Props/SM_Prop_Vault_Wall_C', 'SM_Prop_Vault_Wall_C', wall_c, 512, {}),
    'stones_a': ('prop-vault-stones', 1, '/Game/DnDDiceRoll/Theme/Props/SM_Prop_Vault_Stones', 'SM_Prop_Vault_Stones', stones_a, 512, {}),
    'stones_b': ('prop-vault-stones-b', 1, '/Game/DnDDiceRoll/Theme/Props/SM_Prop_Vault_Stones_B', 'SM_Prop_Vault_Stones_B', stones_b, 512, {}),
    'stones_c': ('prop-vault-stones-c', 1, '/Game/DnDDiceRoll/Theme/Props/SM_Prop_Vault_Stones_C', 'SM_Prop_Vault_Stones_C', stones_c, 512, {}),
    'arch2': ('prop-vault-arch-2cell', 1, '/Game/DnDDiceRoll/Theme/Props/SM_Prop_Vault_Arch2Cell', 'SM_Prop_Vault_Arch2Cell', arch_2cell, 512, {}),
    'sc_bones': ('scatter-vault-bones', 1, '/Game/DnDDiceRoll/Theme/Scatter/SM_Scatter_Vault_Bones', 'SM_Scatter_Vault_Bones', sc_bones, 256, {}),
    'sc_skulls': ('scatter-vault-skulls', 1, '/Game/DnDDiceRoll/Theme/Scatter/SM_Scatter_Vault_Skulls', 'SM_Scatter_Vault_Skulls', sc_skulls, 256, {}),
    'sc_candles': ('scatter-vault-candles', 1, '/Game/DnDDiceRoll/Theme/Scatter/SM_Scatter_Vault_Candles', 'SM_Scatter_Vault_Candles', sc_candles, 256, {}),
    'sc_rubble': ('scatter-vault-rubble', 1, '/Game/DnDDiceRoll/Theme/Scatter/SM_Scatter_Vault_Rubble', 'SM_Scatter_Vault_Rubble', sc_rubble, 256, {}),
    'sc_chains': ('scatter-vault-chains', 1, '/Game/DnDDiceRoll/Theme/Scatter/SM_Scatter_Vault_Chains', 'SM_Scatter_Vault_Chains', sc_chains, 256, {}),
    'sc_urns': ('scatter-vault-urns', 1, '/Game/DnDDiceRoll/Theme/Scatter/SM_Scatter_Vault_Urns', 'SM_Scatter_Vault_Urns', sc_urns, 256, {}),
    'sc_relics': ('scatter-vault-relics', 1, '/Game/DnDDiceRoll/Theme/Scatter/SM_Scatter_Vault_Relics', 'SM_Scatter_Vault_Relics', sc_relics, 256, {}),
    'sc_crystals': ('scatter-vault-crystals', 1, '/Game/DnDDiceRoll/Theme/Scatter/SM_Scatter_Vault_Crystals', 'SM_Scatter_Vault_Crystals', sc_crystals, 256, {}),
}
FX = {
    'mist_a': ('fx-vault-mist-card', 1, '/Game/DnDDiceRoll/Theme/Fx/SM_Fx_Vault_MistCard', 'SM_Fx_Vault_MistCard', lambda: fx_cards('mist_a'), 256),
    'mist_b': ('fx-vault-mist-card-b', 1, '/Game/DnDDiceRoll/Theme/Fx/SM_Fx_Vault_MistCard_B', 'SM_Fx_Vault_MistCard_B', lambda: fx_cards('mist_b'), 256),
    'mist_c': ('fx-vault-mist-card-c', 1, '/Game/DnDDiceRoll/Theme/Fx/SM_Fx_Vault_MistCard_C', 'SM_Fx_Vault_MistCard_C', lambda: fx_cards('mist_c'), 256),
    'shaft_a': ('fx-vault-light-shaft', 1, '/Game/DnDDiceRoll/Theme/Fx/SM_Fx_Vault_LightShaft', 'SM_Fx_Vault_LightShaft', lambda: fx_shaft('prism'), 128),
    'shaft_b': ('fx-vault-light-shaft-b', 1, '/Game/DnDDiceRoll/Theme/Fx/SM_Fx_Vault_LightShaft_B', 'SM_Fx_Vault_LightShaft_B', lambda: fx_shaft('cross'), 128),
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
        finish(asset, unreal, rev, name, s, texs, size, {}, alpha=a * 0.5, bake=False)         # cold cards read brighter than the stone: keep them faint
    if not only:
        with open(os.path.join(OUT_SRC, 'kit_metrics.json'), 'w') as fh:
            json.dump(METRICS, fh, indent=1)


main()
