"""Drowned Vigil world kit r002 (issue #145): metric world shell + occluder mesh.

Run from the repository root:   blender -b --python Art/dark-fantasy-sources/r002/scripts/build_vigil_r002.py

Reads Art/dark-fantasy-sources/r001/vigil.blend (never modified) and writes
  Art/world-vigil/r002/SM_World_Vigil.glb                     world shell: continuous paving + kept architecture
  Art/world-vigil-occluders/r001/SM_World_Vigil_Occluders.glb near / camera-occluding architecture (dither-fade pieces)
  Art/dark-fantasy-sources/r002/world_metrics.json            measured numbers quoted in the README
  Art/dark-fantasy-sources/r002/T_World_Vigil_Atlas.png       the 2048x2048 atlas (identical image embedded in both glbs)

Algorithms taken from the #140 spike (build_world_spike.py, ray-tested occluder split, Voronoi paving, glow mask), changed for r002:
 * stones are tighter (60 cm lattice, Lloyd-relaxed) and their widest chord is measured;
 * subdivision is conforming (no T-junctions), which is what removes most of the spike's 41.6k occluder triangles;
 * the baked rune ring is outside the playable footprint, on the outer terrace;
 * outer terraces use a seamless 512 px paving tile (256 texels/m) with the same stone size as the playable ground;
 * all texture sampling is Linear (the material says so), atlas regions carry 16 px of wrapped padding for mip chains.

Units: source scene has 2 units per cell; SCALE = 0.75 m per unit gives 1 cell = 150 cm, so the 18 x 12 cell footprint is
27 m x 18 m centred on the origin.  Z up, +X forward, metres.  COLOR_0 is RGBA: RGB = baked light (linear), A = glow mask.
"""
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import r002_lib as R
from r002_lib import L, bpy, np, Vector, make_bvh, tri_normals, smoothstep, fbm, vnoise, subdivide, ambient, bake_ao, tri_soup, edge_lengths

import glass

ROOT = R.ROOT
R001 = R.R001
OUT_WORLD = os.path.join(ROOT, 'Art', 'world-vigil', 'r002')
OUT_OCC = os.path.join(ROOT, 'Art', 'world-vigil-occluders', 'r001')
OUT_SRC = os.path.join(ROOT, 'Art', 'dark-fantasy-sources', 'r002')

SCALE = .75                      # source unit -> metres (2 units = 1 cell = 1.5 m)
FX, FY = 18.0, 12.0              # footprint half extents, source units (18 x 12 cells)
MX, MY = FX + 1.0, FY + 1.0      # ground margin (half a cell, as r001)
NEAR = 6.0                       # r001: "about 3 cells" around the footprint
ATLAS = 2048
OCC_EDGE_U = 1.0                 # subdivision edge limit, source units (75 cm)
S_TILE = 1.3334                  # source units covered by one 256 px procedural tile (= 1.0 m)
PAD = 16                         # px of wrapped padding around each atlas region (mip-safe to mip 4)
TERRACE_TILE_M = 2.0             # metres per 512 px terrace paving tile (256 texels/m)
GLOW_MATS = ('lancet', 'rose', 'flame', 'rune')
rng = np.random.default_rng(1451)
STATS = {}


# ------------------------------------------------------------------ small helpers
def level(y):
    return .4 if 6 <= y < 8 else .8 if 8 <= y < 10 else 1.2 if y >= 10 else 0.


def outer_level(y):
    yc = 2 * math.floor(y / 2) + 1
    return level(yc)


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
    t[..., :3] = np.array((.16, .27, .13)) * (.65 + .55 * big[..., None]) * (.9 + .2 * mid[..., None])
    out['moss'] = t
    t = np.ones((n, n, 4), np.float32)
    t[..., :3] = np.array((.26, .23, .20)) * (.7 + .5 * big[..., None]) * (.9 + .2 * mid[..., None])
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
    out['paveT'] = R.periodic_flag_tile(512, TERRACE_TILE_M, seed=5)
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


PATCH_U = 5.5          # source units (6 m): flagstone laying patches, each with its own random orientation
JOINT_U = .06          # joint width, source units (4.5 cm)
SIDE_LIM = (.47, 1.20)  # stone long side limits, source units (35-90 cm)


def _ccw(poly):
    poly = np.asarray(poly, float)
    a = .5 * (np.dot(poly[:, 0], np.roll(poly[:, 1], -1)) - np.dot(poly[:, 1], np.roll(poly[:, 0], -1)))
    return poly if a > 0 else poly[::-1]


def _shrink(poly, g):
    """Move every edge of a convex CCW polygon inward by g."""
    pts = [tuple(q) for q in poly]
    m = len(poly)
    for i in range(m):
        a, b = poly[i], poly[(i + 1) % m]
        d = b - a
        ln = np.linalg.norm(d)
        if ln < 1e-9:
            continue
        n = np.array((d[1], -d[0])) / ln
        pts = clip_poly([np.array(q) for q in pts], n, n @ a - g)
        if len(pts) < 3:
            return None
    return np.array(pts)


def min_area_rect(poly):
    """(area, long, short) of the minimum-area bounding rectangle, brute force over 3 degree steps."""
    best = (9e9, 0.0, 0.0)
    for ang in np.radians(np.arange(0, 180, 3)):
        u = np.array((math.cos(ang), math.sin(ang)))
        v = np.array((-u[1], u[0]))
        w_, h_ = np.ptp(poly @ u), np.ptp(poly @ v)
        if w_ * h_ < best[0]:
            best = (w_ * h_, max(w_, h_), min(w_, h_))
    return best


def flagstone_cells():
    """Irregular flagstones.  The ground is split into ~6 m patches (Voronoi regions of jittered seeds); inside each patch the
    stones are laid in courses of random height with random stone lengths and random bond offsets, the whole patch rotated by its
    own random angle, so no global lattice (square or hex) and no alignment with the 150 cm cells.  Corners are perturbed and
    often chamfered (4-6 sides), then stones are clipped to the patch and the ground rectangle and shrunk by half a joint."""
    seeds = []
    for x in np.arange(-MX - PATCH_U, MX + PATCH_U + 1e-6, PATCH_U):
        for y in np.arange(-MY - PATCH_U, MY + PATCH_U + 1e-6, PATCH_U):
            seeds.append((x + rng.uniform(-.3, .3) * PATCH_U + (y // PATCH_U % 2) * .25 * PATCH_U, y + rng.uniform(-.3, .3) * PATCH_U))
    seeds = np.array(seeds)
    cells = []
    for pi, sd in enumerate(seeds):
        if abs(sd[0]) > MX + PATCH_U * .9 or abs(sd[1]) > MY + PATCH_U * .9:
            continue
        # patch region: clip a big square by bisectors with the neighbours
        region = [sd + np.array(v) for v in ((-PATCH_U * 1.5,) * 2, (PATCH_U * 1.5, -PATCH_U * 1.5), (PATCH_U * 1.5,) * 2, (-PATCH_U * 1.5, PATCH_U * 1.5))]
        d_all = np.linalg.norm(seeds - sd, axis=1)
        for j in np.argsort(d_all)[1:12]:
            n = (seeds[j] - sd) / d_all[j]
            region = [q + sd for q in clip_poly([q - sd for q in region], n, d_all[j] / 2)]
            if len(region) < 3:
                break
        if len(region) < 3:
            continue
        region = np.array(region)
        th = math.radians(rng.uniform(12, 78) + 90 * rng.integers(0, 4))     # never near the cell axes
        ca, sa = math.cos(th), math.sin(th)
        Rm = np.array(((ca, -sa), (sa, ca)))
        reach = float(np.max(np.linalg.norm(region - sd, axis=1))) + 1.0
        y = -reach
        slope_b = rng.uniform(-.05, .05)
        while y < reach:
            h = rng.uniform(.50, .95)
            slope_t = float(np.clip(slope_b + rng.uniform(-.03, .03), -.06, .06))     # course edges drift: no parallel lattice
            x = -reach - rng.uniform(0, 1.2)
            while x < reach:
                if rng.random() < .3:
                    l = h * rng.uniform(.95, 1.35)                                       # near-square stone
                else:
                    l = h * rng.uniform(1.2, 2.6)
                l = float(np.clip(l, *SIDE_LIM))
                lean = rng.uniform(-.14, .14)                                            # slanted joints
                yb0, yb1 = y + slope_b * x, y + slope_b * (x + l)
                yt0, yt1 = y + h + slope_t * x, y + h + slope_t * (x + l)
                corners = np.array(((x, yb0), (x + l, yb1), (x + l + lean, yt1), (x + lean * .5, yt0)))
                corners += rng.uniform(-.04, .04, corners.shape)
                poly = [q for q in corners]
                for _ in range(rng.integers(0, 3) if rng.random() < .45 else 0):         # chamfer a corner -> 5 or 6 sides
                    k = rng.integers(len(poly))
                    a_, b_, c_ = poly[k - 1], poly[k], poly[(k + 1) % len(poly)]
                    t1, t2 = rng.uniform(.14, .32), rng.uniform(.14, .32)
                    la, lc = np.linalg.norm(a_ - b_), np.linalg.norm(c_ - b_)
                    if min(la, lc) < .45:
                        continue
                    poly = poly[:k] + [b_ + (a_ - b_) / la * t1 * 1.4, b_ + (c_ - b_) / lc * t2 * 1.4] + poly[k + 1:]
                x += l
                world = [Rm @ q + sd for q in poly]
                cen = np.mean(world, axis=0)
                if np.linalg.norm(cen - sd) > reach:
                    continue
                for n, d0 in [((seeds[j] - sd) / d_all[j], d_all[j] / 2) for j in np.argsort(d_all)[1:12]]:
                    world = [q + sd for q in clip_poly([q - sd for q in world], n, d0)]
                    if len(world) < 3:
                        break
                if len(world) < 3:
                    continue
                for n, d0 in ((np.array((1., 0)), MX), (np.array((-1., 0)), MX), (np.array((0, 1.)), MY), (np.array((0, -1.)), MY)):
                    world = clip_poly(world, n, d0)
                    if len(world) < 3:
                        break
                if len(world) < 3:
                    continue
                pg = _shrink(_ccw(world), JOINT_U / 2)
                if pg is None or len(pg) < 4:
                    continue
                area = .5 * abs(np.dot(pg[:, 0], np.roll(pg[:, 1], -1)) - np.dot(pg[:, 1], np.roll(pg[:, 0], -1)))
                if area < .07 or min_area_rect(pg)[2] < .28:                       # drop slivers (< 27 cm wide) cut by patch borders
                    continue
                cells.append((pg.mean(0), pg, area))
            y += h
            slope_b = slope_t
    return cells


BASE_Z = -.07        # grout / soil level (source units, 5 cm)


def build_paving(light_at, bvh):
    cells = flagstone_cells()
    parts = []
    diam, ranges, shorts, nverts, eang = [], [], [], [], []
    heights = []
    stone_c = []
    for c, poly, area in cells:
        m_field = fbm(c[0], c[1])
        edge = 1 - np.clip(min(MX - abs(c[0]), MY - abs(c[1])) / 3.0, 0, 1)
        moss = float(smoothstep(.62, .78, m_field + .12 * edge))        # r002: only rare whole mossy stones
        moss_v = lambda q: smoothstep(.56, .74, .6 * fbm(q[0] * 1.0 + 7, q[1] * 1.0 - 3, 5) + .4 * vnoise(q[0] * 1.7, q[1] * 1.7, 1.0, 9) + .1 * edge)
        soilp = float(smoothstep(.72, .84, fbm(c[0] + 40, c[1] - 17, 31)))
        if rng.random() < soilp * .6:
            continue                                       # a bare soil patch instead of a stone
        stone_c.append(c)
        interior = bool(np.all(np.abs(poly[:, 0]) < MX - .1) and np.all(np.abs(poly[:, 1]) < MY - .1))
        if interior and np.linalg.norm(poly[:, None] - poly[None], axis=2).max() > .45:     # skip stones cut small by patch borders / the rectangle
            best = min_area_rect(poly)
            diam.append(math.sqrt(4 * area / math.pi) * SCALE * 100)          # equivalent circle diameter, cm
            ranges.append(best[1] * SCALE * 100)                               # long side of the min-area rectangle, cm
            shorts.append(best[2] * SCALE * 100)
            nverts.append(len(poly))
            e_ = np.roll(poly, -1, axis=0) - poly
            for vv in e_:
                eang.append((math.degrees(math.atan2(vv[1], vv[0])) % 180, float(np.linalg.norm(vv))))
        h = float(np.clip(rng.normal(0, .016), -.036, .036))
        g = rng.uniform(-.028, .028, 2)
        ztop = lambda q: np.clip(h + g[0] * (q[0] - c[0]) + g[1] * (q[1] - c[1]), -.039, .039)
        heights.append(h)
        k = rng.random()
        mat = 'paveM' if moss > .85 else ('paveA' if k < .4 else 'paveB' if k < .75 else 'paveC')
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
        tint = lambda m: 1 + m * (np.array((.80, .93, .70)) - 1) * .75
        mv = np.array([float(moss_v(q)) for q in poly])           # broken patches, mostly on stone edges and in the joints
        mc = float(moss_v(c))
        t_top = tint(mc * .3) * bright
        t_ring = [tint(m_ * .45) * bright * .95 for m_ in mv]
        t_edge = [tint(min(1, m_ * 1.0 + .05)) * bright * .92 for m_ in mv]
        t_wall = [tint(m_) for m_ in mv]
        tris, uvs, cols = [], [], []
        for i in range(n):
            j = (i + 1) % n
            a, b = (inner[i][0], inner[i][1], zin[i]), (inner[j][0], inner[j][1], zin[j])
            tris.append((tuple(ctop), a, b))
            uvs.append((uv(c), uv(inner[i]), uv(inner[j])))
            cols.append((L_in * t_top, L_poly[i] * t_ring[i], L_poly[j] * t_ring[j]))
            o0, o1 = (poly[i][0], poly[i][1], zout[i]), (poly[j][0], poly[j][1], zout[j])
            # bevel: inner ring -> outer ring (normal up/outwards)
            tris += [(a, o0, o1), (a, o1, b)]
            uvs += [(uv(inner[i]), uv(poly[i]), uv(poly[j])), (uv(inner[i]), uv(poly[j]), uv(inner[j]))]
            ce0, ce1 = L_poly[i] * t_edge[i], L_poly[j] * t_edge[j]
            ci0, ci1 = L_poly[i] * t_ring[i], L_poly[j] * t_ring[j]
            cols += [(ci0, ce0, ce1), (ci0, ce1, ci1)]
            # wall down to the soil level
            b0, b1 = (poly[i][0], poly[i][1], BASE_Z), (poly[j][0], poly[j][1], BASE_Z)
            tris += [(o0, b0, b1), (o0, b1, o1)]
            uvs += [(uv(poly[i], zout[i]), uv(poly[i], BASE_Z), uv(poly[j], BASE_Z)), (uv(poly[i], zout[i]), uv(poly[j], BASE_Z), uv(poly[j], zout[j]))]
            cw0, cw1 = L_poly[i] * .84 * t_wall[i], L_poly[j] * .84 * t_wall[j]
            cols += [(ce0, cw0, cw1), (ce0, cw1, ce1)]
        mats = [mat] * len(tris)
        s = tri_soup(tris, uvs, cols, mat)
        s['UV'] = np.clip(s['UV'], .02, .98)
        # orient upwards: flip any top/bevel triangle that faces down
        nz = tri_normals(s['P'])[:, 2]
        top = np.arange(len(tris)) % 5 < 3
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
    diam, ranges, shorts, nverts = map(np.array, (diam, ranges, shorts, nverts))
    asp = ranges / shorts
    pc = lambda v, qs=(5, 50, 95): [round(float(np.percentile(v, q)), 1) for q in qs]
    STATS['stones'] = len(stone_c)
    STATS['stones_measured_interior'] = len(diam)
    STATS['stone_equiv_diameter_cm_p5_p50_p95'] = pc(diam)
    STATS['stone_long_side_cm_p5_p50_p95_max'] = pc(ranges) + [round(float(ranges.max()), 1)]
    STATS['stone_short_side_cm_p5_p50_p95'] = pc(shorts)
    STATS['stone_aspect_long_over_short_p5_p50_p95_max'] = [round(float(np.percentile(asp, q)), 2) for q in (5, 50, 95)] + [round(float(asp.max()), 2)]
    STATS['stone_vertex_count_histogram'] = {int(k): int((nverts == k).sum()) for k in sorted(set(nverts.tolist()))}
    ea = np.array(eang)
    hist, _ = np.histogram(ea[:, 0], bins=18, range=(0, 180), weights=ea[:, 1])
    STATS['edge_orientation_histogram_10deg_bins_length_share'] = [round(float(h / hist.sum()), 3) for h in hist]
    ang = np.radians(ea[:, 0])
    wgt = ea[:, 1] / ea[:, 1].sum()
    STATS['edge_orientation_resultants_R2_R4_R6'] = [round(float(abs((wgt * np.exp(1j * k * ang)).sum())), 3) for k in (2, 4, 6)]
    near_axis = ((ea[:, 0] < 3) | (ea[:, 0] > 177) | (abs(ea[:, 0] - 90) < 3))
    STATS['edge_length_share_within_3deg_of_cell_axes'] = round(float(wgt[near_axis].sum()), 3)
    STATS['stone_top_height_range_cm'] = [round(float(min(heights)) * SCALE * 100, 2), round(float(max(heights)) * SCALE * 100, 2)]
    STATS['stone_tris'] = int(len(stones['P']))
    # base plane in 1-unit quads (75 cm vertex spacing): soil / moss by the same field
    tris, uvs, cols, mats = [], [], [], []
    for x in np.arange(-MX, MX - 1e-6, 1.0):
        for y in np.arange(-MY, MY - 1e-6, 1.0):
            q = [(x, y), (x + 1, y), (x + 1, y + 1), (x, y + 1)]
            m_field = np.array([fbm(*p) for p in q])
            edge = np.array([1 - np.clip(min(MX - abs(p[0]), MY - abs(p[1])) / 3.0, 0, 1) for p in q])
            moss = smoothstep(.56, .72, m_field + .12 * edge)
            mat = 'moss' if moss.mean() > .85 else 'soil'
            Lq = light_at(np.array(q))
            col = [Lq[i] * (1.0 * (1 + moss[i] * (np.array((.85, .95, .8)) - 1))) for i in range(4)]
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


def rune_ring(cx, cy, z, r0=2.0, r1=2.5, seg=48):
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
        h = max(h, 0.0)      # r002: a 5 cm skirt even where the terrace is at floor level, so no gap shows
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


# ------------------------------------------------------------------ rune ring placement (outside the playable area)
def place_rune_ring(P, comp, radius_u):
    """Nearest spot to the board, outside the footprint + margin, where flat level ground (comp == -1) lies under the
    whole ring and no architecture stands within the ring."""
    bvh = make_bvh(P)
    nrm = tri_normals(P)
    best = None
    ring = [(math.cos(a), math.sin(a)) for a in np.linspace(0, 2 * math.pi, 24, endpoint=False)]
    for cx in np.arange(-34, 34.1, 1.0):
        for cy in np.arange(-34, 34.1, 1.0):
            d = float(R_rect(cx, cy))
            if d < radius_u + 1.5 or (best is not None and d >= best[0]):
                continue
            ok = True
            zs = []
            for ox, oy in [(0, 0)] + [(radius_u * c, radius_u * s_) for c, s_ in ring] + [(.5 * radius_u * c, .5 * radius_u * s_) for c, s_ in ring[::3]]:
                loc, n, idx, _ = bvh.ray_cast(Vector((cx + ox, cy + oy, 12.0)), Vector((0, 0, -1)), 30)
                if loc is None or comp[idx] != -1 or nrm[idx][2] < .99:
                    ok = False
                    break
                zs.append(loc.z)
            if ok and np.ptp(zs) < .01:
                # nothing tall standing over the ring: cast sideways 0.5 u above the ground
                best = (d, cx, cy, float(np.mean(zs)))
    return best


def R_rect(cx, cy):
    return float(rect_dist(np.array([[cx, cy]]))[0])


# ------------------------------------------------------------------ main
def main():
    os.makedirs(OUT_WORLD, exist_ok=True)
    os.makedirs(OUT_OCC, exist_ok=True)
    os.makedirs(OUT_SRC, exist_ok=True)
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
    nrm = tri_normals(s['P'])
    flat = np.isin(s['mat'], ground_mats) & (nrm[:, 2] > .99)
    # r002: the outer terraces take the same irregular paving as the playable ground, as a seamless 2 m tile
    # (512 px = 256 texels/m, r001 had ~85).  Variation (moss) comes from vertex colour.
    s['mat'][flat] = 'paveT'
    ct, st_ = math.cos(math.radians(21)), math.sin(math.radians(21))      # rotated so terrace courses never align with the cells
    xy_ = s['P'][flat][..., :2] * SCALE / TERRACE_TILE_M
    s['UV'][flat] = np.stack([xy_[..., 0] * ct - xy_[..., 1] * st_, xy_[..., 0] * st_ + xy_[..., 1] * ct], -1)
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

    # ---- 1. clear only what physically intrudes into the playable footprint (18 x 12 cells; smaller boards sit inside it)
    cen = P.mean(1)
    inside = (np.abs(cen[:, 0]) < FX + .5) & (np.abs(cen[:, 1]) < FY + .5) & (comp >= 0) & (cen[:, 2] > -.2)
    frac = np.bincount(comp[inside], minlength=ncomp + 1)[:ncomp] / np.maximum(np.bincount(comp[comp >= 0], minlength=ncomp), 1)
    drop_whole = np.zeros(ncomp + 1, bool)
    drop_whole[:ncomp] = frac > .4
    drop = inside | drop_whole[comp] & (comp >= 0)
    STATS['cleared_intruding_tris'] = int(drop.sum())
    STATS['cleared_components_whole'] = int(drop_whole.sum())
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
    whole = np.zeros(ncomp + 1, bool)
    resid = {}
    for it in range(5):
        flag_arr = np.zeros(ncomp + 1, bool)
        flag_arr[list(flagged)] = True
        occ = flag_arr[comp] & (comp >= 0) & ((dtri < 8.0) | (diag[comp] < 16.0) | whole[comp])
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
    comp_world = comp[~occ]
    STATS['residual_occluding_components_in_world_mesh'] = len(resid)
    STATS['residual_occluding_rays'] = int(sum(resid.values()))

    # ---- 3. glass cut-outs
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
    # ---- 4. rune ring outside the playable area, then the new continuous ground
    spot = place_rune_ring(arch_world["P"], _world_comp(arch_world, gs), 2.5)
    if spot is None:
        raise RuntimeError('no free spot for the rune ring')
    STATS['rune_ring'] = dict(centre_m=[round(spot[1] * SCALE, 2), round(spot[2] * SCALE, 2)], z_m=round(spot[3] * SCALE, 3),
                              distance_to_footprint_m=round(spot[0] * SCALE, 2), outer_radius_m=round(2.5 * SCALE, 2))
    architecture = L.cat(arch_world, arch_occ)
    bvh_arch = make_bvh(architecture['P'])
    paving = build_paving(light_at, bvh_arch)
    rise = risers(riser_mat, light_at)
    rune = rune_ring(spot[1], spot[2], spot[3] + .03)
    # ---- 5. conforming subdivision (<= 75 cm edges within 14 units of the board; occluders everywhere) + AO + terrace moss
    n_before = (len(arch_world['P']), len(arch_occ['P']))
    def lim_world(mid):        # 75 cm edges near the board: walls within 14 units, ground / terrace within 4
        d = rect_dist(mid[..., :2])
        return np.where(d < 4.0, OCC_EDGE_U, np.where(d < 14.0, np.where(mid[..., 2] > 1.0, OCC_EDGE_U, 2.0), 1e9))

    def lim_occ(mid):          # 75 cm up to ~4 m (where the base AO gradient and the figures are), 2.25 m above
        return np.where(mid[..., 2] < 4.0 / SCALE, OCC_EDGE_U, 3.0)
    arch_world = subdivide(arch_world, lim_world)
    arch_occ = subdivide(arch_occ, lim_occ)
    STATS['arch_tris_before_subdivision'] = {'world': n_before[0], 'occluders': n_before[1]}
    both = L.cat(arch_world, arch_occ)
    bvh_all = make_bvh(both['P'])
    arch_world, nw = bake_ao(arch_world, bvh_all)
    arch_occ, no = bake_ao(arch_occ, bvh_all)
    STATS['ao_samples'] = nw + no
    for a in (arch_world, arch_occ):
        t = a['mat'] == 'paveT'
        if t.any():
            xy = a['P'][t][..., :2]
            m = smoothstep(.58, .74, fbm(xy[..., 0], xy[..., 1]))
            a['C'][t] = a['C'][t] * (1 + m[..., None] * (np.array((.82, .94, .72)) - 1) * .6)
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
        d2, A_, place = L.build_atlas(L.cat(dummy, d), texs, size=ATLAS, pad=PAD, solid=solid, repeat=repeat)
        keep_ = d2['P'][..., 2].max(1) > -5000
        outs.append(L.sub(d2, keep_))
        if A is not None:
            assert np.array_equal(A, A_), 'atlas differs between meshes'
        A = A_
    world, occl = outs
    used_px = sum((w + 2 * PAD) * (h + 2 * PAD) for (_, _, w, h) in place.values())
    STATS['atlas'] = dict(size=ATLAS, regions=len(place), pad_px=PAD, fill_fraction=round(used_px / ATLAS / ATLAS, 3),
                          regions_px={k: [v[2], v[3]] for k, v in place.items()})
    STATS['terrace_texels_per_m'] = 512 / TERRACE_TILE_M
    STATS['r001_terrace_texels_per_m'] = round(512 / 6.0, 1)
    # ---- 7. objects, export
    bpy.ops.wm.read_factory_settings(use_empty=True)
    img = L.new_image('T_World_Vigil_Atlas', A)          # one image shared by both meshes
    o1, g1 = R.make_object_rgba('SM_World_Vigil', world, img, glow_mats=GLOW_MATS, mat_name='M_World_Vigil')
    o2, g2 = R.make_object_rgba('SM_World_Vigil_Occluders', occl, img, glow_mats=GLOW_MATS, mat_name='M_World_Vigil')
    p1 = R.export_glb(o1, os.path.join(OUT_WORLD, 'SM_World_Vigil.glb'))
    p2 = R.export_glb(o2, os.path.join(OUT_OCC, 'SM_World_Vigil_Occluders.glb'))
    L.save_png(A, os.path.join(OUT_SRC, 'T_World_Vigil_Atlas.png'))
    for name, o, g, d, p in (('world', o1, g1, world, p1), ('occluders', o2, g2, occl, p2)):
        P_ = d['P']
        e = edge_lengths(P_).ravel()
        near_tri = rect_dist(P_.mean(1)[:, :2] / SCALE) < 14
        en = edge_lengths(P_[near_tri]).ravel() if near_tri.any() else e
        paving_m = ['paveA', 'paveB', 'paveC', 'paveM', 'soil', 'moss', 'rune']
        non_paving = ~np.isin(d['mat'], paving_m)
        dm = rect_dist(P_.mean(1)[:, :2] / SCALE) * SCALE            # metres from the footprint rectangle
        zc = P_.mean(1)[:, 2]
        ev = lambda m_: edge_lengths(P_[m_]).ravel() if m_.any() else np.zeros(1)
        e_ground4 = ev(non_paving & (dm < 4.5) & (zc < 1.3))        # terrace / risers within 4.5 m of the board
        e_arch10 = ev(non_paving & (dm < 10.5) & (zc >= 1.0))       # architecture above 1 m within 10.5 m
        pave_t = ev(np.isin(d['mat'], ['paveA', 'paveB', 'paveC', 'paveM']))
        STATS[name] = dict(triangles=int(len(o.data.polygons)), glow_tris=g,
                           by_material={k: int((d['mat'] == k).sum()) for k in dict.fromkeys(d['mat'].tolist())},
                           edge_len_m_all_median=round(float(np.median(e)), 3),
                           edge_len_m_terrace_within_4p5m=[round(float(np.median(e_ground4)), 3), round(float(np.percentile(e_ground4, 90)), 3), round(float(e_ground4.max()), 3)],
                           edge_len_m_architecture_above_1m_within_10p5m_median_p90_max=[round(float(np.median(e_arch10)), 3), round(float(np.percentile(e_arch10, 90)), 3), round(float(e_arch10.max()), 3)],
                           edge_len_m_stone_tops_median_p90=[round(float(np.median(pave_t)), 3), round(float(np.percentile(pave_t, 90)), 3)],
                           glb=R.glb_stats(p))
    arch_all = L.cat(arch_world, arch_occ)
    c = arch_all['P'].mean(1) * SCALE
    bad = (np.abs(c[:, 0]) < FX * SCALE) & (np.abs(c[:, 1]) < FY * SCALE) & (c[:, 2] > .1)
    STATS['architecture_tris_inside_footprint_above_10cm'] = int(bad.sum())
    STATS['footprint_m'] = [FX * SCALE * 2, FY * SCALE * 2]
    STATS['cell_m'] = 2 * SCALE
    with open(os.path.join(OUT_SRC, 'world_metrics.json'), 'w') as fh:
        json.dump(STATS, fh, indent=1)
    print('WORLD_METRICS', json.dumps(STATS))


def _world_comp(arch_world, gs):
    """Ground triangles come first in arch_world (clip_rect output), but mixed ordering after the occluder split is not
    guaranteed, so classify by the 'paveT' material and flat normal instead."""
    nz = tri_normals(arch_world['P'])[:, 2]
    return np.where((arch_world['mat'] == 'paveT') & (nz > .99), -1, 0)


main()
