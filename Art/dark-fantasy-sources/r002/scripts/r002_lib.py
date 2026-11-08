"""Shared helpers for the r002 (issue #145) Drowned Vigil recipes.  Runs inside Blender 5.2 (numpy only).

Builds on r001's tlib.py ("soup" = dict of numpy arrays P (n,3,3), UV (n,3,2), C (n,3,3) or None, mat (n,)), which is
imported and never modified.  Everything here is original code; the r001 and #140 spike code it grew out of is cited
where an algorithm was reused.

Conventions (same as r001): metres, +X forward, Z up, glTF Y-up export, COLOR_0 = RGBA with RGB baked light (linear)
and A = glow mask (world/kit) or opacity (mist/shaft cards).
"""
import json
import math
import os
import struct
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..', '..', '..'))
R001 = os.path.join(ROOT, 'Art', 'dark-fantasy-sources', 'r001')
os.environ.setdefault('THEME_LOCAL', tempfile.mkdtemp(prefix='r002_'))
sys.path.insert(0, os.path.join(R001, 'scripts'))

import bpy
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

import tlib as L

RNG = np.random.default_rng(1451)


# ------------------------------------------------------------------ geometry basics
def tri_normals(P):
    n = np.cross(P[:, 1] - P[:, 0], P[:, 2] - P[:, 0])
    return n / np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-12)


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


def make_bvh(P):
    verts = [Vector(v) for v in P.reshape(-1, 3)]
    return BVHTree.FromPolygons(verts, [(3 * i, 3 * i + 1, 3 * i + 2) for i in range(len(P))], all_triangles=True)


def tri_soup(tris, uvs, cols, mat):
    P = np.array(tris, float).reshape(-1, 3, 3)
    return dict(P=P, UV=np.array(uvs, float).reshape(-1, 3, 2),
                C=None if cols is None else np.array(cols, float).reshape(-1, 3, 3), mat=np.array([mat] * len(P), dtype=object))


def edge_lengths(P):
    return np.stack([np.linalg.norm(P[:, (i + 1) % 3] - P[:, i], axis=1) for i in range(3)], 1)


# ------------------------------------------------------------------ conforming subdivision (no T-junctions)
def _edge_ids(P):
    A, B = P, P[:, [1, 2, 0]]
    ka, kb = np.round(A * 1e5).astype(np.int64), np.round(B * 1e5).astype(np.int64)
    lt = (ka[..., 0] < kb[..., 0]) | ((ka[..., 0] == kb[..., 0]) & ((ka[..., 1] < kb[..., 1]) | ((ka[..., 1] == kb[..., 1]) & (ka[..., 2] <= kb[..., 2]))))
    first, second = np.where(lt[..., None], ka, kb), np.where(lt[..., None], kb, ka)
    key = np.concatenate([first, second], -1).reshape(-1, 6)
    _, inv = np.unique(key, axis=0, return_inverse=True)
    return inv.ravel().reshape(-1, 3)


def subdivide(s, maxedge, region=None, max_iter=12):
    """Split every edge longer than maxedge (and whose midpoint passes region(mid_xy)) at its midpoint.  The decision is
    made per geometric edge, so both triangles sharing it split it and the mesh stays watertight (no T-junctions).
    Each triangle becomes 1, 2, 3 or 4 triangles depending on how many of its edges split."""
    for _ in range(max_iter):
        P = s['P']
        if not len(P):
            break
        ln = edge_lengths(P)
        mid = (P + P[:, [1, 2, 0]]) / 2
        want = ln > (maxedge(mid) if callable(maxedge) else maxedge)       # maxedge(mid_xyz) may vary with position
        if region is not None:
            want &= region(mid[..., :2])
        if not want.any():
            break
        eid = _edge_ids(P)
        split = np.zeros(eid.max() + 1, bool)
        split[eid[want]] = True
        flags = split[eid]
        ns = flags.sum(1)
        keep = L.sub(s, ns == 0)
        parts = [keep]
        for case in (1, 2, 3):
            idx = np.nonzero(ns == case)[0]
            if not len(idx):
                continue
            if case == 1:
                rot = flags[idx].argmax(1)
            elif case == 2:
                rot = (np.argmin(flags[idx], 1) + 1) % 3
            else:
                rot = np.zeros(len(idx), int)
            order = (rot[:, None] + np.arange(3)[None]) % 3
            att = {}
            for key in ('P', 'UV', 'C'):
                if s[key] is None:
                    continue
                att[key] = np.take_along_axis(s[key][idx], order[:, :, None], 1)
            Pa = att['P']
            sa = None
            if case == 2:
                A_, B_, C_ = Pa[:, 0], Pa[:, 1], Pa[:, 2]
                M1, M2 = (A_ + B_) / 2, (B_ + C_) / 2
                sa = (np.linalg.norm(A_ - M2, axis=1) <= np.linalg.norm(M1 - C_, axis=1))[:, None]
            outs = {}
            for key, a in att.items():
                A_, B_, C_ = a[:, 0], a[:, 1], a[:, 2]
                M1, M2, M3 = (A_ + B_) / 2, (B_ + C_) / 2, (C_ + A_) / 2
                if case == 1:
                    tris = [(A_, M1, C_), (M1, B_, C_)]
                elif case == 2:
                    t2 = tuple(np.where(sa, x, y) for x, y in zip((A_, M1, M2), (A_, M1, C_)))
                    t3 = tuple(np.where(sa, x, y) for x, y in zip((A_, M2, C_), (M1, M2, C_)))
                    tris = [(M1, B_, M2), t2, t3]
                else:
                    tris = [(A_, M1, M3), (M1, B_, M2), (M3, M2, C_), (M1, M2, M3)]
                outs[key] = tris
            for t in range(len(outs['P'])):
                parts.append(dict(P=np.stack(outs['P'][t], 1), UV=np.stack(outs['UV'][t], 1),
                                  C=None if s['C'] is None else np.stack(outs['C'][t], 1), mat=s['mat'][idx]))
        s = L.cat(*parts)
    return s


# ------------------------------------------------------------------ ambient occlusion
def hemisphere(n):
    i = (np.arange(n) + .5) / n
    r = np.sqrt(i)
    th = np.arange(n) * 2.39996
    return np.stack([r * np.cos(th), r * np.sin(th), np.sqrt(1 - i)], 1)


def ambient(bvh, pts, nrm, dist=2.4, n=10, rng=RNG):
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


def bake_ao(s, bvh, strength=.6, dist=2.4, n=10):
    """Multiply per-corner light by an AO term evaluated once per unique (position, normal)."""
    P = s['P']
    N = tri_normals(P)
    key = np.concatenate([np.round(P.reshape(-1, 3) * 400), np.round(np.repeat(N, 3, 0) * 3)], 1).astype(np.int64)
    _, first, inv = np.unique(key, axis=0, return_index=True, return_inverse=True)
    inv = inv.ravel()
    pts = P.reshape(-1, 3)[first]
    nrm = np.repeat(N, 3, 0)[first]
    ao = ambient(bvh, pts, nrm, dist=dist, n=n)
    f = (1 - strength) + strength * ao
    s = dict(s)
    s['C'] = s['C'] * f[inv].reshape(-1, 3, 1)
    return s, len(first)


# ------------------------------------------------------------------ periodic textures
def periodic_voronoi_tile(n=512, size_m=2.0, stone_m=.62, seed=5, palettes=None, joint_m=.07, moss=0.0):
    """Seamlessly tiling irregular paving: n px = size_m metres.  Returns (n,n,4) float32 (top-down) and cell stats."""
    r = np.random.default_rng(seed)
    k = int(round(size_m / stone_m))
    gx = gy = max(k, 2)
    sites = []
    for j in range(gy * 2):                       # hex-ish lattice, 2 sites per cell cell-row for 62 cm stones
        for i in range(gx):
            sites.append(((i + .5 * (j % 2) + r.uniform(-.22, .22)) / gx, (j + .5 + r.uniform(-.22, .22)) / (gy * 2)))
    S = np.array(sites)
    yy, xx = np.mgrid[0:n, 0:n]
    px = np.stack([(xx + .5) / n, (yy + .5) / n], -1)
    best = np.full((n, n), 9.0)
    second = np.full((n, n), 9.0)
    arg = np.zeros((n, n), int)
    wts = r.uniform(-.012, .012, len(S))                           # power-diagram weights: unequal stone sizes
    spacing = 1 / gx * .9
    for si, s_ in enumerate(S):
        for ox in (-1, 0, 1):
            for oy in (-1, 0, 1):
                d = (np.square(px[..., 0] - (s_[0] + ox)) + np.square(px[..., 1] - (s_[1] + oy)) - wts[si]) / (2 * spacing)
                better = d < best
                second = np.where(better, best, np.minimum(second, d))
                arg = np.where(better, si, arg)
                best = np.where(better, d, best)
    gap = (second - best) * size_m                # ~ distance to the cell border * 2, metres
    base_cols = palettes or [(.34, .33, .36), (.40, .37, .36), (.30, .31, .35), (.36, .35, .33)]
    cell_col = np.array([base_cols[r.integers(len(base_cols))] for _ in S]) * r.uniform(.86, 1.12, (len(S), 1))
    col = cell_col[arg]
    n1, n2 = _tile_noise(n, 5, seed + 1), _tile_noise(n, 15, seed + 2)
    col = col * (.80 + .26 * n1[..., None] + .14 * n2[..., None] - .07)
    edge = smoothstep(joint_m * .6, joint_m * 1.9, gap)           # 0 in the joint, 1 on the stone
    bevel = 1 - .22 * (1 - smoothstep(joint_m * 1.9, joint_m * 4.5, gap))
    col = col * (bevel * (.38 + .62 * edge))[..., None]
    if moss > 0:
        m = smoothstep(.45, .7, _tile_noise(n, 6, seed + 3) * .6 + _tile_noise(n, 18, seed + 4) * .4) * moss
        col = col * (1 - m[..., None]) + np.array((.20, .36, .13)) * (.7 + .5 * n2[..., None]) * m[..., None]
    t = np.ones((n, n, 4), np.float32)
    t[..., :3] = np.clip(col, 0, 1)
    return t


def _tile_noise(n, cells, seed):
    a = np.random.default_rng(seed).random((cells, cells))
    yy, xx = np.mgrid[0:n, 0:n]
    xs, ys = xx / n * cells, yy / n * cells
    x0, y0 = xs.astype(int) % cells, ys.astype(int) % cells
    fx, fy = xs - np.floor(xs), ys - np.floor(ys)
    fx, fy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
    x1, y1 = (x0 + 1) % cells, (y0 + 1) % cells
    return (a[y0, x0] * (1 - fx) + a[y0, x1] * fx) * (1 - fy) + (a[y1, x0] * (1 - fx) + a[y1, x1] * fx) * fy


def tile_noise(n, cells, seed):
    return _tile_noise(n, cells, seed)


# ------------------------------------------------------------------ objects + export
def make_object_rgba(name, s, img, alpha=None, glow_mats=(), interp='Linear', mat_name=None):
    """Soup -> Blender mesh with UVs and a float COLOR_0 (RGB = light, A = alpha or glow mask).
    alpha: optional (n,3) per-corner alpha; otherwise 1 on glow_mats triangles and 0 elsewhere."""
    keep = np.ones(len(s['P']), bool)
    s = L.drop_degenerate(s)
    n = len(s['P'])
    P = s['P'].reshape(-1, 3)
    key = np.round(P * 1e5).astype(np.int64)
    _, idx, inv = np.unique(key, axis=0, return_index=True, return_inverse=True)
    inv = inv.ravel()
    faces = inv.reshape(n, 3)
    ok = (faces[:, 0] != faces[:, 1]) & (faces[:, 1] != faces[:, 2]) & (faces[:, 0] != faces[:, 2])
    # coincident duplicate triangles (r001 source overlaps) are dropped: they z-fight and Blender flags the mesh invalid
    _, first = np.unique(np.sort(faces, axis=1), axis=0, return_index=True)
    dup = np.ones(len(faces), bool)
    dup[first] = False
    ok &= ~dup
    n_dup = int(dup.sum())
    if not ok.all():
        s = L.sub(s, ok)
        if alpha is not None:
            alpha = alpha[ok]
        faces = faces[ok]
        n = len(faces)
    print(f'DEDUP {name}: removed {n_dup} duplicate triangles')
    me = bpy.data.meshes.new(name)
    me.from_pydata(P[idx].tolist(), [], faces.tolist())
    me.update()
    bad = me.validate(verbose=True)
    print(f'VALIDATE {name}: changed={bad} polygons={len(me.polygons)}/{n}')
    if len(me.polygons) != n:
        raise RuntimeError(f'{name}: face count changed {n} -> {len(me.polygons)}')
    uvl = me.uv_layers.new(name='UVMap')
    uvl.data.foreach_set('uv', s['UV'].reshape(-1).astype(np.float32))
    ca = me.color_attributes.new('Col', 'FLOAT_COLOR', 'CORNER')
    C = s['C'] if s['C'] is not None else np.ones((n, 3, 3))
    rgb = L.srgb2lin(np.clip(C, 0, 1)).reshape(-1, 3)
    if alpha is None:
        a = np.repeat(np.isin(s['mat'], glow_mats).astype(np.float32), 3)
    else:
        a = np.asarray(alpha, np.float32).reshape(-1)
    c4 = np.concatenate([rgb, a[:, None]], 1).astype(np.float32)
    ca.data.foreach_set('color', c4.ravel())
    me.color_attributes.active_color = ca
    me.color_attributes.render_color_index = 0
    me.shade_flat()
    mat = bpy.data.materials.new(mat_name or ('M_' + (name[3:] if name.startswith('SM_') else name)))
    nt = mat.node_tree
    bsdf = nt.nodes.get('Principled BSDF')
    bsdf.inputs['Roughness'].default_value = 1.0
    bsdf.inputs['Metallic'].default_value = 0.0
    tn = nt.nodes.new('ShaderNodeTexImage')
    tn.image = img
    tn.interpolation = interp                      # r002: Linear, never Closest (trilinear + anisotropic in Unreal)
    nt.links.new(tn.outputs['Color'], bsdf.inputs['Base Color'])
    mat.use_backface_culling = True
    me.materials.append(mat)
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    return ob, int(round(a.reshape(-1, 3)[:, 0].sum() / 3 * 1.0)) if alpha is None else 0


def export_glb(ob, path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    for o in bpy.context.scene.objects:
        o.select_set(False)
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    bpy.ops.export_scene.gltf(
        filepath=path, export_format='GLB', use_selection=True, export_yup=True, export_apply=True,
        export_texcoords=True, export_normals=True, export_tangents=False, export_materials='EXPORT',
        export_image_format='AUTO', export_vertex_color='ACTIVE', export_all_vertex_colors=False,
        export_active_vertex_color_when_no_material=True, export_animations=False, export_skins=False,
        export_morph=False, export_cameras=False, export_lights=False)
    return path


def glb_stats(path):
    """Read an exported glb back (stdlib): triangles, vertices, bbox in Blender axes, image size, COLOR_0 type, sampler."""
    data = open(path, 'rb').read()
    jl = struct.unpack('<I', data[12:16])[0]
    js = json.loads(data[20:20 + jl])
    bin_off = 20 + jl + 8
    tris = verts = 0
    lo, hi = [1e9] * 3, [-1e9] * 3
    col = None
    for m in js['meshes']:
        for pr in m['primitives']:
            tris += js['accessors'][pr['indices']]['count'] // 3
            a = js['accessors'][pr['attributes']['POSITION']]
            verts += a['count']
            lo = [min(x, y) for x, y in zip(lo, a['min'])]
            hi = [max(x, y) for x, y in zip(hi, a['max'])]
            if 'COLOR_0' in pr['attributes']:
                ca = js['accessors'][pr['attributes']['COLOR_0']]
                col = (ca['type'], ca['componentType'], bool(ca.get('normalized')))
    img = None
    for im in js.get('images', []):
        bv = js['bufferViews'][im['bufferView']]
        png = data[bin_off + bv.get('byteOffset', 0): bin_off + bv.get('byteOffset', 0) + bv['byteLength']]
        img = list(struct.unpack('>II', png[16:24]))
    samp = [(s.get('magFilter'), s.get('minFilter')) for s in js.get('samplers', [])]
    bmin = [lo[0], -hi[2], lo[1]]
    bmax = [hi[0], -lo[2], hi[1]]
    return dict(triangles=tris, vertices=verts, bbox_min_m=[round(v, 4) for v in bmin], bbox_max_m=[round(v, 4) for v in bmax],
                size_m=[round(b - a, 4) for a, b in zip(bmin, bmax)], image_px=img, color0=col, samplers=samp,
                materials=len(js.get('materials', [])), bytes=len(data), extensions=js.get('extensionsUsed', []))
