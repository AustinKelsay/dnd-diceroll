"""Shared Vault r002 (issue #149) builders: textures and procedural geometry (ashlar walls, pilasters, ribs, alcoves, the Waystone,
sarcophagi, chains, bones, urns, rubble).  Runs inside Blender 5.2 (numpy only).  All geometry is original code in metres, +X forward,
Z up.  The look is a drowned-dark crypt hall of cold basalt, bone, rusted iron and a single glow accent, the cold blue (0.5, 0.85, 1.0)
the game's Vault text already uses; glow exists only as vertex alpha on the 'rune' and 'crystal' materials.

Textures are procedural, periodic where they tile, at the r002 density: 512 px per 2 m (256 texels/m) for floor and ashlar, 256 px per
1 m for flagstones.  Nothing is taken from outside the repository.
"""
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import r002_lib as R
from r002_lib import L, bpy, np, Vector, smoothstep, vnoise, fbm, tri_normals, tile_noise

from prims import Soup, rock

GLOW_MATS = ('rune', 'crystal')


# ------------------------------------------------------------------ textures
def _rgba(rgb):
    t = np.ones((*rgb.shape[:2], 4), np.float32)
    t[..., :3] = np.clip(rgb, 0, 1)
    return t


def _wrap_dot(img, cx, cy, rx, ry, rgb, ang=0.0):
    n = img.shape[0]
    r = int(max(rx, ry)) + 2
    ys, xs = np.mgrid[-r:r + 1, -r:r + 1]
    ca, sa = math.cos(ang), math.sin(ang)
    u, v = xs * ca + ys * sa, -xs * sa + ys * ca
    m = (u / rx) ** 2 + (v / ry) ** 2 <= 1.0
    img[(cy + ys[m]) % n, (cx + xs[m]) % n, :3] = rgb


def floor_tile(n=512, size_m=2.0, seed=171):
    """Crypt floor between the flagstones: dark slate dust with faint slab joints and grit, seamless, 512 px = 2 m."""
    r = np.random.default_rng(seed)
    big, mid, fine = tile_noise(n, 3, seed), tile_noise(n, 9, seed + 1), tile_noise(n, 28, seed + 2)
    base = np.array((.16, .18, .23)) * (.78 + .4 * big[..., None]) * (.9 + .2 * mid[..., None]) * (.94 + .12 * fine[..., None])
    t = _rgba(base)
    px = n / size_m
    for _ in range(260):                                           # grit and chips
        _wrap_dot(t, int(r.integers(0, n)), int(r.integers(0, n)), r.uniform(1.5, 3.5), r.uniform(1.2, 2.6), np.array((.30, .32, .37)) * r.uniform(.8, 1.15), r.uniform(0, math.pi))
    for k in range(3):                                             # a few slab joints (hairline, off the axes by a few degrees)
        y0 = (k + .5) / 3 * n + r.uniform(-.05, .05) * n
        for x in range(n):
            y = int(y0 + 6 * math.sin(x / n * math.tau * 2 + k)) % n
            t[y, x, :3] *= .55
            t[(y + 1) % n, x, :3] *= .78
    return t


def ashlar_tile(n=512, size_m=2.0, seed=181):
    """Dressed ashlar: four courses (50 cm), blocks 70-130 cm long on a running bond, each with its own tone, dark mortar, soft
    bevel; seamless in both directions, 512 px = 2 m (256 texels/m)."""
    r = np.random.default_rng(seed)
    yy, xx = np.mgrid[0:n, 0:n]
    wx = (xx + .5) / n * size_m + .012 * (tile_noise(n, 4, seed + 11) - .5) * 2
    wy = (yy + .5) / n * size_m
    course_h = size_m / 4
    course = np.minimum((wy / course_h).astype(int), 3)
    pal = [np.array(c) for c in ((.25, .27, .33), (.29, .31, .37), (.22, .24, .30), (.27, .28, .34))]
    col = np.zeros((n, n, 3))
    dist = np.full((n, n), 9.0)
    for k in range(4):
        cnt = int(r.integers(2, 4))
        ls = r.uniform(.7, 1.3, cnt)
        ls = ls / ls.sum() * size_m
        edges = np.sort((r.uniform(0, size_m) + np.cumsum(ls)) % size_m)
        sid = np.searchsorted(edges, wx) % len(edges)
        d = np.min(np.minimum(np.abs(wx[..., None] - edges[None, None]), size_m - np.abs(wx[..., None] - edges[None, None])), axis=2)
        dy = np.minimum(np.abs(wy - k * course_h), np.abs(wy - (k + 1) * course_h))
        dy = np.minimum(dy, size_m - dy) if k in (0, 3) else dy
        m = course == k
        tones = np.array([pal[int(r.integers(4))] * r.uniform(.86, 1.14) for _ in edges])
        col = np.where(m[..., None], tones[sid], col)
        dist = np.where(m, np.minimum(d, dy), dist)
    n1, n2 = tile_noise(n, 5, seed + 1), tile_noise(n, 17, seed + 2)
    col = col * (.82 + .24 * n1[..., None] + .12 * n2[..., None] - .06)
    jm = .018
    edge = smoothstep(jm * .5, jm * 2.4, dist)
    bevel = 1 - .16 * (1 - smoothstep(jm * 2.4, jm * 7, dist))
    col = col * (bevel * (.50 + .50 * edge))[..., None]
    streak = smoothstep(.55, .85, tile_noise(n, 6, seed + 3) * (1 - wy / size_m * .4))      # damp streaks run down the face
    col = col * (1 - .16 * streak[..., None]) + np.array((.02, .05, .08)) * .5 * streak[..., None]
    return _rgba(col)


def stone_tile(n=256, seed=191, tint=(.30, .32, .38)):
    big, mid = tile_noise(n, 4, seed), tile_noise(n, 14, seed + 1)
    col = np.array(tint) * (.72 + .5 * big[..., None]) * (.9 + .2 * mid[..., None])
    crack = smoothstep(.47, .5, tile_noise(n, 9, seed + 2)) * (1 - smoothstep(.5, .53, tile_noise(n, 9, seed + 2)))
    col = col * (1 - .4 * crack[..., None])
    return _rgba(col)


def basalt_tile(n=256, seed=201):
    """The Waystone's dark, smooth stone: vertical flow lines, blue-black."""
    yy, xx = np.mgrid[0:n, 0:n]
    flow = tile_noise(n, 3, seed) * .5 + .5 * tile_noise(n, 22, seed + 1) * (.6 + .4 * np.sin(xx / n * math.tau * 6))
    col = np.array((.10, .12, .18)) * (.7 + .6 * flow[..., None])
    return _rgba(col)


def pave_tile(seed, base, crack, n=256):
    r = np.random.default_rng(seed)
    n1, n2, n3 = tile_noise(n, 5, seed), tile_noise(n, 14, seed + 9), tile_noise(n, 3, seed + 3)
    t = _rgba(np.array(base) * (.88 + .17 * n1[..., None] + .08 * n2[..., None] - .03))
    t[..., :3] *= (1 - .12 * smoothstep(.62, .85, n3))[..., None]
    for _ in range(crack):
        x0, y0 = r.integers(40, n - 40, 2)
        ang = r.uniform(0, math.tau)
        for k in range(int(r.integers(30, 70))):
            x = int(x0 + math.cos(ang + .3 * math.sin(k * .15)) * k)
            y = int(y0 + math.sin(ang + .3 * math.sin(k * .15)) * k)
            if 2 <= x < n - 2 and 2 <= y < n - 2:
                t[y, x, :3] *= .5
    return t


def bone_tile(n=64, seed=211):
    return _rgba(np.array((.62, .58, .48)) * (.78 + .36 * tile_noise(n, 5, seed)[..., None]) * (.92 + .16 * tile_noise(n, 16, seed + 1)[..., None]))


def iron_tile(n=64, seed=221):
    yy, xx = np.mgrid[0:n, 0:n]
    rust = smoothstep(.5, .8, tile_noise(n, 5, seed))
    col = np.array((.14, .14, .17)) * (.8 + .4 * tile_noise(n, 9, seed + 1)[..., None])
    col = col * (1 - rust[..., None]) + np.array((.34, .17, .08)) * rust[..., None] * (.8 + .4 * tile_noise(n, 11, seed + 2)[..., None])
    return _rgba(col)


def wood_tile(n=64, seed=231):
    yy, xx = np.mgrid[0:n, 0:n]
    grain = .5 + .5 * np.sin(xx / n * math.tau * 5 + 3 * tile_noise(n, 4, seed))
    return _rgba(np.array((.20, .13, .09)) * (.7 + .4 * grain[..., None]) * (.85 + .3 * tile_noise(n, 8, seed + 1)[..., None]))


def cloth_tile(n=64, seed=241):
    return _rgba(np.array((.30, .28, .26)) * (.75 + .4 * tile_noise(n, 6, seed)[..., None]))


def vault_textures():
    t = {}
    t['floor'] = floor_tile()
    t['ashlar'] = ashlar_tile()
    t['stone'] = stone_tile()
    t['basalt'] = basalt_tile()
    t['paveA'] = pave_tile(321, (.205, .225, .285), 2)
    t['paveB'] = pave_tile(331, (.225, .24, .30), 3)
    t['paveC'] = pave_tile(341, (.185, .205, .26), 1)
    p = pave_tile(351, (.19, .21, .27), 3)                          # a sunken, damp-darkened slab with a pale rime in its hollows
    rime = .6 * smoothstep(.45, .8, tile_noise(256, 3, 352) * .7 + tile_noise(256, 8, 353) * .3)
    p[..., :3] = p[..., :3] * (1 - rime[..., None]) + np.array((.26, .32, .40)) * (.75 + .4 * tile_noise(256, 12, 354))[..., None] * rime[..., None]
    t['paveM'] = p
    t['bone'] = bone_tile()
    t['iron'] = iron_tile()
    t['wood'] = wood_tile()
    t['cloth'] = cloth_tile()
    t['rune'] = L.swatch((.50, .85, 1.0))
    t['crystal'] = L.swatch((.62, .92, 1.0))
    return t


# ------------------------------------------------------------------ small geometry helpers
def xform(s, M):
    return L.xform(s, M)


def Rz(deg):
    return L.Rz(deg)


def T(x=0, y=0, z=0):
    return L.T(x, y, z)


def Rx(deg):
    a = math.radians(deg)
    M = np.eye(4)
    M[1, 1], M[1, 2], M[2, 1], M[2, 2] = math.cos(a), -math.sin(a), math.sin(a), math.cos(a)
    return M


def Ry(deg):
    a = math.radians(deg)
    M = np.eye(4)
    M[0, 0], M[0, 2], M[2, 0], M[2, 2] = math.cos(a), math.sin(a), -math.sin(a), math.cos(a)
    return M


def planar_uv(sp, mat, V, F, tile):
    for f in F:
        p = V[list(f)]
        n = np.cross(p[1] - p[0], p[2] - p[0])
        ax = int(np.argmax(np.abs(n)))
        a, b = [i for i in range(3) if i != ax]
        sp.tri(mat, *[tuple(q) for q in p], *[(q[a] / tile, -q[b] / tile) for q in p])


def add_rock(sp, mat, rng, c, r, squash=(1, 1, .7), tile=.5, detail=1, lean=0.0, floor=0.0):
    V, F = rock(rng, c, r, squash, detail)
    if lean:
        V[:, 0] += (V[:, 2] - c[2]) * lean
    V[:, 2] = np.maximum(V[:, 2], floor)
    planar_uv(sp, mat, V, F, tile)


def tube_path(sp, mat, pts, radii, sides, tile, cap=(True, True)):
    u1 = max(1, round(math.tau * float(np.mean(radii)) / tile)) if tile else 1
    sp.tube(mat, pts, radii, sides, tile=tile, patch=(0, 0, u1, 1), cap=cap)


def convex_prism(sp, mat, poly_xz, y0, y1, tile=1.0, mat_cap=None):
    """Extrude a convex polygon given in the XZ plane (x, z) between y0 and y1; faces are oriented outward from the centroid."""
    poly = [(float(a), float(b)) for a, b in poly_xz]
    cen = np.array((np.mean([p[0] for p in poly]), (y0 + y1) / 2, np.mean([p[1] for p in poly])))
    n = len(poly)

    def put(m, a, b, c, ua, ub, uc):
        pa, pb, pc = map(np.array, (a, b, c))
        nrm = np.cross(pb - pa, pc - pa)
        if np.dot(nrm, (pa + pb + pc) / 3 - cen) < 0:
            sp.tri(m, tuple(pa), tuple(pc), tuple(pb), ua, uc, ub)
        else:
            sp.tri(m, tuple(pa), tuple(pb), tuple(pc), ua, ub, uc)

    for i in range(n):
        j = (i + 1) % n
        (xa, za), (xb, zb) = poly[i], poly[j]
        a, b, c, d = (xa, y0, za), (xb, y0, zb), (xb, y1, zb), (xa, y1, za)
        ln = math.hypot(xb - xa, zb - za)
        u0, u1 = xa / tile, xb / tile
        put(mat, a, b, c, (u0, -za / tile), (u1, -zb / tile), (u1, -zb / tile))
        put(mat, a, c, d, (u0, -za / tile), (u1, -zb / tile), (u0, -za / tile))
    for k in range(1, n - 1):
        for yy in (y0, y1):
            m = mat_cap or mat
            put(m, (poly[0][0], yy, poly[0][1]), (poly[k][0], yy, poly[k][1]), (poly[k + 1][0], yy, poly[k + 1][1]),
                (poly[0][0] / tile, -poly[0][1] / tile), (poly[k][0] / tile, -poly[k][1] / tile), (poly[k + 1][0] / tile, -poly[k + 1][1] / tile))


# ------------------------------------------------------------------ architecture
def ashlar_block(sp, x0, x1, y0, y1, z0, z1, top='stone', bottom=False, tile=2.0):
    sp.box('ashlar', ((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2), (x1 - x0, y1 - y0, z1 - z0), tile=tile, top=top, bottom=bottom)


def broken_wall_run(sp, rng, axis, a0, a1, face, thick, heights, seg=1.4, base=0.0, plinth=True, cornice=True):
    """A wall run of ashlar columns, each its own height (a ragged, broken top).  axis 'x': runs along X at inner face y=face
    (extends to y=face+thick*sign), axis 'y': runs along Y at inner face x=face.  Returns the list of (centre, height) of each
    segment.  `heights(t)` gives the target height for the run parameter t in 0..1."""
    out = []
    n = max(1, int(round(abs(a1 - a0) / seg)))
    for i in range(n):
        t0, t1 = i / n, (i + 1) / n
        lo, hi = a0 + (a1 - a0) * t0, a0 + (a1 - a0) * t1
        h = max(.25, heights((t0 + t1) / 2) + rng.uniform(-.18, .18))
        sgn = 1 if face > 0 else -1
        f0, f1 = (face, face + sgn * thick) if sgn > 0 else (face + sgn * thick, face)
        if axis == 'x':
            ashlar_block(sp, min(lo, hi), max(lo, hi), f0, f1, base, h)
        else:
            ashlar_block(sp, f0, f1, min(lo, hi), max(lo, hi), base, h)
        out.append(((lo + hi) / 2, h))
    return out


def pilaster(sp, x, y, yaw, h, w=.7, proj=.35):
    """A wall pilaster whose face looks along +X (before yaw): plinth, shaft, capital with a corbel; origin at the wall face."""
    s = Soup()
    s.box('stone', (proj / 2 - .02, 0, .2), (proj + .12, w + .22, .4), tile=1.0, top='stone', bottom=False)
    s.box('ashlar', (proj / 2 - .04, 0, .4 + (h - .8) / 2), (proj, w, h - .8), tile=1.0, top='stone', bottom=False)
    s.box('stone', (proj / 2 - .02, 0, h - .3), (proj + .12, w + .2, .3), tile=1.0, top='stone', bottom=False)
    s.box('stone', (proj / 2 + .0, 0, h - .55), (proj + .02, w - .12, .2), tile=1.0, top='stone', bottom=False)
    return xform(s.soup(), T(x, y, 0) @ Rz(yaw))


def rib_arc(sp, c0, c1, rise, width=.5, depth=.55, n=7, broken=1.0, mat='stone'):
    """A vault rib springing from c0 toward c1 as a half-pointed arc with a rectangular profile; `broken` is the fraction of the
    span that exists (the rest has fallen).  Built as a chain of boxes turned along the arc (faceted, PS1)."""
    c0, c1 = np.array(c0, float), np.array(c1, float)
    pts = []
    m = max(2, int(n * broken))
    for i in range(m + 1):
        t = i / n
        p = c0 + (c1 - c0) * t
        p[2] += rise * math.sin(t * math.pi / 2)
        pts.append(p)
    for a, b in zip(pts, pts[1:]):
        d = b - a
        ln = np.linalg.norm(d)
        if ln < 1e-3:
            continue
        yaw = math.degrees(math.atan2(d[1], d[0]))
        pitch = math.degrees(math.asin(d[2] / ln))
        blk = Soup()
        blk.box(mat, (0, 0, 0), (ln * 1.04, width, depth), tile=1.0, top=mat, bottom=True)
        sp_ = xform(blk.soup(), T(*((a + b) / 2)) @ Rz(yaw) @ Ry(-pitch))
        sp.P.extend(map(tuple, sp_['P'])); sp.UV.extend(map(tuple, sp_['UV'])); sp.M.extend(sp_['mat'].tolist())
    return pts


def merge(sp, s):
    sp.P.extend(map(tuple, s['P'])); sp.UV.extend(map(tuple, s['UV'])); sp.M.extend(s['mat'].tolist())


# ------------------------------------------------------------------ props
def skull(rng, c, scale=1.0, yaw=0.0):
    sp = Soup()
    r = .085 * scale
    sp.lathe('bone', (0, 0, 0), [(.0, 0), (r * .55, .01), (r * .95, r * .55), (r, r * 1.2), (r * .8, r * 1.9), (r * .35, r * 2.25), (.0, r * 2.35)], 7, tile=.4, cap=(False, False))
    sp.box('bone', (r * .75, 0, r * .3), (r * .7, r * 1.1, r * .55), tile=.4, bottom=False)           # jaw
    sp.box('stone', (r * .92, -r * .42, r * 1.2), (r * .1, r * .4, r * .45), tile=.4, bottom=False)      # eye sockets (dark)
    sp.box('stone', (r * .92, r * .42, r * 1.2), (r * .1, r * .4, r * .45), tile=.4, bottom=False)
    return xform(sp.soup(), T(*c) @ Rz(yaw))


def bone_long(rng, c, length=.34, yaw=0.0, lean=0.0):
    sp = Soup()
    r = .022
    sp.lathe('bone', (0, 0, 0), [(r * 1.5, 0), (r * 1.4, .02), (r * .8, .06), (r * .8, length - .06), (r * 1.4, length - .02), (r * 1.5, length)], 5, tile=.4, cap=(True, True))
    return xform(sp.soup(), T(*c) @ Rz(yaw) @ Ry(90 + lean))


def rib_cage(rng, c, yaw=0.0):
    sp = Soup()
    for k in range(5):
        z = .05 + k * .045
        sp.lathe('bone', (0, 0, z), [(.14 - .012 * k, 0), (.14 - .012 * k, .018)], 7, cap=(False, False), tile=.4)
    sp.box('bone', (0, 0, .13), (.03, .03, .22), tile=.4, bottom=False)
    return xform(sp.soup(), T(*c) @ Rz(yaw))


def urn(rng, c, h=.7, r=.28, mat='stone', yaw=0.0):
    sp = Soup()
    prof = [(r * .45, 0), (r * .7, h * .06), (r, h * .38), (r * .9, h * .62), (r * .5, h * .8), (r * .55, h * .88), (r * .62, h * .94), (r * .5, h)]
    sp.lathe(mat, (0, 0, 0), prof, 8, tile=.8, cap=(True, False))
    return xform(sp.soup(), T(*c) @ Rz(yaw))


def candle_stub(rng, c, h=.14, melt=True):
    sp = Soup()
    sp.lathe('bone', (0, 0, 0), [(.05, 0), (.035, .02), (.03, h), (.012, h + .01)], 5, tile=.4, cap=(True, True))
    if melt:
        sp.lathe('bone', (0, 0, 0), [(.11, 0), (.07, .014), (.04, .03)], 6, tile=.4, cap=(False, True))
    return xform(sp.soup(), T(*c))


def crystal_cluster(rng, c, h=.5, n=4, yaw=0.0):
    sp = Soup()
    for k in range(n):
        a = rng.uniform(0, math.tau)
        rr = rng.uniform(0, .12) * (h / .5)
        hh = h * rng.uniform(.5, 1.0)
        r = .045 * (h / .5) * rng.uniform(.8, 1.2)
        x, y = rr * math.cos(a), rr * math.sin(a)
        sp.lathe('crystal', (x, y, 0), [(r * .01, 0), (r, .0), (r, hh * .7), (r * .01, hh)], 5, cap=(False, False), tile=.5)
    return xform(sp.soup(), T(*c) @ Rz(yaw))


def sarcophagus(rng, w=.95, l=2.2, h=.8, effigy=True, cracked=False):
    """Stone coffin: tapered chest, lipped lid with a ridge and a carved head end.  Origin at the floor centre, long axis X."""
    sp = Soup()
    sp.box('ashlar', (0, 0, .1), (l, w, .2), tile=1.0, top='stone', bottom=False)                    # plinth
    body = [(-l / 2 + .08, .2), (l / 2 - .08, .2), (l / 2 - .02, h * .66), (-l / 2 + .02, h * .66)]
    convex_prism(sp, 'ashlar', [(x, z) for x, z in body], -w / 2 + .05, w / 2 - .05, tile=1.0, mat_cap='stone')
    sp.box('stone', (0, 0, h * .66 + .04), (l + .06, w + .02, .09), tile=1.0, top='stone', bottom=False)       # lid slab
    lid = [(-l / 2 + .12, h * .66 + .08), (l / 2 - .12, h * .66 + .08), (l / 2 - .3, h), (-l / 2 + .3, h)]
    convex_prism(sp, 'stone', lid, -w * .32, w * .32, tile=1.0, mat_cap='stone')
    if effigy:
        sp.lathe('stone', (-l / 2 + .42, 0, h), [(.15, 0), (.15, .03), (.1, .1), (.0, .12)], 7, tile=.5, cap=(False, False))
        sp.box('stone', (.15, 0, h + .01), (.9, .26, .05), tile=.5, bottom=False)
    s = sp.soup()
    if cracked:
        s = xform(s, Rz(rng.uniform(-8, 8)))
    return s


def beaded_chain(sp, pts, bead=.04, mat='iron'):
    """A hanging chain as one continuous 4-sided tube whose radius alternates bead / half bead every ~8 cm (reads as links at game distance, a
    few hundred triangles, and decimates cleanly because it is a single connected surface)."""
    pts = [np.array(p, float) for p in pts]
    path, rad = [], []
    for a, b in zip(pts, pts[1:]):
        ln = np.linalg.norm(b - a)
        n = max(1, int(ln / .08))
        for k in range(n):
            path.append(tuple(a + (b - a) * k / n))
            rad.append(bead if (len(path) % 2) else bead * .5)
    path.append(tuple(pts[-1])); rad.append(bead * .5)
    sp.tube(mat, path, rad, 4, tile=.3, patch=(0, 0, 1, 1), cap=(True, True))


def chain_run(sp, pts, link=.07, mat='iron'):
    """A hanging or lying chain along the polyline pts: alternating flat and upright links (small boxes), plus a thin core tube."""
    pts = [np.array(p, float) for p in pts]
    for a, b in zip(pts, pts[1:]):
        d = b - a
        ln = np.linalg.norm(d)
        if ln < 1e-3:
            continue
        n = max(1, int(ln / (link * 1.4)))
        yaw = math.degrees(math.atan2(d[1], d[0]))
        pitch = math.degrees(math.asin(d[2] / ln))
        for k in range(n):
            p = a + d * (k + .5) / n
            blk = Soup()
            blk.box(mat, (0, 0, 0), (link * 1.5, link * (.8 if k % 2 == 0 else .35), link * (.35 if k % 2 == 0 else .8)), tile=.3, bottom=True)
            merge(sp, xform(blk.soup(), T(*p) @ Rz(yaw) @ Ry(-pitch)))


def column_drum(rng, c, r=.45, h=.7, yaw=0.0, lean=90.0):
    """A fallen column drum lying on its side."""
    sp = Soup()
    sp.lathe('stone', (0, 0, -h / 2), [(r, 0), (r, h)], 9, tile=1.0, cap=(True, True))
    return xform(sp.soup(), T(c[0], c[1], r * .95) @ Rz(yaw) @ Ry(lean))


def chest(rng, c, yaw=0.0):
    sp = Soup()
    sp.box('wood', (0, 0, .22), (.9, .55, .44), tile=.6, top='wood', bottom=False)
    convex_prism(sp, 'wood', [(-.45, .44), (.45, .44), (.4, .58), (-.4, .58)], -.275, .275, tile=.6, mat_cap='wood')
    sp.box('iron', (0, 0, .3), (.94, .07, .06), tile=.3, bottom=False)
    sp.box('iron', (.452, 0, .22), (.04, .12, .14), tile=.3, bottom=False)
    return xform(sp.soup(), T(*c) @ Rz(yaw))


def hooded_statue(rng, c, h=1.9, yaw=0.0, glow=True):
    """A robed funerary figure with a bowed hood, hands folded: lathe body, hood blob, plinth."""
    sp = Soup()
    sp.box('stone', (0, 0, .15), (.8, .8, .3), tile=1.0, top='stone', bottom=False)
    sp.lathe('stone', (0, 0, .3), [(.34, 0), (.30, h * .25), (.2, h * .55), (.17, h * .7), (.14, h * .78)], 8, tile=1.0, cap=(False, False))
    sp.lathe('stone', (0, 0, .3 + h * .74), [(.12, 0), (.17, .1), (.18, .22), (.08, .3), (.01, .32)], 8, tile=1.0, cap=(False, True))
    sp.box('stone', (.17, 0, .3 + h * .48), (.1, .26, .2), tile=1.0, bottom=False)          # folded hands
    sp.lathe('crystal' if glow else 'stone', (.19, 0, .3 + h * .52), [(.05, 0), (.04, .1), (.01, .14)], 5, cap=(False, True))   # the lantern shard they hold
    return xform(sp.soup(), T(*c) @ Rz(yaw))


def rubble_heap(sp, rng, c, r=.8, h=.5, n=6, mat='stone'):
    for k in range(n):
        a = rng.uniform(0, math.tau)
        rr = r * math.sqrt(rng.uniform(0, 1))
        rr_ = rng.uniform(.14, .34) * (h / .5 + .4)
        add_rock(sp, mat, rng, (c[0] + rr * math.cos(a), c[1] + rr * math.sin(a), rr_ * .28 * (1.2 - rr / max(r, .01))), rr_,
                 (1.0, rng.uniform(.7, 1.1), rng.uniform(.5, .8)), tile=.8, detail=0, floor=-.02)
    for k in range(max(1, n // 3)):                                    # a few dressed blocks among the rubble
        blk = Soup()
        w = rng.uniform(.3, .6)
        blk.box('ashlar', (0, 0, 0), (w, w * rng.uniform(.6, 1.0), w * .55), tile=1.0, top='stone', bottom=False)
        a = rng.uniform(0, math.tau)
        rr = r * rng.uniform(0, .8)
        merge(sp, xform(blk.soup(), T(c[0] + rr * math.cos(a), c[1] + rr * math.sin(a), w * .25) @ Rz(rng.uniform(0, 360)) @ Rx(rng.uniform(-14, 14))))
