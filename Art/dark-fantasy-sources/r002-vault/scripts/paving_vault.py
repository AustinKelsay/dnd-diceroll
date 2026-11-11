"""Irregular flagstone paving for the Vault floor (issue #149), adapted from the Root r002 recipe (paving_root.py, itself from #145's Vigil recipe; copied, not imported).

Work is in 'source units' of 0.75 m (2 units = 1 cell = 1.5 m) as the Vigil and Root recipes, so the stone statistics are comparable; the caller
scales the returned soup by SCALE.  Differences for the Vault: cold blue-grey stone, almost no bare patches (a crypt is paved wall to wall), the
field fades into plain dark slab only far outside the walls, rime in the joints instead of moss.
"""
import math

import numpy as np

import r002_lib as R
from r002_lib import L, make_bvh, tri_normals, smoothstep, fbm, vnoise, tri_soup, ambient

SCALE = .75
S_TILE = 1.3334              # source units per 256 px stone tile (1.0 m = 256 texels/m)
STATS = {}
rng = np.random.default_rng(1493)
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


PATCH_U = 5.5          # (kept for reference) the #145 patch size; Root uses variable Voronoi patches, see PATCH_SIZE_U
JOINT_U = .06          # joint width, source units (4.5 cm)
SIDE_LIM = (.47, 1.20)  # stone long side limits, source units (35-90 cm)
# Root revision (#149 review): variable, wavy, interlocking patches with earth and moss between stone islands
PATCH_SIZE_U = (3.0, 7.5)   # patch "radius" range, source units (2.3-5.6 m); Poisson-disc seeds, so Voronoi patches of different sizes
EARTH_P = .04               # share of patches left as bare earth / moss / root (no stones)
INTERLOCK_U = 1.3           # amplitude of the noise that warps the patch boundaries (stones of two patches interlock)
FEATHER_U = .9             # stones nearer than this to a patch boundary (source units) are thinned out ...
FEATHER_DROP = .42          # ... with up to this probability, so earth / moss fills the seams


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


def _poisson_seeds(rx, ry):
    """Dart-throwing seeds whose spacing follows a slowly varying size field: patches of different sizes, not a grid."""
    cand = rng.uniform((-rx, -ry), (rx, ry), size=(9000, 2))
    seeds, radii = [], []
    for c in cand:
        r = PATCH_SIZE_U[0] + (PATCH_SIZE_U[1] - PATCH_SIZE_U[0]) * float(smoothstep(.30, .70, fbm(c[0] * .45 + 11, c[1] * .45 - 5, 41)))
        if all(np.linalg.norm(c - s) >= .95 * (r + rr) / 2 for s, rr in zip(seeds, radii)):
            seeds.append(c)
            radii.append(r)
    return np.array(seeds), np.array(radii)


def _sep(A, B):
    """Largest gap along an edge-normal axis between two convex polygons (>= g: the polygons are at least g apart)."""
    best = -9.0
    for P_, Q_ in ((A, B), (B, A)):
        e = np.roll(P_, -1, axis=0) - P_
        nrm = np.stack([e[:, 1], -e[:, 0]], 1)
        nrm /= np.maximum(np.linalg.norm(nrm, axis=1, keepdims=True), 1e-12)
        pa, qa = P_ @ nrm.T, Q_ @ nrm.T
        gap = np.maximum(qa.min(0) - pa.max(0), pa.min(0) - qa.max(0))
        best = max(best, float(gap.max()))
    return best


def flagstone_cells():
    """Irregular flagstones in wavy, interlocking Voronoi patches.  Seeds are Poisson-disc points with a varying spacing, so the patches
    differ in size and shape.  A share of them (EARTH_P) stays bare; each other patch is laid in courses of random height with
    random stone lengths and bond offsets, rotated by its own angle (never near the cell axes).  A stone belongs to the patch that
    owns its centre in a noise-warped Voronoi diagram, so the borders wave and two patches interlock instead of meeting in a straight
    line; stones near a border are thinned out at random (earth and moss fill the seams) and any stone overlapping an accepted one is
    dropped.  Corners are perturbed and often chamfered (4-6 sides); stones are clipped to the ground rectangle and shrunk by half a joint."""
    seeds, radii = _poisson_seeds(MX + PATCH_SIZE_U[1], MY + PATCH_SIZE_U[1])
    earth = rng.random(len(seeds)) < EARTH_P
    warp = lambda c: c + INTERLOCK_U * np.array((float(fbm(c[0] * .55 + 3, c[1] * .55 - 17, 23)) - .5, float(fbm(c[0] * .55 - 29, c[1] * .55 + 5, 27)) - .5)) * 2
    cand = []
    for pi, sd in enumerate(seeds):
        if earth[pi] or abs(sd[0]) > MX + PATCH_SIZE_U[1] * .6 or abs(sd[1]) > MY + PATCH_SIZE_U[1] * .6:
            continue
        th = math.radians(rng.uniform(12, 78) + 90 * rng.integers(0, 4))     # never near the cell axes
        ca, sa = math.cos(th), math.sin(th)
        Rm = np.array(((ca, -sa), (sa, ca)))
        reach = radii[pi] * 1.5 + INTERLOCK_U + 1.0
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
                world = np.array([Rm @ q + sd for q in poly])
                cen = world.mean(0)
                if abs(cen[0]) > MX or abs(cen[1]) > MY:
                    continue
                d = np.linalg.norm(seeds - warp(cen)[None], axis=1)
                order = np.argsort(d)
                if order[0] != pi:                                                       # owned by another patch (stone or earth)
                    continue
                margin = d[order[1]] - d[order[0]]
                if rng.random() < FEATHER_DROP * (1 - float(smoothstep(0, FEATHER_U, margin))):
                    continue                                                             # thinned at the seam: earth or moss shows
                for n, d0 in ((np.array((1., 0)), MX), (np.array((-1., 0)), MX), (np.array((0, 1.)), MY), (np.array((0, -1.)), MY)):
                    world = clip_poly(list(world), n, d0)
                    if len(world) < 3:
                        break
                if len(world) < 3:
                    continue
                pg = _shrink(_ccw(world), JOINT_U / 2)
                if pg is None or len(pg) < 4:
                    continue
                area = .5 * abs(np.dot(pg[:, 0], np.roll(pg[:, 1], -1)) - np.dot(pg[:, 1], np.roll(pg[:, 0], -1)))
                if area < .04 or min_area_rect(pg)[2] < .20:
                    continue
                cand.append((pg.mean(0), pg, area, pi))
            y += h
            slope_b = slope_t
    # overlap resolution in random order: the first stone claims the spot, a stone closer than 70 % of a joint to an accepted one is dropped
    grid, cells = {}, []
    for k in rng.permutation(len(cand)):
        c, pg, area, pid = cand[k]
        gx, gy = int(c[0] // 2.5), int(c[1] // 2.5)
        ok = True
        for ix in (gx - 1, gx, gx + 1):
            for iy in (gy - 1, gy, gy + 1):
                for (c2, pg2, _a, pid2) in grid.get((ix, iy), ()):
                    if pid2 != pid and np.linalg.norm(c - c2) < 1.6 and _sep(pg, pg2) < JOINT_U * .7:      # only stones of different patches can collide
                        ok = False
                        break
                if not ok:
                    break
            if not ok:
                break
        if ok:
            grid.setdefault((gx, gy), []).append((c, pg, area, pid))
            cells.append((c, pg, area))
    STATS['patches'] = dict(total=int(len(seeds)), bare_earth=int(earth.sum()), stone=int((~earth).sum()), radius_m_min_max=[round(float(radii.min()) * SCALE, 2), round(float(radii.max()) * SCALE, 2)],
                            candidate_stones=len(cand), stones_after_overlap=len(cells))
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
        if rng.random() < soilp * .18 + thin ** .9 * .96:
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
        bright = rng.uniform(.93, 1.07)
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
        tint = lambda m: 1 + m * (np.array((.80, .90, 1.02)) - 1) * .75
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
    stones['C'] = stones['C'] * (.65 + .35 * per_tri)[:, None, None]
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
    # share of the 27 x 18 m footprint covered by stone tops (projected area of up-facing stone triangles); the rest is joints, earth, moss, roots
    nz_ = tri_normals(stones['P'])[:, 2]
    c_ = stones['P'].mean(1)
    ar_ = np.linalg.norm(np.cross(stones['P'][:, 1] - stones['P'][:, 0], stones['P'][:, 2] - stones['P'][:, 0]), axis=1) / 2 * nz_
    inside_ = (np.abs(c_[:, 0]) < 13.5 / SCALE) & (np.abs(c_[:, 1]) < 9.0 / SCALE) & (nz_ > .3)
    STATS['footprint_share_stone_tops'] = round(float(ar_[inside_].sum() / (27.0 / SCALE) / (18.0 / SCALE)), 3)
    return stones
