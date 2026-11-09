"""Root of the World r002 world (issue #149): metric world shell + occluder mesh, authored for the HUD-safe camera.

Run from the repository root:   blender -b --python Art/dark-fantasy-sources/r002-root/scripts/build_root_r002.py

Writes
  Art/world-root/r002/SM_World_Root.glb                          world shell: ground, dressing, trunks, canopy (outside the sight volume)
  Art/world-root-occluders/r001/SM_World_Root_Occluders.glb      pieces that enter the sight volume (dither fade), never near an eye
  Art/dark-fantasy-sources/r002-root/world_metrics.json          the measured numbers quoted in the README
  Art/dark-fantasy-sources/r002-root/T_World_Root_Atlas.png      the 2048 x 2048 atlas (identical image embedded in both glbs)

Nothing from r001's geometry is reused (r001's Root was a 106 m diorama whose giant tree stood 4 m behind the board and swallowed the
HUD-safe camera); only the identity is kept: the Root of the World, its roots, mossy flagstones and fungus, light falling through
the canopy.  The paving recipe is #145's (copied, see paving_root.py).  Everything is original and procedural.

Units: metres, 1 cell = 1.5 m, the 18 x 12 cell footprint is 27 x 18 m centred on the origin (the 16 x 11 board sits inside it), +X
forward, Z up.  COLOR_0 is RGBA: RGB = baked light, A = glow mask (alpha 1 only on fungus caps and sap veins, 0 elsewhere).
"""
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import r002_lib as R
from r002_lib import L, bpy, np, Vector, make_bvh, tri_normals, smoothstep, fbm, vnoise, subdivide, ambient, bake_ao, tri_soup, edge_lengths
import camera_volume as CV
import paving_root as PV
import root_lib as RL
from prims import Soup, rock

ROOT = R.ROOT
OUT_WORLD = os.path.join(ROOT, 'Art', 'world-root', 'r002')
OUT_OCC = os.path.join(ROOT, 'Art', 'world-root-occluders', 'r001')
OUT_SRC = os.path.join(ROOT, 'Art', 'dark-fantasy-sources', 'r002-root')

FX, FY, MARGIN = CV.FX, CV.FY, CV.MARGIN       # 13.5 x 9 m half extents, half-cell margin
MX, MY = FX + MARGIN, FY + MARGIN
ZONE = 3.6                                       # stone field extends up to here beyond the margin (irregular fade)
EXTENT = 96.0                                   # ground half extent
ATLAS = 2048
PAD = 16
SIGHT_MARGIN = 1.0                               # metres kept between the world shell and the sight volume
GLOW_MATS = ('glowcap', 'veins')
BASE_Z = -.05
FLOOR_TILE_M = 2.0
rng = np.random.default_rng(1491)
STATS = {}
PIECES = []                                      # (name, kind, soup)  kind: 'world' | 'occ?' (classified later)

VOL = CV.Volume(margin=SIGHT_MARGIN)


# ------------------------------------------------------------------ field functions (metres)
def rect_dr(x, y):
    x, y = np.asarray(x, float), np.asarray(y, float)
    return np.hypot(np.maximum(0, np.abs(x) - MX), np.maximum(0, np.abs(y) - MY))


def hull_cap(x, y):
    """Lowest height (m above the floor datum) at which the sight volume (plus margin) exists above the map point."""
    zlo, _ = VOL.z_interval(np.atleast_1d(x), np.atleast_1d(y))
    return zlo


_ZLO_CACHE = {}


def cap_at(x, y, r=0.0):
    """Allowed height of a mass of radius r centred at (x, y): lowest hull height over its footprint disc."""
    pts = [(x, y)] + [(x + r * math.cos(a), y + r * math.sin(a)) for a in np.linspace(0, math.tau, 12, endpoint=False)] if r > 0 else [(x, y)]
    pts = np.array(pts)
    return float(hull_cap(pts[:, 0], pts[:, 1]).min())


def relief(x, y):
    """Ground height: flat clearing, a bowl that rises away from the board, capped under the sight volume."""
    x, y = np.asarray(x, float), np.asarray(y, float)
    dr = rect_dr(x, y)
    bowl = 9.0 * smoothstep(4.0, 80.0, dr) ** 1.25
    und = (fbm(x * .55 + 5, y * .55 - 8, 3) - .5) * 2.6 + (fbm(x * 1.6 - 4, y * 1.6 + 2, 8) - .5) * .5
    z = (bowl + und * smoothstep(5.0, 22.0, dr)) * smoothstep(4.5, 9.0, dr)
    z = np.maximum(z, 0.0)
    cap = np.reshape(hull_cap(x.ravel(), y.ravel()), x.shape) - SIGHT_MARGIN - .8
    return np.minimum(z, np.maximum(cap, 0.0))


def in_footprint(x, y, m=.6):
    return abs(x) < MX + m and abs(y) < MY + m


def ground_fn(x, y):
    return float(relief(np.array([x]), np.array([y]))[0])


def light_field(x, y):
    """Baked ambient light (RGB multiplier before the shoulder): bright clearing, canopy dapple, darker forest away from the board."""
    x, y = np.asarray(x, float), np.asarray(y, float)
    dr = rect_dr(x, y)
    clearing = np.exp(-dr / 14.0)
    dap = smoothstep(.45, .74, fbm(x * .8 + 13, y * .8 - 7, 21))
    v = .62 + .28 * clearing + .38 * dap * (.45 + .55 * clearing)
    warm = np.array((1.0, .95, .80))
    cool = np.array((.80, .92, .86))
    t = np.clip(dap * (.5 + clearing), 0, 1)[..., None]
    return v[..., None] * (cool * (1 - t) + warm * t)


def light_units(xy):
    xy = np.asarray(xy, float) * PV.SCALE
    return light_field(xy[..., 0], xy[..., 1])


# ------------------------------------------------------------------ small soup utilities
def finish_piece(sp_or_soup, name, kind='world', tint=None):
    s = sp_or_soup.soup() if hasattr(sp_or_soup, 'soup') else sp_or_soup
    if not len(s['P']):
        return
    PIECES.append(dict(name=name, kind=kind, soup=s, tint=tint))


def shade_vertical(s, z0, height, lo=.55, hi=1.0):
    """Per-corner light = light field at the corner xy x height gradient x slight up-facing brightening."""
    P = s['P']
    N = tri_normals(P)
    lf = light_field(P[..., 0], P[..., 1])
    g = lo + (hi - lo) * np.clip((P[..., 2] - z0) / max(height, 1e-3), 0, 1)
    up = (.88 + .12 * np.clip(N[:, 2], -1, 1))[:, None]
    s['C'] = lf * (g * up)[..., None]
    return s


def place(s, x, y, z=0.0, yaw=0.0):
    return L.xform(s, T(x, y, z) @ Rz(yaw))


def T(x, y, z):
    return L.T(x, y, z)


def Rz(a):
    return L.Rz(a)


def ring_xy(r0, r1, a0=0.0, a1=360.0):
    a = math.radians(rng.uniform(a0, a1))
    r = rng.uniform(r0, r1)
    return r * math.cos(a), r * math.sin(a)


# ------------------------------------------------------------------ ground
def build_ground():
    PV.configure(MX + ZONE, MY + ZONE, (MX, MY), (1.0, ZONE - .2))
    stones = PV.build_paving(light_units, ao_bvh_holder['bvh'])
    stones = L.xform(stones, L.S(PV.SCALE))
    for k, v in PV.STATS.items():
        STATS[k] = v
    # the base plane: 8 m quads subdivided to 75 cm near the board, 1.5 m to 30 m, 3 m to 60 m
    n = int(EXTENT // 8)
    tris = []
    for i in range(-n, n):
        for j in range(-n, n):
            x0, y0 = i * 8.0, j * 8.0
            q = [(x0, y0), (x0 + 8, y0), (x0 + 8, y0 + 8), (x0, y0 + 8)]
            for t in ((0, 1, 2), (0, 2, 3)):
                tris.append([(q[k][0], q[k][1], 0.0) for k in t])
    s = tri_soup(tris, [[(0, 0)] * 3] * len(tris), np.ones((len(tris), 3, 3)), 'floor')

    def lim(mid):
        d = rect_dr(mid[..., 0], mid[..., 1])
        k = 1.42                                           # right-triangle hypotenuse = 1.42 x leg: legs end up at the nominal spacing
        return k * np.where(d < .01, 3.0, np.where(d < 11.0, .75, np.where(d < 30.0, 1.5, np.where(d < 60.0, 3.0, 8.0))))
    s = subdivide(s, lim)
    P = s['P'].copy()
    P[..., 2] = BASE_Z + relief(P[..., 0].ravel(), P[..., 1].ravel()).reshape(P.shape[:2])
    s['P'] = P
    # make sure the triangles face up
    f = tri_normals(P)[:, 2] < 0
    s['P'][f] = s['P'][f][:, [0, 2, 1]]
    # UV: continuous scale, 2 m tile near the board (512 px, 256 texels/m), up to 8 m far away; rotated 21 degrees (off the cell axes)
    ct, st_ = math.cos(math.radians(21)), math.sin(math.radians(21))
    uv = np.empty((len(P), 3, 2))
    dr = rect_dr(P[..., 0], P[..., 1])
    sc = FLOOR_TILE_M + 6.0 * smoothstep(14.0, 60.0, dr)
    uv[..., 0] = (P[..., 0] * ct - P[..., 1] * st_) / sc
    uv[..., 1] = (P[..., 0] * st_ + P[..., 1] * ct) / sc
    s['UV'] = uv
    # colour: light field x moss / earth / leaf tint
    xs, ys = P[..., 0], P[..., 1]
    moss = smoothstep(.50, .68, .6 * fbm(xs * .45 + 7, ys * .45 - 3, 5) + .4 * vnoise(xs * .9, ys * .9, 1.0, 9))
    leaf = smoothstep(.55, .75, fbm(xs * .6 + 31, ys * .6 + 17, 13))
    tint = (np.array((1.12, .98, .84)) * (1 - moss[..., None]) + np.array((.84, 1.16, .74)) * moss[..., None])
    tint = tint * (1 - .5 * leaf[..., None]) + np.array((1.22, .96, .66)) * .5 * leaf[..., None]
    shade = np.where(dr < 0.2, .78, 1.0)[..., None] * (1 - .25 * leaf[..., None] * (dr < 0.2)[..., None])      # under the stones: grout stays dark, not orange
    s['C'] = light_field(xs, ys) * tint * shade
    STATS['base_tris'] = int(len(s['P']))
    return stones, s


# ------------------------------------------------------------------ dressing and masses
def stump_with_roots(x, y, name, r_base=1.4, desired=8.0, n_roots=7, kind='world', tile=2.0, fungus=True, lean=(0, 0)):
    g = ground_fn(x, y)
    cap = cap_at(x, y, r_base * 2.4) - g - SIGHT_MARGIN
    h = min(desired, cap - 2.8)
    if h < 1.2:
        return None
    sp = Soup()
    top = RL.trunk(sp, rng, (x, y), h, r_base, r_base * .82, lean=lean, sides=10, tile=tile, broken=True, ground_z=g, flare=1.9, rings_per_m=.3)
    RL.buttress_roots(sp, rng, (x, y), r_base, min(h * .5, 2.6), n_roots, ground_fn, reach=2.6)
    s = shade_vertical(sp.soup(), g, h + 3, lo=.5, hi=1.0)
    finish_piece(s, name, kind)
    return dict(x=x, y=y, r=r_base, h=h, top=g + h)


def fallen_log(x, y, yaw, length, r, name):
    """A fallen trunk lying along +X (then yawed), with a root ball at the thick end, half sunk in the ground."""
    g = ground_fn(x, y)
    if cap_at(x, y, length * .55) - g - SIGHT_MARGIN < 2 * r + .3:
        return None
    sp = Soup()
    n = 8
    sides = 10
    rings, jit = [], []
    for i in range(n + 1):
        t = i / n
        rr = r * (1 - .25 * t) * (1.0 + (.35 if i == 0 else 0))
        rings.append((0.0, 0.0, -length / 2 + length * t, rr, rr))        # built along +z, rotated onto +x below
        jit.append([1 + .08 * math.sin(k * 2.3 + i) + rng.uniform(-.04, .04) for k in range(sides)])
    sp.loft('bark', rings, sides, (0, 0, max(1, round(math.tau * r / 2.0)), length / 2.0), 0.0, (True, True), jit)
    s = L.xform(sp.soup(), T(0, 0, r * .92) @ RL.Ry(90))
    rb = Soup()
    for k in range(6):
        a = rng.uniform(0, math.tau)
        pts, rad = [], []
        m = 6
        for i in range(m):
            t = i / (m - 1)
            pts.append((-length / 2 - .2 - .9 * t * math.cos(a) - .3 * t, 1.5 * t * math.sin(a), max(r * .95 + 1.4 * t * abs(math.cos(a * 1.3)), r * .5)))
            rad.append(max(r * .5 * (1 - .8 * t), .04))
        RL.tube_path(rb, 'bark', pts, rad, 5, 1.4, cap=(False, True))
    s = L.cat(s, rb.soup())
    s = place(s, x, y, g, yaw)
    s = shade_vertical(s, g, 2 * r + 1.2, lo=.55, hi=1.0)
    finish_piece(s, name, 'world')
    return True


def boulder_cluster(x, y, name, rs=(.5, 1.6)):
    g = ground_fn(x, y)
    sp = Soup()
    k = int(rng.integers(1, 4))
    big = 0
    for i in range(k):
        r = rng.uniform(*rs) * (1 if i == 0 else .5)
        dx, dy = (0, 0) if i == 0 else (rng.normal(0, r * 1.8), rng.normal(0, r * 1.8))
        RL.add_rock(sp, 'stone', rng, (dx, dy, r * .35), r, (1.1, rng.uniform(.8, 1.1), rng.uniform(.5, .8)), tile=1.0, detail=1, lean=rng.uniform(-.1, .1), floor=-.1)
        big = max(big, r * .35 + r * .8)
    if cap_at(x, y, 2.4) - g - SIGHT_MARGIN < big + .2:
        return None
    s = place(sp.soup(), x, y, g, rng.uniform(0, 360))
    s = shade_vertical(s, g, big + .3, lo=.62, hi=1.0)
    finish_piece(s, name, 'world')
    return dict(x=x, y=y, h=big)


def root_snake(x, y, ang, length, r0, name, depth=2):
    g = ground_fn(x, y)
    sp = Soup()
    RL.root_system(sp, rng, (x, y), ang, length, r0, ground_fn, depth=depth, sides=5, tile=2.4, wander=.45, hug=.5, n=8, stop=in_footprint)
    if not sp.P:
        return
    s = shade_vertical(sp.soup(), g - .2, 1.0, lo=.55, hi=1.0)
    # moss on the up-facing parts of roots
    N = tri_normals(s['P'])
    m = smoothstep(.4, .9, N[:, 2])[:, None] * (.5 + .5 * smoothstep(.45, .7, fbm(s['P'][..., 0].mean(1) * .8, s['P'][..., 1].mean(1) * .8, 7)))[:, None]
    s['C'] = s['C'] * (1 + m[..., None] * (np.array((.72, 1.05, .6)) - 1))
    finish_piece(s, name, 'world')


def fungus_on(x, y, z, name, n=4, scale=1.0, glow=True):
    sp = Soup()
    RL.fungus_cluster(sp, rng, (x, y, z), n=n, scale=scale, glow=glow)
    s = sp.soup()
    s['C'] = np.broadcast_to(light_field(s['P'][..., 0], s['P'][..., 1]), s['P'].shape) * 1.0
    finish_piece(s, name, 'world')


def fern_at(x, y, name):
    g = ground_fn(x, y)
    if cap_at(x, y) - g - SIGHT_MARGIN < 1.2:
        return
    s = RL.fern_clump(rng, (x, y), h=rng.uniform(.6, 1.15), w=rng.uniform(.8, 1.4), blades=int(rng.integers(5, 8)), ground_z=g)
    lf = light_field(s['P'][..., 0], s['P'][..., 1])
    zz = s['P'][..., 2]
    s['C'] = lf * (.7 + .5 * np.clip((zz - g) / 1.0, 0, 1))[..., None]
    finish_piece(s, name, 'world')


def arch_root(x, y, ang, span, apex, r_end, name):
    """A great root arching out of the ground and back in (an occluder candidate): two feet `span` apart along `ang`."""
    d = np.array((math.cos(ang), math.sin(ang)))
    c = np.array((x, y))
    f0, f1 = c - d * span / 2, c + d * span / 2
    g0, g1 = ground_fn(*f0), ground_fn(*f1)
    sp = Soup()
    pts, rad = [], []
    m = 15
    for i in range(m):
        t = i / (m - 1)
        base = c + d * (t - .5) * span
        sway = np.array((-d[1], d[0])) * math.sin(t * math.pi * 2) * span * .06
        z = g0 * (1 - t) + g1 * t + apex * math.sin(math.pi * t) ** .8
        r = r_end * (.55 + .45 * abs(math.cos(math.pi * t))) * (1 + .12 * math.sin(t * 17))
        p = base + sway
        pts.append((p[0], p[1], z + (r * .4 if i in (0, m - 1) else 0)))
        rad.append(r)
    RL.tube_path(sp, 'bark', pts, rad, 8, 2.0, cap=(True, True))
    # buttress fins at the feet
    for f in (f0, f1):
        RL.buttress_roots(sp, rng, tuple(f), r_end * .8, r_end * 1.4, 4, ground_fn, reach=2.2)
    s = shade_vertical(sp.soup(), min(g0, g1), apex + 1, lo=.55, hi=1.0)
    N = tri_normals(s['P'])
    m_ = smoothstep(.35, .9, N[:, 2])[:, None]
    s['C'] = s['C'] * (1 + m_[..., None] * (np.array((.72, 1.05, .6)) - 1))
    finish_piece(s, name, 'world')


def great_tree(x, y, name, height, r_base, tilt=(0, 0), veins=True, canopy=True, tile=4.0, sides=10, rings_per_m=.11):
    g = ground_fn(x, y)
    sp = Soup()
    RL.trunk(sp, rng, (x, y), height, r_base, r_base * .62, lean=tilt, sides=sides, tile=tile, broken=False, ground_z=g, flare=2.4, rings_per_m=rings_per_m)
    RL.buttress_roots(sp, rng, (x, y), r_base * 1.2, min(height * .12, 9.0), 7, ground_fn, reach=2.6, sides=6, tile=4.0)
    if veins:
        RL.sap_veins(sp, rng, (x, y), r_base * 1.02, g + 1.0, g + height * .8, n=4, width=.14 + r_base * .01)
    s = shade_vertical(sp.soup(), g, height, lo=.5, hi=1.15)
    finish_piece(s, name, 'world')
    if canopy:
        cs = Soup()
        top = g + height
        for k in range(int(rng.integers(3, 6))):
            a = rng.uniform(0, math.tau)
            rr = rng.uniform(.6, 1.6) * r_base
            cx, cy = x + math.cos(a) * rr * 1.5 + tilt[0] * height, y + math.sin(a) * rr * 1.5 + tilt[1] * height
            rb = rng.uniform(.8, 1.5) * r_base
            RL.canopy_blob(cs, rng, (cx, cy, top - rb * rng.uniform(0, .5)), rb, (1.0, 1.0, .55), detail=1)
        sc = cs.soup()
        sc['C'] = np.broadcast_to(light_field(sc['P'][..., 0], sc['P'][..., 1])[:, :, :], sc['P'].shape) * (.75 + .5 * np.clip((sc['P'][..., 2] - (top - r_base * 2)) / (r_base * 2.5), 0, 1))[..., None]
        finish_piece(sc, name + '_canopy', 'world')


# ------------------------------------------------------------------ layout
def layout():
    # ---- the near clearing edge: low roots wrapping the paving, boulders, fungus (below the sight volume by construction)
    for i in range(72):
        a = rng.uniform(0, math.tau)
        # start on the clearing's rectangle boundary region, wander out
        x, y = ring_xy(8, 46)
        dr = float(rect_dr(x, y))
        if dr < 2.6:
            continue
        ang = math.atan2(-y, -x) + rng.uniform(-.9, .9) + (math.pi if rng.random() < .35 else 0)
        root_snake(x, y, ang, rng.uniform(3.5, 11.0), rng.uniform(.14, .55) * (1 + dr / 35), f'root{i}', depth=1)
    for i in range(46):
        x, y = ring_xy(11, 42)
        if float(rect_dr(x, y)) < 2.6:
            continue
        boulder_cluster(x, y, f'boulder{i}', rs=(.45, 1.7))
    # a ring of boulders and broken stone right at the paving's edge (kept low)
    for i in range(18):
        a = rng.uniform(0, math.tau)
        x, y = ring_xy(10, 30)
        if float(rect_dr(x, y)) < 2.8 or float(rect_dr(x, y)) > 9:
            continue
        boulder_cluster(x, y, f'edgeboulder{i}', rs=(.35, .8))
    # ---- stumps (broken trunks capped under the sight volume), fallen logs
    stumps = []
    attempts = 0
    while len(stumps) < 26 and attempts < 500:
        attempts += 1
        x, y = ring_xy(17, 60)
        r = rng.uniform(.8, 2.6)
        st = stump_with_roots(x, y, f'stump{len(stumps)}', r_base=r, desired=rng.uniform(5, 30), n_roots=int(rng.integers(4, 7)), tile=2.0)
        if st:
            stumps.append(st)
    logs = 0
    attempts = 0
    while logs < 10 and attempts < 200:
        attempts += 1
        x, y = ring_xy(14, 50)
        if float(rect_dr(x, y)) < 4:
            continue
        if fallen_log(x, y, rng.uniform(0, 360), rng.uniform(7, 15), rng.uniform(.5, 1.1), f'log{logs}'):
            logs += 1
    # fungus on the stumps, logs and in rings on the ground
    for i, st in enumerate(stumps):
        for k in range(int(rng.integers(0, 3))):
            a = rng.uniform(0, math.tau)
            rr = st['r'] * rng.uniform(1.0, 1.5)
            x, y = st['x'] + rr * math.cos(a), st['y'] + rr * math.sin(a)
            fungus_on(x, y, ground_fn(x, y) + rng.uniform(.0, 1.6), f'fung_st{i}_{k}', n=int(rng.integers(2, 5)), scale=rng.uniform(.9, 1.8))
    for i in range(44):
        x, y = ring_xy(5, 40)
        if float(rect_dr(x, y)) < 2.6:
            continue
        fungus_on(x, y, ground_fn(x, y), f'fung{i}', n=int(rng.integers(2, 5)), scale=rng.uniform(.6, 1.4), glow=rng.random() < .72)
    for i in range(190):
        x, y = ring_xy(4, 48)
        if float(rect_dr(x, y)) < 2.4:
            continue
        fern_at(x, y, f'fern{i}')
    # ---- the mid ring and the great trees (outside the sight volume's reach: full height)
    outer = 0
    attempts = 0
    while outer < 22 and attempts < 400:
        attempts += 1
        a = rng.uniform(0, math.tau)
        d = rng.uniform(63, 90)
        r = rng.uniform(2.2, 5.0)
        x, y = d * math.cos(a), d * math.sin(a)
        if max(abs(x), abs(y)) + r > EXTENT - 2:
            continue
        great_tree(x, y, f'tree{outer}', rng.uniform(32, 62), r, tilt=(rng.uniform(-.015, .015), rng.uniform(-.015, .015)), veins=rng.random() < .35, tile=12.0)
        outer += 1
    great_tree(-4.0, 78.0, 'rootoftheworld', 98.0, 11.0, tilt=(.004, 0.0), veins=True, canopy=True, tile=12.0, sides=16, rings_per_m=.12)
    # ---- occluder candidates: great roots arching over the clearing's edge
    arches = 0
    for k, ang0 in enumerate(np.linspace(0, 360, 9, endpoint=False)):
        a = math.radians(ang0 + rng.uniform(-12, 12))
        d = rng.uniform(21, 27)
        x, y = d * math.cos(a), d * math.sin(a)
        arch_root(x, y, a + math.pi / 2 + rng.uniform(-.3, .3), rng.uniform(8, 12), rng.uniform(7, 11), rng.uniform(.8, 1.3), f'arch{k}')
        arches += 1
    # low leaning logs close to the board's long sides (the camera can pass behind them)
    for k in range(4):
        side = 1 if k % 2 == 0 else -1
        x = rng.uniform(-9, 9)
        y = side * (MY + rng.uniform(5.5, 8.0))
        fallen_log(x, y, rng.uniform(-20, 20), rng.uniform(8, 12), rng.uniform(.7, 1.0), f'nearlog{k}')


# ------------------------------------------------------------------ classification against the sight volume
OCCLUDER_PREFIXES = ('arch', 'nearlog')


def classify():
    """Pieces whose geometry enters the sight volume (inflated by SIGHT_MARGIN) become occluders.  Eye clearance is checked later."""
    world, occ = [], []
    inside_counts = {}
    for p in PIECES:
        P = p['soup']['P']
        pts = np.concatenate([P.reshape(-1, 3), P.mean(1), ((P + P[:, [1, 2, 0]]) / 2).reshape(-1, 3)])
        n_in = int((VOL.contains(pts) & (pts[:, 2] > .15)).sum())
        if n_in and p['name'].startswith(OCCLUDER_PREFIXES):
            occ.append(p)                                  # designed near architecture: dither-faded occluder
            inside_counts[p['name']] = n_in
        elif n_in:
            STATS.setdefault('pieces_dropped_for_sight_volume', []).append(p['name'])      # a dressing piece that strays into the volume is dropped
        else:
            world.append(p)
    return world, occ, inside_counts


ao_bvh_holder = {}


def main():
    os.makedirs(OUT_WORLD, exist_ok=True)
    os.makedirs(OUT_OCC, exist_ok=True)
    os.makedirs(OUT_SRC, exist_ok=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    # a first pass of the dressing (no ground) gives the BVH the stones' AO samples against
    layout()
    # nothing above 10 cm may stand inside the 27 x 18 m footprint (roots and logs are stopped or dropped)
    kept, dropped = [], []
    for p in PIECES:
        P = p['soup']['P']
        c = np.concatenate([P.reshape(-1, 3), P.mean(1)])
        if ((np.abs(c[:, 0]) < FX + .25) & (np.abs(c[:, 1]) < FY + .25) & (c[:, 2] > .10)).any():
            dropped.append(p['name'])
        else:
            kept.append(p)
    PIECES[:] = kept
    STATS['pieces_dropped_for_footprint'] = dropped
    world_p, occ_p, inside = classify()
    print('pieces', len(PIECES), 'world', len(world_p), 'occluders', len(occ_p), inside)
    arch = L.cat(*[p['soup'] for p in PIECES if len(p['soup']['P'])])
    ao_bvh_holder['bvh'] = make_bvh(arch['P'])
    stones, base = build_ground()
    texs = RL.root_textures()
    # ---- AO on the near dressing, then assemble
    near = lambda s_: rect_dr(s_['P'].mean(1)[:, 0], s_['P'].mean(1)[:, 1]) < 40.0
    bvh_all = make_bvh(L.cat(stones, arch, base)['P'])
    ao_total = 0
    for p in PIECES:
        s = p['soup']
        if s['C'] is None:
            continue
        mid = s['P'].mean(1)
        if float(rect_dr(mid[:, 0].mean(), mid[:, 1].mean())) < 40.0 and len(s['P']) < 6000 and not p['name'].startswith(('tree', 'rootoftheworld')):
            s2, nao = bake_ao(s, bvh_all, strength=.55, dist=2.0, n=8)
            p['soup'] = s2
            ao_total += nao
    # ground AO in the clearing ring around stones/roots
    near_base = rect_dr(base['P'].mean(1)[:, 0], base['P'].mean(1)[:, 1]) < 12.0
    b_n, b_f = L.sub(base, near_base), L.sub(base, ~near_base)
    b_n, nb = bake_ao(b_n, bvh_all, strength=.5, dist=2.0, n=8)
    ao_total += nb
    base = L.cat(b_n, b_f)
    STATS['ao_samples'] = ao_total
    world_pieces = [p for p in PIECES if p in world_p]
    occ_pieces = [p for p in PIECES if p in occ_p]
    world = L.cat(stones, base, *[p['soup'] for p in world_pieces])
    occl = L.cat(*[p['soup'] for p in occ_pieces]) if occ_pieces else None
    import re as _re
    grp = {}
    for p in PIECES:
        k = _re.sub(r'[0-9]+(_[0-9]+)?', '', p['name'])
        grp[k] = grp.get(k, 0) + len(p['soup']['P'])
    STATS['source_tris_by_group'] = grp
    print('GROUPS', grp, 'stones', len(stones['P']), 'base', len(base['P']))
    STATS['pieces'] = dict(total=len(PIECES), world=len(world_pieces), occluders=len(occ_pieces), occluder_names=[p['name'] for p in occ_pieces])
    # ---- light shoulder, glow
    for d in [x for x in (world, occl) if x is not None]:
        d['C'] = L.knee(d['C'] * 1.05)
        gl = np.isin(d['mat'], GLOW_MATS)
        d['C'][gl] = 1.0
    mats_all = list(dict.fromkeys(world['mat'].tolist() + (occl['mat'].tolist() if occl is not None else [])))
    dummy = tri_soup([[(0, 0, -9999), (1, 0, -9999), (0, 1, -9999)]] * len(mats_all), [[(.5, .5), (.51, .5), (.5, .51)]] * len(mats_all),
                     [[(1, 1, 1)] * 3] * len(mats_all), 'x')
    dummy['mat'] = np.array(mats_all, dtype=object)
    solid = [k for k in ('glowcap', 'veins', 'stem') if k in mats_all]
    A = None
    outs = []
    for d in (world, occl):
        if d is None:
            outs.append(None)
            continue
        d2, A_, place_ = L.build_atlas(L.cat(dummy, d), texs, size=ATLAS, pad=PAD, solid=solid, repeat={})
        keep_ = d2['P'][..., 2].max(1) > -5000
        outs.append(L.sub(d2, keep_))
        if A is not None:
            assert np.array_equal(A, A_), 'atlas differs between meshes'
        A = A_
    world, occl = outs
    used_px = sum((w + 2 * PAD) * (h + 2 * PAD) for (_, _, w, h) in place_.values())
    STATS['atlas'] = dict(size=ATLAS, regions=len(place_), pad_px=PAD, fill_fraction=round(used_px / ATLAS / ATLAS, 3), regions_px={k: [v[2], v[3]] for k, v in place_.items()})
    STATS['floor_texels_per_m'] = 512 / FLOOR_TILE_M
    STATS['paving_texels_per_m'] = 256
    bpy.ops.wm.read_factory_settings(use_empty=True)
    img = L.new_image('T_World_Root_Atlas', A)
    o1, g1 = R.make_object_rgba('SM_World_Root', world, img, glow_mats=GLOW_MATS, mat_name='M_World_Root')
    p1 = R.export_glb(o1, os.path.join(OUT_WORLD, 'SM_World_Root.glb'))
    o2 = g2 = p2 = None
    if occl is not None:
        o2, g2 = R.make_object_rgba('SM_World_Root_Occluders', occl, img, glow_mats=GLOW_MATS, mat_name='M_World_Root')
        p2 = R.export_glb(o2, os.path.join(OUT_OCC, 'SM_World_Root_Occluders.glb'))
    L.save_png(A, os.path.join(OUT_SRC, 'T_World_Root_Atlas.png'))
    for name, o, g, d, p in (('world', o1, g1, world, p1), ('occluders', o2, g2, occl, p2)):
        if d is None:
            continue
        P_ = d['P']
        e = edge_lengths(P_)
        dm = rect_dr(P_.mean(1)[:, 0], P_.mean(1)[:, 1])
        zc = P_.mean(1)[:, 2]
        ev = lambda m_: edge_lengths(P_[m_]).ravel() if m_.any() else np.zeros(1)
        e_ground = ev((dm < 8) & (zc < 1.0) & np.isin(d['mat'], ['floor']))
        e_stone = ev(np.isin(d['mat'], ['paveA', 'paveB', 'paveC', 'paveM']))
        e_dress = ev((dm < 14) & (zc >= .2) & ~np.isin(d['mat'], ['floor', 'paveA', 'paveB', 'paveC', 'paveM']))
        STATS[name] = dict(triangles=int(len(o.data.polygons)), glow_tris=g,
                           by_material={k: int((d['mat'] == k).sum()) for k in dict.fromkeys(d['mat'].tolist())},
                           edge_len_m_floor_within_8m_median_p90_max=[round(float(np.median(e_ground)), 3), round(float(np.percentile(e_ground, 90)), 3), round(float(e_ground.max()), 3)],
                           edge_len_m_stone_tops_median_p90=[round(float(np.median(e_stone)), 3), round(float(np.percentile(e_stone, 90)), 3)],
                           edge_len_m_dressing_above_20cm_within_14m_median_p90_max=[round(float(np.median(e_dress)), 3), round(float(np.percentile(e_dress, 90)), 3), round(float(e_dress.max()), 3)],
                           glb=R.glb_stats(p))
    STATS['footprint_m'] = [FX * 2, FY * 2]
    STATS['cell_m'] = 1.5
    with open(os.path.join(OUT_SRC, 'world_metrics.json'), 'w') as fh:
        json.dump(STATS, fh, indent=1)
    print('WORLD_METRICS', json.dumps({k: v for k, v in STATS.items() if k not in ('edge_orientation_histogram_10deg_bins_length_share',)}))


main()
