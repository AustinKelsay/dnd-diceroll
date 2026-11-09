"""Irregular flagstone paving for the Root clearing (issue #149), adapted from the #145 r002 Vigil recipe
(Art/dark-fantasy-sources/r002/scripts/build_vigil_r002.py in feature/145-vigil-world-kit; copied, not imported).

Work is in 'source units' of 0.75 m (2 units = 1 cell = 1.5 m) exactly as the Vigil recipe, so the stone statistics are comparable;
the caller scales the returned soup by SCALE.  Differences for Root: warmer stone, the stone field fades out into earth with distance
from the board (irregular edge, no straight cut), moss in the joints and on whole stones, light from the Root light field.
"""
import math

import numpy as np

import r002_lib as R
from r002_lib import L, make_bvh, tri_normals, smoothstep, fbm, vnoise, tri_soup, ambient

SCALE = .75
S_TILE = 1.3334              # source units per 256 px stone tile (1.0 m = 256 texels/m)
STATS = {}
rng = np.random.default_rng(1491)
MX = MY = 0.0                # ground rectangle half extents in source units; set by configure()
FADE = (0.0, 1.0)            # (start, end) metres outside the footprint+margin over which stones thin out into earth
RECT = (0.0, 0.0)            # board footprint+margin half extents, metres


def configure(mx_m, my_m, rect_m, fade_m):
    global MX, MY, RECT, FADE
    MX, MY = mx_m / SCALE, my_m / SCALE
    RECT = rect_m
    FADE = fade_m


def dr_m(x_u, y_u):
    """Distance in metres from a point (source units) to the board footprint+margin rectangle."""
    dx = max(0.0, abs(x_u) * SCALE - RECT[0])
    dy = max(0.0, abs(y_u) * SCALE - RECT[1])
    return math.hypot(dx, dy)


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
                if area < .04 or min_area_rect(pg)[2] < .20:                       # drop slivers (< 27 cm wide) cut by patch borders
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
        d_m = dr_m(c[0], c[1])
        thin = float(smoothstep(FADE[0], FADE[1], d_m + 1.2 * (fbm(c[0] * .8 + 3, c[1] * .8 - 9, 17) - .5)))      # irregular fade into earth
        if rng.random() < soilp * .45 + thin ** .9 * .96:
            continue                                       # a bare earth / moss patch instead of a stone
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
        mats = [mat] * len(tris)
        s = tri_soup(tris, uvs, cols, mat)
        s['UV'] = np.clip(s['UV'], .02, .98)
        # orient upwards: flip any top/bevel triangle that faces down
        nz = tri_normals(s['P'])[:, 2]
        top = np.ones(len(tris), bool)          # root r002: no wall triangles (the 5 cm skirt is hidden by the base plane)
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
    pts = np.array([(c[0] * SCALE, c[1] * SCALE, 0.0) for c in stone_c])
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
    return stones
