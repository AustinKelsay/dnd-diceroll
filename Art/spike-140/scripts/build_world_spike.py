"""#140 spike: Drowned Vigil rebuilt around a true-metric footprint.

Run from the repository root:  blender -b --python Art/spike-140/scripts/build_world_spike.py

Reads Art/dark-fantasy-sources/r001/vigil.blend (never modifies it) and writes to Art/spike-140/exports/:
  SM_World_VigilSpike.glb            continuous ground + kept architecture + rune inlay (one 2048 atlas)
  SM_World_VigilSpike_Occluders.glb  near / camera-occluding architecture, same atlas, for dither fading
  T_World_VigilSpike_Atlas.png       the 2048x2048 atlas
  world_metrics.json                 triangle counts and spacing statistics

Units: source scene has 2 units per cell (r001 SCALE 0.55 -> 110 cm cells).  Here SCALE = 0.75, so 1 cell = 2 units
= 150 cm and the 18 x 12 cell footprint (36 x 24 source units) is 27 m x 18 m centred on the origin.  Z up, +X
forward, metres.  COLOR_0 is RGBA: RGB is baked light (linear), A is the glow mask (1 = flames, runes, glass).

Differences from r001 build_worlds.py:
 * architecture is kept; only triangles inside the footprint are cleared.  Pieces that physically occlude the
   candidate camera rays (or rise near the board) are exported as a second mesh instead of being deleted.
 * the per-cell floor is replaced by one irregular Voronoi paving (stones 50-75 cm across, +-3 cm height, not cell
   aligned) over a soil/moss base with moss transitions.
 * vertex spacing near the board is <= 75 cm everywhere (architecture is subdivided), light = r001's baked light
   resampled + a new ray-cast ambient-occlusion term.
"""
import json
import math
import os
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..', '..'))
OUT = os.path.join(ROOT, 'Art', 'spike-140', 'exports')
R001 = os.path.join(ROOT, 'Art', 'dark-fantasy-sources', 'r001')
os.environ.setdefault('THEME_LOCAL', tempfile.mkdtemp(prefix='spike140_'))
sys.path.insert(0, os.path.join(R001, 'scripts'))

import bpy
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

import glass
import tlib as L

SCALE = .75                      # source unit -> metres (2 units = 1 cell = 1.5 m)
FX, FY = 18.0, 12.0              # footprint half extents, source units (18 x 12 cells)
MX, MY = FX + 1.0, FY + 1.0      # ground margin (half a cell, as r001)
NEAR = 6.0                       # r001: "about 3 cells" around the footprint
ATLAS = 2048
S_TILE = 1.3334                  # source units covered by one 256 px procedural tile (= 1.0 m)
GLOW_MATS = ('lancet', 'rose', 'flame', 'rune')
rng = np.random.default_rng(1401)
STATS = {}


# ------------------------------------------------------------------ small helpers
def level(y):
    return .4 if 6 <= y < 8 else .8 if 8 <= y < 10 else 1.2 if y >= 10 else 0.


def outer_level(y):
    yc = 2 * math.floor(y / 2) + 1
    return level(yc)


def tri_normals(P):
    n = np.cross(P[:, 1] - P[:, 0], P[:, 2] - P[:, 0])
    return n / np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-12)


def rect_dist(xy):
    """Distance from XY points (...,2) to the footprint rectangle, source units."""
    dx = np.maximum(0, np.abs(xy[..., 0]) - FX)
    dy = np.maximum(0, np.abs(xy[..., 1]) - FY)
    return np.hypot(dx, dy)


def sampler(points, colours):
    def f(xy):
        d = np.linalg.norm(points[None, :, :2] - np.asarray(xy)[:, None, :], axis=2)
        idx = np.argsort(d, axis=1)[:, :6]
        w = 1 / (np.take_along_axis(d, idx, 1) + .5)
        return (colours[idx] * w[..., None]).sum(1) / w.sum(1, keepdims=True)
    return f


def smoothstep(a, b, x):
    t = np.clip((x - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)


_LAT = {}


def vnoise(x, y, freq, seed):
    if seed not in _LAT:
        _LAT[seed] = np.random.default_rng(seed).random((64, 64))
    g = _LAT[seed]
    xs, ys = np.asarray(x) * freq, np.asarray(y) * freq
    x0, y0 = np.floor(xs).astype(int), np.floor(ys).astype(int)
    fx, fy = xs - x0, ys - y0
    fx, fy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
    a, b = g[x0 % 64, y0 % 64], g[(x0 + 1) % 64, y0 % 64]
    c, d = g[x0 % 64, (y0 + 1) % 64], g[(x0 + 1) % 64, (y0 + 1) % 64]
    return (a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy


def fbm(x, y, seed=11):
    return .55 * vnoise(x, y, .11, seed) + .30 * vnoise(x, y, .29, seed + 1) + .15 * vnoise(x, y, .71, seed + 2)


def tri_soup(tris, uvs, cols, mat):
    """tris: list of 3 points; returns a soup dict."""
    P = np.array(tris, float).reshape(-1, 3, 3)
    return dict(P=P, UV=np.array(uvs, float).reshape(-1, 3, 2), C=np.array(cols, float).reshape(-1, 3, 3),
                mat=np.array([mat] * len(P), dtype=object))


# ------------------------------------------------------------------ glass cut-outs (copied from r001 build_tabletop.py;
# that module cannot be imported without r001's .local textures)
def alpha_at(tex, uv):
    h, w = tex.shape[:2]
    c = (np.mod(uv[..., 0], 1) * w).astype(int).clip(0, w - 1)
    r = (np.mod(uv[..., 1], 1) * h).astype(int).clip(0, h - 1)
    return tex[r, c, 3]


def refine_cutout(s, tex, depth=2, mat='parch'):
    keep_tris = []
    bary = np.array([[1, 0, 0], [0, 1, 0], [0, 0, 1], [1 / 3, 1 / 3, 1 / 3], [.5, .5, 0], [0, .5, .5], [.5, 0, .5],
                     [.6, .2, .2], [.2, .6, .2], [.2, .2, .6]])
    bary[:3] = [[.9, .05, .05], [.05, .9, .05], [.05, .05, .9]]

    def rec(T, d):
        uv = bary @ T[:, 3:5]
        a = alpha_at(tex, uv) >= .5
        if a.all():
            keep_tris.append(T)
            return
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
    return L._unpack(keep_tris, s['C'] is not None, [mat] * len(keep_tris))


# ------------------------------------------------------------------ occlusion test (r001 ray method, soup based)
def make_bvh(P):
    verts = [Vector(v) for v in P.reshape(-1, 3)]
    return BVHTree.FromPolygons(verts, [(3 * i, 3 * i + 1, 3 * i + 2) for i in range(len(P))], all_triangles=True)


def cameras():
    out = []
    for dist_m in (18, 25, 35):
        for pitch in (35, 45, 60):
            D = dist_m / SCALE
            for yaw in range(0, 360, 45):
                r, p = math.radians(yaw), math.radians(pitch)
                out.append((math.cos(r) * math.cos(p) * D, math.sin(r) * math.cos(p) * D, math.sin(p) * D))
    return out


def occluding(P, comp, protect):
    """Components hit (front faces only) before any footprint target from any candidate camera."""
    nrm = tri_normals(P)
    bvh = make_bvh(P)
    tx = np.arange(-FX + .5, FX, 1.0)
    ty = np.arange(-FY + .5, FY, 1.0)
    targets = [Vector((x, y, .05)) for x in tx for y in ty]
    hit = {}
    for eye in cameras():
        e = Vector(eye)
        for tv in targets:
            d = tv - e
            dist = d.length
            d.normalize()
            o = e.copy()
            remaining = dist
            for _ in range(40):
                loc, _n, idx, h = bvh.ray_cast(o, d, remaining - .03)
                if loc is None:
                    break
                if nrm[idx] @ np.array(d) >= 0:      # back face: culled in game, look through it
                    o = loc + d * 1e-3
                    remaining -= h + 1e-3
                    continue
                c = comp[idx]
                if c not in protect:
                    hit[c] = hit.get(c, 0) + 1
                break
    return hit


# ------------------------------------------------------------------ subdivision and ambient occlusion
def subdivide(s, maxedge, region):
    """Split the longest edge of every triangle (centroid passing region(centroid_xy)) until all edges <= maxedge."""
    for _ in range(12):
        P = s['P']
        e = np.stack([np.linalg.norm(P[:, (i + 1) % 3] - P[:, i], axis=1) for i in range(3)], 1)
        longest = e.argmax(1)
        go = (e.max(1) > maxedge) & region(P.mean(1)[:, :2])
        if not go.any():
            break
        keep = L.sub(s, ~go)
        idx = np.nonzero(go)[0]
        order = (longest[idx][:, None] + np.arange(3)[None]) % 3          # A,B,C with the longest edge A->B
        new = []
        for key in ('P', 'UV', 'C'):
            arr = s[key][idx]
            if arr is None:
                new.append(None)
                continue
            A, B, C = (np.take_along_axis(arr, order[:, k, None, None], 1)[:, 0] for k in range(3))
            M = (A + B) / 2
            new.append((np.stack([A, M, C], 1), np.stack([M, B, C], 1)))
        mats = s['mat'][idx]
        parts = []
        for h in (0, 1):
            parts.append(dict(P=new[0][h], UV=new[1][h], C=None if s['C'] is None else new[2][h], mat=mats.copy()))
        s = L.cat(keep, *parts)
    return s


def hemisphere(n):
    i = (np.arange(n) + .5) / n
    r = np.sqrt(i)
    th = np.arange(n) * 2.39996
    return np.stack([r * np.cos(th), r * np.sin(th), np.sqrt(1 - i)], 1)


def ambient(bvh, pts, nrm, dist=2.4, n=10):
    """Cosine-weighted hemisphere visibility, 0 (closed) .. 1 (open)."""
    H = hemisphere(n)
    out = np.ones(len(pts))
    for k, (p, nn) in enumerate(zip(pts, nrm)):
        nn = np.array(nn)
        ref = np.array((0, 0, 1.0)) if abs(nn[2]) < .9 else np.array((1.0, 0, 0))
        t = np.cross(nn, ref)
        t /= np.linalg.norm(t)
        b = np.cross(nn, t)
        a = rng.uniform(0, 2 * math.pi)
        ca, sa = math.cos(a), math.sin(a)
        o = Vector(p + nn * .03)
        hits = 0
        for h in H:
            d = t * (h[0] * ca - h[1] * sa) + b * (h[0] * sa + h[1] * ca) + nn * h[2]
            if bvh.ray_cast(o, Vector(d), dist)[0] is not None:
                hits += 1
        out[k] = 1 - hits / n
    return out


def bake_ao(s, bvh, strength=.6):
    """Multiply per-corner light by an AO term evaluated once per unique (position, normal)."""
    P = s['P']
    N = tri_normals(P)
    key = np.concatenate([np.round(P.reshape(-1, 3) * 400), np.round(np.repeat(N, 3, 0) * 3)], 1).astype(np.int64)
    uniq, first, inv = np.unique(key, axis=0, return_index=True, return_inverse=True)
    inv = inv.ravel()
    pts = P.reshape(-1, 3)[first]
    nrm = np.repeat(N, 3, 0)[first]
    ao = ambient(bvh, pts, nrm)
    f = (1 - strength) + strength * ao
    s = dict(s)
    s['C'] = s['C'] * f[inv].reshape(-1, 3, 1)
    return s, len(first)


# ------------------------------------------------------------------ procedural textures (256 px tiles, 1 m)
def tiles_procedural():
    r = np.random.default_rng(77)
    n = 256
    yy, xx = np.mgrid[0:n, 0:n]

    def noise(cells, seed):
        a = np.random.default_rng(seed).random((cells + 1, cells + 1))
        xs, ys = xx / n * cells, yy / n * cells
        x0, y0 = xs.astype(int), ys.astype(int)
        fx, fy = xs - x0, ys - y0
        fx, fy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
        return (a[y0, x0] * (1 - fx) + a[y0, x0 + 1] * fx) * (1 - fy) + (a[y0 + 1, x0] * (1 - fx) + a[y0 + 1, x0 + 1] * fx) * fy

    big, mid, fine = noise(4, 1), noise(12, 2), noise(32, 3)
    out = {}

    def pave(seed, base, crack):
        b = np.array(base)
        n1, n2 = noise(5, seed), noise(14, seed + 9)
        t = np.ones((n, n, 4), np.float32)
        t[..., :3] = b * (.78 + .30 * n1[..., None] + .16 * n2[..., None] - .08)
        # a few smooth worn hollows and two or three crack lines (no per-pixel grain)
        t[..., :3] *= (1 - .22 * smoothstep(.62, .85, noise(3, seed + 3)))[..., None]
        for _ in range(crack):
            x0, y0 = r.integers(40, 216, 2)
            ang = r.uniform(0, math.tau)
            for k in range(r.integers(30, 70)):
                x = int(x0 + math.cos(ang + .3 * math.sin(k * .15)) * k)
                y = int(y0 + math.sin(ang + .3 * math.sin(k * .15)) * k)
                if 2 <= x < n - 2 and 2 <= y < n - 2:
                    t[y, x, :3] *= .45
        return t

    out['paveA'] = pave(21, (.34, .33, .36), 2)
    out['paveB'] = pave(31, (.40, .37, .36), 3)
    out['paveC'] = pave(41, (.30, .31, .35), 1)
    t = pave(51, (.33, .34, .34), 2)
    moss = smoothstep(.45, .7, noise(6, 52) * .6 + noise(18, 53) * .4)
    t[..., :3] = t[..., :3] * (1 - moss[..., None]) + np.array((.20, .36, .13)) * (.7 + .5 * mid)[..., None] * moss[..., None]
    out['paveM'] = t
    t = np.ones((n, n, 4), np.float32)
    t[..., :3] = np.array((.17, .33, .12)) * (.65 + .55 * big[..., None]) * (.9 + .2 * mid[..., None])
    out['moss'] = t
    t = np.ones((n, n, 4), np.float32)
    t[..., :3] = np.array((.30, .22, .15)) * (.7 + .5 * big[..., None]) * (.9 + .2 * mid[..., None])
    out['soil'] = t
    # rune band: dark teal metal with bright angular glyph strokes
    t = np.ones((n, n, 4), np.float32)
    t[..., :3] = np.array((.06, .20, .22))
    t[:12] = t[-12:] = (.18, .62, .6, 1)
    t[:, ::64, :3] = (.18, .62, .6)
    for gx in range(0, n, 16):
        for _ in range(3):
            a, b = r.integers(0, 3, 2), r.integers(0, 8, 2)
            x0 = gx + 3 + a[0] * 4
            y0 = 40 + b[0] * 22
            if r.random() < .5:
                t[y0:y0 + 90, x0:x0 + 2, :3] = (.30, .98, .92)
            else:
                t[y0:y0 + 2, gx + 2:gx + 13, :3] = (.30, .98, .92)
                t[y0:y0 + 40, gx + 2:gx + 4, :3] = (.30, .98, .92)
    out['rune'] = t
    return out


# ------------------------------------------------------------------ paving
def clip_poly(poly, n, d):
    """Keep points with dot(p, n) <= d (2D Sutherland-Hodgman)."""
    out = []
    m = len(poly)
    for i in range(m):
        a, b = poly[i], poly[(i + 1) % m]
        da, db = a @ n - d, b @ n - d
        if da <= 0:
            out.append(a)
        if (da <= 0) != (db <= 0):
            out.append(a + (b - a) * (da / (da - db)))
    return out


def voronoi_cells(gap=.075):
    sites = []
    rows = np.arange(-MY - .9, MY + .9 + 1e-6, .88)
    for ri, y in enumerate(rows):
        off = (ri % 2) * .47 + rng.uniform(-.12, .12)
        for x in np.arange(-MX - 1.0 + off, MX + 1.0, .96):
            sites.append((x + rng.uniform(-.22, .22), y + rng.uniform(-.2, .2)))
    sites = np.array(sites)
    cells = []
    for i, p in enumerate(sites):
        d = np.linalg.norm(sites - p, axis=1)
        nb = np.nonzero((d > 0) & (d < 2.6))[0]
        poly = [p + np.array(v) for v in ((-1.6, -1.6), (1.6, -1.6), (1.6, 1.6), (-1.6, 1.6))]
        for j in nb[np.argsort(d[nb])]:
            n = (sites[j] - p) / d[j]
            poly = clip_poly([q - p for q in poly], n, d[j] / 2 - gap / 2)
            poly = [q + p for q in poly]
            if len(poly) < 3:
                break
        if len(poly) < 3:
            continue
        for n, d0 in ((np.array((1., 0)), MX), (np.array((-1., 0)), MX), (np.array((0, 1.)), MY), (np.array((0, -1.)), MY)):
            poly = clip_poly(poly, n, d0)
            if len(poly) < 3:
                break
        if len(poly) < 3:
            continue
        poly = np.array(poly)
        c = poly.mean(0)
        area = .5 * abs(np.dot(poly[:, 0], np.roll(poly[:, 1], -1)) - np.dot(poly[:, 1], np.roll(poly[:, 0], -1)))
        if area < .12:
            continue
        # order counter-clockwise
        ang = np.arctan2(poly[:, 1] - c[1], poly[:, 0] - c[0])
        cells.append((c, poly[np.argsort(ang)], area))
    return cells


BASE_Z = -.07        # grout / soil level (source units, 5 cm)


def build_paving(light_at, bvh):
    cells = voronoi_cells()
    parts = []
    diam, ranges = [], []
    heights = []
    stone_c = []
    for c, poly, area in cells:
        m_field = fbm(c[0], c[1])
        edge = 1 - np.clip(min(MX - abs(c[0]), MY - abs(c[1])) / 3.0, 0, 1)
        moss = float(smoothstep(.53, .68, m_field + .12 * edge))
        soilp = float(smoothstep(.72, .84, fbm(c[0] + 40, c[1] - 17, 31)))
        if rng.random() < soilp * .6:
            continue                                       # a bare soil patch instead of a stone
        stone_c.append(c)
        diam.append(math.sqrt(area))
        w = np.ptp(poly, axis=0)
        ranges.append(w)
        h = float(np.clip(rng.normal(0, .016), -.036, .036))
        g = rng.uniform(-.028, .028, 2)
        ztop = lambda q: np.clip(h + g[0] * (q[0] - c[0]) + g[1] * (q[1] - c[1]), -.039, .039)
        heights.append(h)
        k = rng.random()
        mat = 'paveM' if moss > .45 else ('paveA' if k < .4 else 'paveB' if k < .75 else 'paveC')
        bright = rng.uniform(.88, 1.1)
        off = rng.uniform(-.05, .05, 2)

        def uv(q, z=0.0, c=c, off=off):
            return (.5 + (q[0] - c[0]) / S_TILE + off[0], .5 + (q[1] - c[1]) / S_TILE + off[1] + z * .6)

        n = len(poly)
        dist = np.linalg.norm(poly - c, axis=1)
        inner = c + (poly - c) * np.maximum(.55, 1 - .045 / np.maximum(dist, 1e-3))[:, None]
        zin = np.array([ztop(q) for q in inner])
        zout = np.array([ztop(q) for q in poly]) - .03
        ctop = np.array((c[0], c[1], ztop(c) + .008))
        L_in = light_at(np.array([c]))[0]
        L_poly = light_at(poly)
        tint = lambda m: 1 + m * (np.array((.62, .88, .5)) - 1) * .8
        t_top = tint(moss * .8) * bright
        t_edge = tint(min(1, moss * 1.0 + .3)) * bright * .82
        tris, uvs, cols = [], [], []
        for i in range(n):
            j = (i + 1) % n
            a, b = (inner[i][0], inner[i][1], zin[i]), (inner[j][0], inner[j][1], zin[j])
            tris.append((tuple(ctop), a, b))
            uvs.append((uv(c), uv(inner[i]), uv(inner[j])))
            cols.append((L_in * t_top, L_poly[i] * t_top * .95, L_poly[j] * t_top * .95))
            o0, o1 = (poly[i][0], poly[i][1], zout[i]), (poly[j][0], poly[j][1], zout[j])
            # bevel: inner ring -> outer ring (normal up/outwards)
            tris += [(a, o0, o1), (a, o1, b)]
            uvs += [(uv(inner[i]), uv(poly[i]), uv(poly[j])), (uv(inner[i]), uv(poly[j]), uv(inner[j]))]
            ce0, ce1 = L_poly[i] * t_edge, L_poly[j] * t_edge
            ci0, ci1 = L_poly[i] * t_top * .95, L_poly[j] * t_top * .95
            cols += [(ci0, ce0, ce1), (ci0, ce1, ci1)]
            # wall down to the soil level
            b0, b1 = (poly[i][0], poly[i][1], BASE_Z), (poly[j][0], poly[j][1], BASE_Z)
            tris += [(o0, b0, b1), (o0, b1, o1)]
            uvs += [(uv(poly[i], zout[i]), uv(poly[i], BASE_Z), uv(poly[j], BASE_Z)), (uv(poly[i], zout[i]), uv(poly[j], BASE_Z), uv(poly[j], zout[j]))]
            cw = L_poly[i] * .6
            cols += [(ce0, cw, cw), (ce0, cw, ce1)]
        mats = [mat] * len(tris)
        s = tri_soup(tris, uvs, cols, mat)
        s['UV'] = np.clip(s['UV'], .02, .98)
        # orient upwards: flip any top/bevel triangle that faces down
        nz = tri_normals(s['P'])[:, 2]
        top = np.arange(len(tris)) % 6 < 3
        flip = top & (nz < 0)
        s['P'][flip] = s['P'][flip][:, [0, 2, 1]]
        s['UV'][flip] = s['UV'][flip][:, [0, 2, 1]]
        s['C'][flip] = s['C'][flip][:, [0, 2, 1]]
        wallflip = ~top & (np.einsum('ij,ij->i', tri_normals(s['P'])[:, :2], (s['P'].mean(1)[:, :2] - c)) < 0)
        s['P'][wallflip] = s['P'][wallflip][:, [0, 2, 1]]
        s['UV'][wallflip] = s['UV'][wallflip][:, [0, 2, 1]]
        s['C'][wallflip] = s['C'][wallflip][:, [0, 2, 1]]
        parts.append(s)
    stones = L.cat(*parts)
    # AO at stone tops (one sample per stone -> applied to the whole stone), grout darkening already in colours
    pts = np.array([(c[0], c[1], 0.0) for c in stone_c])
    ao = ambient(bvh, pts, np.tile((0, 0, 1.0), (len(pts), 1)), n=10)
    per_tri = np.concatenate([np.full(len(p['P']), a) for p, a in zip(parts, ao)])
    stones['C'] = stones['C'] * (.45 + .55 * per_tri)[:, None, None]
    diam = np.array(diam)
    ranges = np.array(ranges)
    STATS['stones'] = len(stone_c)
    STATS['stone_equiv_diameter_cm'] = [round(float(np.percentile(diam, q)) * SCALE * 100, 1) for q in (5, 50, 95)]
    STATS['stone_bbox_width_cm_p5_p50_p95'] = [round(float(np.percentile(ranges.max(1), q)) * SCALE * 100, 1) for q in (5, 50, 95)]
    STATS['stone_top_height_range_cm'] = [round(float(min(heights)) * SCALE * 100, 2), round(float(max(heights)) * SCALE * 100, 2)]
    STATS['stone_tris'] = int(len(stones['P']))
    # base plane in 1-unit quads (75 cm vertex spacing): soil / moss by the same field
    tris, uvs, cols, mats = [], [], [], []
    for x in np.arange(-MX, MX - 1e-6, 1.0):
        for y in np.arange(-MY, MY - 1e-6, 1.0):
            q = [(x, y), (x + 1, y), (x + 1, y + 1), (x, y + 1)]
            m_field = np.array([fbm(*p) for p in q])
            edge = np.array([1 - np.clip(min(MX - abs(p[0]), MY - abs(p[1])) / 3.0, 0, 1) for p in q])
            moss = smoothstep(.50, .66, m_field + .12 * edge)
            mat = 'moss' if moss.mean() > .5 else 'soil'
            Lq = light_at(np.array(q))
            col = [Lq[i] * (1.0 * (1 + moss[i] * (np.array((.7, 1.1, .6)) - 1))) for i in range(4)]
            o = rng.uniform(-.05, .05, 2)
            cx, cy = x + .5, y + .5
            uvq = [(.5 + (p[0] - cx) / S_TILE + o[0], .5 + (p[1] - cy) / S_TILE + o[1]) for p in q]
            pz = [(p[0], p[1], BASE_Z) for p in q]
            for tri in ((0, 1, 2), (0, 2, 3)):
                tris.append([pz[i] for i in tri])
                uvs.append([uvq[i] for i in tri])
                cols.append([col[i] for i in tri])
                mats.append(mat)
    base = tri_soup(tris, uvs, cols, 'soil')
    base['mat'] = np.array(mats, dtype=object)
    base['UV'] = np.clip(base['UV'], .02, .98)
    STATS['base_tris'] = int(len(base['P']))
    return L.cat(stones, base)


def rune_ring(light_at, cx=-5.0, cy=3.0, r0=2.7, r1=3.3, seg=48, z=.065):
    tris, uvs, cols = [], [], []
    for i in range(seg):
        a0, a1 = 2 * math.pi * i / seg, 2 * math.pi * (i + 1) / seg
        p = [(cx + r0 * math.cos(a0), cy + r0 * math.sin(a0), z), (cx + r1 * math.cos(a0), cy + r1 * math.sin(a0), z),
             (cx + r1 * math.cos(a1), cy + r1 * math.sin(a1), z), (cx + r0 * math.cos(a1), cy + r0 * math.sin(a1), z)]
        u0, u1 = i / seg, (i + 1) / seg
        uv = [(u0, 0), (u0, 1), (u1, 1), (u1, 0)]
        for tri in ((0, 1, 2), (0, 2, 3)):
            tris.append([p[k] for k in tri])
            uvs.append([uv[k] for k in tri])
            cols.append([(1, 1, 1)] * 3)
    s = tri_soup(tris, uvs, cols, 'rune')
    s['UV'] = np.clip(s['UV'], .01, .99)
    nz = tri_normals(s['P'])[:, 2]
    f = nz < 0
    s['P'][f] = s['P'][f][:, [0, 2, 1]]
    s['UV'][f] = s['UV'][f][:, [0, 2, 1]]
    return s


# ------------------------------------------------------------------ outer risers (as r001, onto the continuous ground)
def quad(p, uv, c, mat):
    a, b, cc, d = range(4)
    P = np.array([[p[a], p[b], p[cc]], [p[a], p[cc], p[d]]], float)
    UV = np.array([[uv[a], uv[b], uv[cc]], [uv[a], uv[cc], uv[d]]], float)
    C = np.array([[c[a], c[b], c[cc]], [c[a], c[cc], c[d]]], float)
    return dict(P=P, UV=UV, C=C, mat=np.array([mat, mat], dtype=object))


def risers(riser_mat, light_at):
    parts = []

    def riser(a, b, h, inward):
        if h <= 0:
            return
        p = [(a[0], a[1], BASE_Z), (b[0], b[1], BASE_Z), (b[0], b[1], h), (a[0], a[1], h)]
        n = np.cross(np.subtract(p[1], p[0]), np.subtract(p[3], p[0]))
        if np.dot(n[:2], inward) < 0:
            p = [p[1], p[0], p[3], p[2]]
        uv = [((q[0] + q[1]) / 4, q[2] * .1) for q in p]
        c = list(light_at(np.array([[q[0], q[1]] for q in p])) * .8)
        parts.append(quad(p, uv, c, riser_mat))
    for x0 in np.arange(-MX, MX - 1e-6, 1.0):
        riser((x0, MY), (x0 + 1, MY), outer_level(MY + .5), (0, -1))
        riser((x0, -MY), (x0 + 1, -MY), outer_level(-MY - .5), (0, 1))
    for y0 in np.arange(-MY, MY - 1e-6, 1.0):
        h = outer_level(y0 + .5)
        riser((MX, y0), (MX, y0 + 1), h, (-1, 0))
        riser((-MX, y0), (-MX, y0 + 1), h, (1, 0))
    return L.cat(*parts)


# ------------------------------------------------------------------ object + export with a glow-mask alpha
def make_world_object(name, s, img):
    s = L.drop_degenerate(s)
    n = len(s['P'])
    P = s['P'].reshape(-1, 3)
    key = np.round(P * 1e5).astype(np.int64)
    uniq, idx, inv = np.unique(key, axis=0, return_index=True, return_inverse=True)
    inv = inv.ravel()
    faces = inv.reshape(n, 3)
    ok = (faces[:, 0] != faces[:, 1]) & (faces[:, 1] != faces[:, 2]) & (faces[:, 0] != faces[:, 2])
    if not ok.all():
        s = L.sub(s, ok)
        faces = faces[ok]
        n = len(faces)
    me = bpy.data.meshes.new(name)
    me.from_pydata(P[idx].tolist(), [], faces.tolist())
    me.update()
    if len(me.polygons) != n:
        raise RuntimeError(f'{name}: face count changed {n} -> {len(me.polygons)}')
    uvl = me.uv_layers.new(name='UVMap')
    uvl.data.foreach_set('uv', s['UV'].reshape(-1).astype(np.float32))
    ca = me.color_attributes.new('Col', 'FLOAT_COLOR', 'CORNER')
    rgb = L.srgb2lin(np.clip(s['C'], 0, 1)).reshape(-1, 3)
    glow = np.repeat(np.isin(s['mat'], GLOW_MATS).astype(np.float32), 3)
    c4 = np.concatenate([rgb, glow[:, None]], 1).astype(np.float32)
    ca.data.foreach_set('color', c4.ravel())
    me.color_attributes.active_color = ca
    me.color_attributes.render_color_index = 0
    me.shade_flat()
    mat = bpy.data.materials.new('M_World_VigilSpike')
    nt = mat.node_tree
    bsdf = nt.nodes.get('Principled BSDF')
    bsdf.inputs['Roughness'].default_value = 1.0
    bsdf.inputs['Metallic'].default_value = 0.0
    tn = nt.nodes.new('ShaderNodeTexImage')
    tn.image = img
    tn.interpolation = 'Closest'
    nt.links.new(tn.outputs['Color'], bsdf.inputs['Base Color'])
    mat.use_backface_culling = True
    me.materials.append(mat)
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    return ob, int(glow.reshape(-1, 3)[:, 0].sum() / 3)


def export_glb(ob):
    for o in bpy.context.scene.objects:
        o.select_set(False)
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    path = os.path.join(OUT, ob.name + '.glb')
    bpy.ops.export_scene.gltf(
        filepath=path, export_format='GLB', use_selection=True, export_yup=True, export_apply=True,
        export_texcoords=True, export_normals=True, export_tangents=False, export_materials='EXPORT',
        export_image_format='AUTO', export_vertex_color='ACTIVE', export_all_vertex_colors=False,
        export_active_vertex_color_when_no_material=True, export_animations=False, export_skins=False,
        export_morph=False, export_cameras=False, export_lights=False)
    return path


# ------------------------------------------------------------------ main
def main():
    os.makedirs(OUT, exist_ok=True)
    bpy.ops.wm.open_mainfile(filepath=os.path.join(R001, 'vigil.blend'))
    s, texs = L.soup_from_object(bpy.data.objects['world'], light=L.bake_light(1.05, .58))
    print('source tris', len(s['P']))
    extra = []
    for nm in ('dynamic_snapshot_flame', 'dynamic_snapshot_iron', 'dynamic_snapshot_banner'):
        o = bpy.data.objects[nm]
        so, to = L.soup_from_object(o, light=L.bake_light(1.05, .58))
        texs.update(to)
        extra.append(so)
    nrm = tri_normals(s['P'])
    ground_mats = ['floor', 'moss']
    flat = np.isin(s['mat'], ground_mats) & (nrm[:, 2] > .99)
    gnd = flat & np.all(np.abs(s['P'][..., 0]) < MX + 3, 1) & np.all(np.abs(s['P'][..., 1]) < MY + 3, 1)
    gp, gc = s['P'][gnd].reshape(-1, 3), s['C'][gnd].reshape(-1, 3).copy()
    # r001's bake contains the shadows of obstacles that are cleared from the footprint; lift those so the new
    # ground is not stained by shadows of nothing (wall/prop proximity is handled by the new AO term)
    lum = gc @ np.array((.3, .5, .2))
    floor_l = .75 * np.median(lum)
    gc = gc * np.maximum(1.0, floor_l / np.maximum(lum, 1e-3))[:, None]
    light_at = sampler(gp, gc)
    for so in extra:
        if so['C'] is None:
            so['C'] = np.repeat(light_at(so['P'].mean(1)[:, :2])[:, None, :], 3, 1) * .9
    s = L.cat(s, *extra)
    if 'flame' in texs:
        texs['flame'] = L.swatch((1.0, .62, .25))
    # r001: align ground texture to world units on flat ground that survives outside the margin
    nrm = tri_normals(s['P'])
    flat = np.isin(s['mat'], ground_mats) & (nrm[:, 2] > .99)
    s['UV'][flat] = s['P'][flat][..., :2] / 8
    riser_mat = 'sstone'
    P = s['P']
    vert = np.abs(nrm[:, 2]) < .01
    ymid = P[..., 1].mean(1)
    on_edge = np.abs(ymid - np.round(ymid / 2) * 2) < 1e-3
    riser_src = (s['mat'] == riser_mat) & vert & on_edge & (P[..., 2].min(1) > -.45) & (P[..., 2].max(1) < 1.25) & (np.ptp(P[..., 0], 1) <= 2.001)
    is_ground = flat | riser_src
    comp = np.full(len(P), -1)
    comp[~is_ground] = L.components(L.sub(s, ~is_ground))
    ncomp = comp.max() + 1
    print('components', ncomp, 'ground tris', int(is_ground.sum()))

    # ---- 1. clear only what physically intrudes into the playable footprint
    cen = P.mean(1)
    inside = (np.abs(cen[:, 0]) < FX + .5) & (np.abs(cen[:, 1]) < FY + .5) & (comp >= 0) & (cen[:, 2] > -.2)
    frac = np.bincount(comp[inside], minlength=ncomp + 1)[:ncomp] / np.maximum(np.bincount(comp[comp >= 0], minlength=ncomp), 1)
    drop_whole = np.zeros(ncomp + 1, bool)
    drop_whole[:ncomp] = frac > .4
    drop = inside | drop_whole[comp] & (comp >= 0)
    STATS['cleared_intruding_tris'] = int(drop.sum())
    STATS['cleared_components_whole'] = int(drop_whole.sum())
    # ground inside footprint+margin is replaced by the new continuous ground
    g = comp == -1
    ground_in = g & np.all(np.abs(P[..., 0]) <= MX, 1) & np.all(np.abs(P[..., 1]) <= MY, 1)
    keep = ~drop
    s2 = L.sub(s, keep)
    comp = comp[keep]
    g = comp == -1
    gs = L.clip_rect(L.sub(s2, g), -MX, MX, -MY, MY)
    rest = L.sub(s2, ~g)
    s = L.cat(gs, rest)
    comp = np.concatenate([np.full(len(gs['P']), -1), comp[~g]])
    P = s['P']
    # ---- 2. which kept pieces occlude or rise near the board?
    hit = occluding(P, comp, {-1})
    print('ray-hit components', len(hit), sum(hit.values()), 'rays')
    cmin = np.full((ncomp + 1, 3), np.inf)
    cmax = np.full((ncomp + 1, 3), -np.inf)
    np.minimum.at(cmin, comp, P.min(1))
    np.maximum.at(cmax, comp, P.max(1))
    dnear_c = np.hypot(np.maximum(0, np.maximum(-FX - cmax[:, 0], cmin[:, 0] - FX)), np.maximum(0, np.maximum(-FY - cmax[:, 1], cmin[:, 1] - FY)))
    tall = (dnear_c < NEAR) & (cmax[:, 2] - np.minimum(cmin[:, 2], 1.2) > 1.0 + dnear_c * math.tan(math.radians(50)))
    flagged = set(hit) | set(np.nonzero(tall[:ncomp])[0].tolist())
    flagged.discard(-1)
    cen = P.mean(1)
    dtri = rect_dist(cen[:, :2])
    diag = np.linalg.norm(cmax - cmin, axis=1)
    whole = np.zeros(ncomp + 1, bool)            # components exported to the occluder mesh in full
    for it in range(5):
        flag_arr = np.zeros(ncomp + 1, bool)
        flag_arr[list(flagged)] = True
        occ = flag_arr[comp] & (comp >= 0) & ((dtri < 8.0) | (diag[comp] < 16.0) | whole[comp])
        # rays that still reach the footprint after removing these pieces hit new pieces (or the far part of a chained one)
        resid = occluding(P[~occ], np.where(comp[~occ] == -1, -1, comp[~occ]), {-1})
        print('occluder pass', it, 'occluder tris', int(occ.sum()), 'residual components', len(resid))
        if not resid:
            break
        for c in resid:
            whole[c] = True if c in flagged else whole[c]
            flagged.add(c)
    STATS['flagged_components'] = len(flagged)
    STATS['occluder_source_tris'] = int(occ.sum())
    STATS['world_source_tris'] = int((~occ).sum())
    arch_world, arch_occ = L.sub(s, ~occ), L.sub(s, occ)
    STATS['residual_occluding_components_in_world_mesh'] = len(resid)
    STATS['residual_occluding_rays'] = int(sum(resid.values()))

    # ---- 3. glass cut-outs, ferns n/a, per-mesh finishing
    texs['rose'], texs['lancet'] = glass.rose_window(), glass.lancet()

    def glass_pass(a):
        two = np.isin(a['mat'], ['lancet', 'rose'])
        if not two.any():
            return a
        g = [refine_cutout(L.sub(a, a['mat'] == m_), texs[m_], depth=4, mat=m_) for m_ in ('rose', 'lancet') if (a['mat'] == m_).any()]
        a = L.cat(L.sub(a, ~two), *g)
        two = np.isin(a['mat'], ['lancet', 'rose'])
        return L.cat(a, L.flipped(L.sub(a, two), .01))

    arch_world, arch_occ = glass_pass(arch_world), glass_pass(arch_occ)
    # ---- 4. new continuous ground (needs a BVH of everything that can shade it)
    architecture = L.cat(arch_world, arch_occ)
    bvh_arch = make_bvh(architecture['P'])
    paving = build_paving(light_at, bvh_arch)
    rise = risers(riser_mat, light_at)
    rune = rune_ring(light_at)
    # ---- 5. denser vertex spacing + AO on the architecture near the board (<= 1.0 unit = 75 cm edges)
    near = lambda xy: rect_dist(xy) < 14.0
    n_before = (len(arch_world['P']), len(arch_occ['P']))
    arch_world = subdivide(arch_world, 1.0, near)
    arch_occ = subdivide(arch_occ, 1.0, lambda xy: np.ones(len(xy), bool))
    STATS['arch_tris_before_subdivision'] = {'world': n_before[0], 'occluders': n_before[1]}
    both = L.cat(arch_world, arch_occ)
    bvh_all = make_bvh(both['P'])
    arch_world, nw = bake_ao(arch_world, bvh_all)
    arch_occ, no = bake_ao(arch_occ, bvh_all)
    STATS['ao_samples'] = nw + no
    world = L.cat(paving, rise, rune, arch_world)
    occl = arch_occ
    # ---- 6. metres, light shoulder, atlas
    world = L.xform(world, L.S(SCALE))
    occl = L.xform(occl, L.S(SCALE))
    for d in (world, occl):
        d['C'] = L.knee(d['C'])
        gl = np.isin(d['mat'], GLOW_MATS)
        d['C'][gl] = 1.0                                   # emissive in source: diffuse bake ~0, show at full brightness
    texs.update(tiles_procedural())
    texs['flame'] = L.swatch((1.0, .62, .25))
    mats_all = list(dict.fromkeys(world['mat'].tolist() + occl['mat'].tolist()))
    dummy = tri_soup([[(0, 0, -9999), (1, 0, -9999), (0, 1, -9999)]] * len(mats_all), [[(.5, .5), (.51, .5), (.5, .51)]] * len(mats_all),
                     [[(1, 1, 1)] * 3] * len(mats_all), 'x')
    dummy['mat'] = np.array(mats_all, dtype=object)
    solid = [k for k in ('flame',) if k in mats_all]
    repeat = {'pier': (1, 4), 'wall': (1, 2)}
    outs = []
    A = None
    for d in (world, occl):
        d2, A_, place = L.build_atlas(L.cat(dummy, d), texs, size=ATLAS, pad=4, solid=solid, repeat=repeat)
        keep_ = d2['P'][..., 2].max(1) > -5000
        outs.append(L.sub(d2, keep_))
        if A is not None:
            assert np.array_equal(A, A_), 'atlas differs between meshes'
        A = A_
    world, occl = outs
    # ---- 7. objects, export
    bpy.ops.wm.read_factory_settings(use_empty=True)
    img = L.new_image('T_World_VigilSpike_Atlas', A)          # one image shared by both meshes
    o1, g1 = make_world_object('SM_World_VigilSpike', world, img)
    o2, g2 = make_world_object('SM_World_VigilSpike_Occluders', occl, img)
    p1, p2 = export_glb(o1), export_glb(o2)
    L.save_png(A, os.path.join(OUT, 'T_World_VigilSpike_Atlas.png'))
    for name, o, g, d in (('world', o1, g1, world), ('occluders', o2, g2, occl)):
        P_ = d['P']
        e = np.concatenate([np.linalg.norm(P_[:, (i + 1) % 3] - P_[:, i], axis=1) for i in range(3)])
        near_tri = rect_dist(P_.mean(1)[:, :2] / SCALE) < 14
        en = np.concatenate([np.linalg.norm(P_[near_tri][:, (i + 1) % 3] - P_[near_tri][:, i], axis=1) for i in range(3)]) if near_tri.any() else e
        STATS[name] = dict(triangles=int(len(o.data.polygons)), vertices=len(o.data.vertices), glow_tris=g,
                           edge_len_m_median=round(float(np.median(e)), 3), edge_len_m_p90_within_14_units_of_board=round(float(np.percentile(en, 90)), 3),
                           bytes=os.path.getsize(p1 if name == 'world' else p2))
    # footprint clearance check (metres): kept architecture triangles inside the footprint above 10 cm
    arch_all = L.cat(arch_world, arch_occ)
    c = arch_all['P'].mean(1) * SCALE
    bad = (np.abs(c[:, 0]) < FX * SCALE) & (np.abs(c[:, 1]) < FY * SCALE) & (c[:, 2] > .1)
    STATS['architecture_tris_inside_footprint_above_10cm'] = int(bad.sum())
    STATS['footprint_m'] = [FX * SCALE * 2, FY * SCALE * 2]
    STATS['cell_m'] = 2 * SCALE
    STATS['atlas'] = ATLAS
    with open(os.path.join(OUT, 'world_metrics.json'), 'w') as fh:
        json.dump(STATS, fh, indent=1)
    print('WORLD_METRICS', json.dumps(STATS))


main()
