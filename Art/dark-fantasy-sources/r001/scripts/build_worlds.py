"""Environments: vigil.blend / root.blend `world` -> SM_World_Vigil.glb / SM_World_Root.glb.

Source units: 2 per tactical cell, 18x12 footprint = [-18,18] x [-12,12], centred on the origin.
Steps: cut footprint + 0.5-cell margin, flat ground at z=0 with risers to the outer terraces,
remove camera occluders, scale 0.55 (1 cell = 1.10 m), single atlas, baked light as COLOR_0.
"""
import math
import sys

sys.path.insert(0, __import__('os').path.dirname(__import__('os').path.abspath(__file__)))
import bpy
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

import tlib as L

SCALE = .55
FX, FY = 18.0, 12.0                  # footprint half extents (source units)
MX, MY = FX + 1.0, FY + 1.0          # + half-cell margin
NEAR = 6.0                           # "about 3 cells" around the footprint
WHICH = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else ['vigil', 'root']


def level(y):
    return .4 if 6 <= y < 8 else .8 if 8 <= y < 10 else 1.2 if y >= 10 else 0.


def outer_level(y):
    """Outer terrain height at the cut line, by the source cell centre (odd coordinates)."""
    yc = 2 * math.floor(y / 2) + 1
    return level(yc)


def tri_normals(P):
    n = np.cross(P[:, 1] - P[:, 0], P[:, 2] - P[:, 0])
    return n / np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-12)


def quad(p, uv, c, mat):
    """Two triangles a,b,c / a,c,d with attributes."""
    a, b, cc, d = range(4)
    P = np.array([[p[a], p[b], p[cc]], [p[a], p[cc], p[d]]], float)
    UV = np.array([[uv[a], uv[b], uv[cc]], [uv[a], uv[cc], uv[d]]], float)
    C = np.array([[c[a], c[b], c[cc]], [c[a], c[cc], c[d]]], float)
    return dict(P=P, UV=UV, C=C, mat=np.array([mat, mat], dtype=object))


def sampler(points, colours):
    """Inverse-distance light lookup from the original ground vertices (XY)."""
    def f(xy):
        d = np.linalg.norm(points[None, :, :2] - np.asarray(xy)[:, None, :], axis=2)
        idx = np.argsort(d, axis=1)[:, :6]
        w = 1 / (np.take_along_axis(d, idx, 1) + .5)
        return (colours[idx] * w[..., None]).sum(1) / w.sum(1, keepdims=True)
    return f


def new_ground(floor_mat, riser_mat, light_at):
    xs = np.array([-MX] + list(np.arange(-FX, FX + .1, 2)) + [MX])
    ys = np.array([-MY] + list(np.arange(-FY, FY + .1, 2)) + [MY])
    gx, gy = np.meshgrid(xs, ys, indexing='ij')
    col = light_at(np.stack([gx.ravel(), gy.ravel()], 1)).reshape(len(xs), len(ys), 3)
    parts = []
    for i in range(len(xs) - 1):
        for j in range(len(ys) - 1):
            p = [(xs[i], ys[j], 0), (xs[i + 1], ys[j], 0), (xs[i + 1], ys[j + 1], 0), (xs[i], ys[j + 1], 0)]
            uv = [(x / 8, y / 8) for x, y, _ in p]
            c = [col[i, j], col[i + 1, j], col[i + 1, j + 1], col[i, j + 1]]
            parts.append(quad(p, uv, c, floor_mat))
    # risers from z=0 up to the outer terraces, facing into the footprint
    def riser(a, b, h, inward):
        if h <= 0:
            return
        p = [(a[0], a[1], 0), (b[0], b[1], 0), (b[0], b[1], h), (a[0], a[1], h)]
        n = np.cross(np.subtract(p[1], p[0]), np.subtract(p[3], p[0]))
        if np.dot(n[:2], inward) < 0:
            p = [p[1], p[0], p[3], p[2]]
        along = [np.hypot(q[0], q[1]) for q in p]
        uv = [((q[0] + q[1]) / 4, q[2] * .1) for q in p]
        c = list(light_at(np.array([[q[0], q[1]] for q in p])) * .8)
        parts.append(quad(p, uv, c, riser_mat))
    for x0 in np.arange(-MX, MX - 1e-6, 1.0):              # north edge, 1-unit segments
        riser((x0, MY), (x0 + 1, MY), outer_level(MY + .5), (0, -1))
        riser((x0, -MY), (x0 + 1, -MY), outer_level(-MY - .5), (0, 1))
    for y0 in np.arange(-MY, MY - 1e-6, 1.0):
        h = outer_level(y0 + .5)
        riser((MX, y0), (MX, y0 + 1), h, (-1, 0))
        riser((-MX, y0), (-MX, y0 + 1), h, (1, 0))
    return L.cat(*parts)


def fern_blades(s, light_mean):
    """Cutout fern cards (8 tris each: cross x front/back) -> opaque low-poly blade clusters."""
    n = len(s['P']) // 8
    parts = []
    rng = np.random.default_rng(7)
    for i in range(n):
        P = s['P'][8 * i:8 * i + 8].reshape(-1, 3)
        c = P.mean(0)
        base = np.array([c[0], c[1], P[:, 2].min()])
        h = P[:, 2].max() - base[2]
        w = np.ptp(P[:, :2], axis=0).max()
        C = s['C'][8 * i:8 * i + 8].reshape(-1, 3)
        lo = C[np.argsort(P[:, 2])[:8]].mean(0)
        hi = C[np.argsort(P[:, 2])[-8:]].mean(0)
        k = 5
        for b in range(k):
            a = b / k * math.tau + rng.uniform(0, .6)
            reach = w * .5 * rng.uniform(.6, 1.0)
            tall = h * rng.uniform(.65, 1.0)
            d = np.array([math.cos(a), math.sin(a), 0])
            side = np.array([-d[1], d[0], 0]) * .2
            mid = base + d * reach * .45 + np.array([0, 0, tall * .8])
            tip = base + d * reach + np.array([0, 0, tall * .55])
            p0, p1 = base - side, base + side
            m0, m1 = mid - side * .6, mid + side * .6
            tri = [(p0, p1, m1), (p0, m1, m0), (m0, m1, tip)]
            vv = {id(p0): .95, id(p1): .95, id(m0): .45, id(m1): .45, id(tip): .05}
            for t in tri:
                cols = [lo * (1 - (1 - vv[id(q)])) + hi * (1 - vv[id(q)]) for q in t]
                uv = [(.5, vv[id(q)]) for q in t]
                one = dict(P=np.array([t]), UV=np.array([uv]), C=np.array([cols]), mat=np.array(['fernblade'], dtype=object))
                parts.append(one)
                parts.append(L.flipped(one, .004))
    return L.cat(*parts)


def fern_texture():
    """Vertical blade gradient in the fern's own greens (top-down rows: tip at top)."""
    ramp = np.array([[.62, .78, .36], [.48, .64, .27], [.36, .5, .2], [.26, .38, .15], [.18, .27, .1], [.12, .18, .07]])
    t = np.ones((32, 8, 4), np.float32)
    for r in range(32):
        t[r, :, :3] = ramp[min(5, r * 6 // 32)]
    t[:, ::4, :3] *= .85
    # same muted bronze-moss grade worlds_pass applied to Root greens
    rgb = t[..., :3]
    rgb = rgb * .7 + rgb.mean(-1, keepdims=True) * .3
    t[..., :3] = np.clip(rgb * np.array([1.0, .78, .78]), 0, 1)
    return t


DEBUG = None


def occluders(s, comp, protect, cams, targets):
    """Components hit (front faces only) before any footprint target, from any camera."""
    P = s['P']
    nrm = tri_normals(P)
    verts = P.reshape(-1, 3)
    bvh = BVHTree.FromPolygons([Vector(v) for v in verts], [(3 * i, 3 * i + 1, 3 * i + 2) for i in range(len(P))], all_triangles=True)
    hit = {}
    for eye in cams:
        e = Vector(eye)
        for t in targets:
            tv = Vector(t)
            d = (tv - e)
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
                    if DEBUG is not None and len(DEBUG) < 2000:
                        DEBUG.append((idx, tuple(e), tuple(tv)))
                break
    return hit


def cameras():
    out = []
    for dist_m in (18, 25, 35):
        for pitch in (45, 55, 65):
            D = dist_m / SCALE
            for yaw in (0, 90, 180, 270):
                r, p = math.radians(yaw), math.radians(pitch)
                out.append((math.cos(r) * math.cos(p) * D, math.sin(r) * math.cos(p) * D, math.sin(p) * D))
    return out


def build(key):
    L.open_blend(key)
    ob = bpy.data.objects['world']
    s, texs = L.soup_from_object(ob, light=L.bake_light(1.05, .58))
    print(key, 'source tris', len(s['P']))
    floor_mat = 'floor'
    riser_mat = 'sstone' if key == 'vigil' else 'menhir'
    ground_mats = {'floor', 'moss'}
    nrm = tri_normals(s['P'])
    # 1. align ground texture to the cell grid (u = X/8: one flagstone per cell, no UV seams)
    flat = np.isin(s['mat'], list(ground_mats)) & (nrm[:, 2] > .99)
    s['UV'][flat] = s['P'][flat][..., :2] / 8
    # light samples from the original ground near the footprint (before removal)
    gnd = flat & np.all(np.abs(s['P'][..., 0]) < MX + 3, 1) & np.all(np.abs(s['P'][..., 1]) < MY + 3, 1)
    light_at = sampler(s['P'][gnd].reshape(-1, 3), s['C'][gnd].reshape(-1, 3))
    # 2. ground by geometry (flat floor/moss tiles + terrace risers on cell edges), props by connectivity
    P = s['P']
    vert = np.abs(nrm[:, 2]) < .01
    ymid = P[..., 1].mean(1)
    on_edge = np.abs(ymid - np.round(ymid / 2) * 2) < 1e-3
    riser = np.isin(s['mat'], [riser_mat]) & vert & on_edge & (P[..., 2].min(1) > -.45) & (P[..., 2].max(1) < 1.25) & (np.ptp(P[..., 0], 1) <= 2.001)
    is_ground = flat | riser
    comp = np.full(len(P), -1)
    comp[~is_ground] = L.components(L.sub(s, ~is_ground))
    ground_comps = {-1}
    print(key, 'prop components', comp.max() + 1, 'ground tris', int(is_ground.sum()), 'risers', int(riser.sum()))
    # whole non-ground components touching footprint + margin are removed
    P = s['P']
    ncomp = comp.max() + 1
    cmin = np.full((ncomp + 1, 3), np.inf)
    cmax = np.full((ncomp + 1, 3), -np.inf)
    np.minimum.at(cmin, comp, P.min(1))
    np.maximum.at(cmax, comp, P.max(1))     # index -1 (last row) collects the ground
    touch = (cmin[:, 0] < MX) & (cmax[:, 0] > -MX) & (cmin[:, 1] < MY) & (cmax[:, 1] > -MY)
    # tall pieces within ~3 cells: rise above a 50-degree sight line from the footprint edge
    dx = np.maximum(0, np.maximum(-FX - cmax[:, 0], cmin[:, 0] - FX))
    dy = np.maximum(0, np.maximum(-FY - cmax[:, 1], cmin[:, 1] - FY))
    dnear = np.hypot(dx, dy)
    tall = (dnear < NEAR) & (cmax[:, 2] - np.minimum(cmin[:, 2], 1.2) > 1.0 + dnear * math.tan(math.radians(50)))
    drop = touch.copy()
    drop[-1] = False
    print(key, 'drop: footprint', int(drop.sum()))
    # tall-near pieces can be chained to far ones (shared buttress vertices): split per base anchor
    cen = P.mean(1)
    tri_drop = drop[comp].copy()
    for c in np.nonzero(tall & ~touch)[0]:
        if c == ncomp:
            continue
        idx = np.nonzero(comp == c)[0]
        base = idx[P[idx][..., 2].min(1) < cmin[c, 2] + .15]
        anchors = []
        for b in base:
            for a in anchors:
                if np.hypot(*(cen[b, :2] - a[0] / a[1])) < 3:
                    a[0] += cen[b, :2]; a[1] += 1
                    break
            else:
                anchors.append([cen[b, :2].copy(), 1])
        if not anchors:
            # floating (buttress chains, arches): trim only the part over the near band
            dc = np.hypot(np.maximum(0, np.abs(cen[idx, 0]) - FX), np.maximum(0, np.abs(cen[idx, 1]) - FY))
            gone = dc < NEAR + .5
            tri_drop[idx[gone]] = True
            print('   tall-near comp', c, s['mat'][idx[0]], 'floating, trimmed', int(gone.sum()), 'of', len(idx), 'bbox', cmin[c].round(1).tolist(), cmax[c].round(1).tolist())
            continue
        A = np.array([a[0] / a[1] for a in anchors])
        near = np.nonzero(np.hypot(np.maximum(0, np.abs(A[:, 0]) - FX), np.maximum(0, np.abs(A[:, 1]) - FY)) < NEAR + .5)[0]
        own = np.argmin(np.linalg.norm(cen[idx, None, :2] - A[None], axis=2), axis=1)
        gone = np.isin(own, near)
        tri_drop[idx[gone]] = True
        print('   tall-near comp', c, s['mat'][idx[0]], 'anchors', len(A), 'removed anchors', A[near].round(1).tolist(), 'tris', int(gone.sum()))
    keep = ~tri_drop
    s = L.sub(s, keep)
    comp = comp[keep]
    # ground: clip out the footprint + margin
    g = comp == -1
    gs = L.clip_rect(L.sub(s, g), -MX, MX, -MY, MY)
    rest = L.sub(s, ~g)
    rest_comp = comp[~g]
    s = L.cat(gs, rest)
    comp = np.concatenate([np.full(len(gs['P']), -1), rest_comp])
    # 3. flat ground + risers
    ng = new_ground(floor_mat, riser_mat, light_at)
    s = L.cat(s, ng)
    comp = np.concatenate([comp, np.full(len(ng['P']), -1)])
    # 4. occlusion: rays from game-like cameras to the playable cells, iterate until clear
    tx = np.arange(-FX + .25, FX, .5)
    ty = np.arange(-FY + .25, FY, .5)
    targets = [(x, y, .02) for x in tx for y in ty]
    cams = cameras()
    for it in range(4):
        hit = occluders(s, comp, {-1}, cams, targets)
        if not hit:
            break
        for c, n in sorted(hit.items(), key=lambda kv: -kv[1]):
            m = comp == c
            print(f'   occluder comp {c} ({s["mat"][m][0]}, {int(m.sum())} tris) blocked {n} rays, bbox',
                  s['P'][m].reshape(-1, 3).min(0).round(1), s['P'][m].reshape(-1, 3).max(0).round(1))
        keep = ~np.isin(comp, list(hit))
        s = L.sub(s, keep)
        comp = comp[keep]
    global DEBUG
    DEBUG = []
    ground_hits = occluders(s, np.where(comp == -1, -2, comp), set(), cams, targets)
    print(key, 'remaining occluders after pass:', ground_hits)
    if DEBUG:
        import collections
        cnt = collections.Counter((s['mat'][i], tuple(s['P'][i].mean(0).round(0)), tuple(np.round(e, 0))) for i, e, _ in DEBUG)
        for k_, v_ in cnt.most_common(12):
            print('   ground hit', k_, v_)
    DEBUG = None
    # 5. ferns -> opaque blades; two-sided stained glass gets back faces
    if 'fern' in set(s['mat']):
        fm = s['mat'] == 'fern'
        blades = fern_blades(L.sub(s, fm), None)
        s = L.cat(L.sub(s, ~fm), blades)
        texs['fernblade'] = fern_texture()
    two = np.isin(s['mat'], ['lancet', 'rose'])
    if two.any():
        # stained glass from the source recipe; its cut-out outline becomes geometry
        import glass
        from build_tabletop import refine_cutout
        texs['rose'], texs['lancet'] = glass.rose_window(), glass.lancet()
        g = [refine_cutout(L.sub(s, s['mat'] == m_), texs[m_], depth=4, mat=m_) for m_ in ('rose', 'lancet') if (s['mat'] == m_).any()]
        s = L.cat(L.sub(s, ~two), *g)
        two = np.isin(s['mat'], ['lancet', 'rose'])
        s = L.cat(s, L.flipped(L.sub(s, two), .01))
    # 6. scale, atlas, light
    s = L.xform(s, L.S(SCALE))
    s['C'] = L.knee(s['C'])
    glow = np.isin(s['mat'], ['capg', 'lancet', 'rose'])   # emissive in source: diffuse bake is ~0, show at full brightness
    s['C'][glow] = 1.0
    solid = [k for k in ('capg',) if k in set(s['mat'])]
    if 'capg' in texs:
        texs['capg'] = L.swatch((1.0, .62, .25))
    s, A, place = L.build_atlas(s, texs, size=1024, pad=4, solid=solid, repeat={'bark': (1, 3), 'pier': (1, 4), 'wall': (1, 2)}, downscale={'iron': 2, 'timber': 2} if key == 'root' else {})
    name = 'SM_World_Vigil' if key == 'vigil' else 'SM_World_Root'
    bpy.ops.wm.read_factory_settings(use_empty=True)
    ob = L.make_object(name, s, A, color=True)
    L.export_glb(ob, color=True)
    L.save_png(A, f'{L.WORK}/{name}_atlas.png')


for k in WHICH:
    build(k)
