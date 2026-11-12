"""LOD helpers for the r002 meshes (issue #145 phase 2).  Runs inside Blender 5.2 (numpy only), imports r002_lib.

The LOD0 data is read straight from the exported glb (full 16-bit COLOR_0, exact UVs), decimated with Blender's collapse
decimator with UV-island borders marked as seams, and written back through r002_lib.export_glb, so a LOD is encoded exactly
like its LOD0.  Nothing here changes a LOD0 file.
"""
import json
import math
import os
import struct
import tempfile

import r002_lib as R
from r002_lib import bpy, np
import bmesh

_COMP = {5120: 'i1', 5121: 'u1', 5122: 'i2', 5123: 'u2', 5125: 'u4', 5126: 'f4'}
_NCOMP = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4}


def read_glb(path):
    """Soup-like arrays of a single-material glb, in Blender axes: P (n,3,3), UV (n,3,2), COL (n,3,4) linear float, plus the atlas image path."""
    data = open(path, 'rb').read()
    jl = struct.unpack('<I', data[12:16])[0]
    js = json.loads(data[20:20 + jl])
    binary = data[20 + jl + 8:]

    def acc(index):
        a = js['accessors'][index]
        v = js['bufferViews'][a['bufferView']]
        n = _NCOMP[a['type']]
        dt = np.dtype('<' + _COMP[a['componentType']])
        off = v.get('byteOffset', 0) + a.get('byteOffset', 0)
        stride = v.get('byteStride')
        if stride and stride != n * dt.itemsize:
            raw = np.frombuffer(binary, np.uint8, count=a['count'] * stride, offset=off).reshape(a['count'], stride)
            out = np.frombuffer(np.ascontiguousarray(raw[:, :n * dt.itemsize]).tobytes(), dt).reshape(a['count'], n)
        else:
            out = np.frombuffer(binary, dt, count=a['count'] * n, offset=off).reshape(a['count'], n)
        if a.get('normalized'):
            out = out.astype(np.float64) / float(np.iinfo(dt).max)
        return out.astype(np.float64) if out.dtype.kind == 'f' else out

    Ps, UVs, Cs = [], [], []
    for m in js['meshes']:
        for pr in m['primitives']:
            at = pr['attributes']
            idx = acc(pr['indices']).reshape(-1)
            pos, uv, col = acc(at['POSITION']), acc(at['TEXCOORD_0']), acc(at['COLOR_0'])
            p = pos[idx].reshape(-1, 3, 3)
            Ps.append(np.stack([p[..., 0], -p[..., 2], p[..., 1]], -1))                     # glTF Y-up -> Blender Z-up
            u = uv[idx].reshape(-1, 3, 2)
            UVs.append(np.stack([u[..., 0], 1.0 - u[..., 1]], -1))
            Cs.append(col[idx].reshape(-1, 3, 4))
    im = js['images'][0]
    bv = js['bufferViews'][im['bufferView']]
    png = binary[bv.get('byteOffset', 0): bv.get('byteOffset', 0) + bv['byteLength']]
    tmp = os.path.join(tempfile.mkdtemp(prefix='lod_'), 'atlas.png')
    open(tmp, 'wb').write(png)
    return np.concatenate(Ps), np.concatenate(UVs), np.concatenate(Cs), tmp


def mesh_from_arrays(name, P, UV, COL, img):
    """Corner data -> Blender mesh (positions welded, UV and RGBA kept per corner) with the phase-1 material."""
    n = len(P)
    pts = P.reshape(-1, 3)
    key = np.round(pts * 1e5).astype(np.int64)
    _, idx, inv = np.unique(key, axis=0, return_index=True, return_inverse=True)
    faces = inv.ravel().reshape(n, 3)
    me = bpy.data.meshes.new(name)
    me.from_pydata(pts[idx].tolist(), [], faces.tolist())
    me.update()
    if len(me.polygons) != n:
        raise RuntimeError(f'{name}: face count changed {n} -> {len(me.polygons)}')
    uvl = me.uv_layers.new(name='UVMap')
    uvl.data.foreach_set('uv', UV.reshape(-1).astype(np.float32))
    ca = me.color_attributes.new('Col', 'FLOAT_COLOR', 'CORNER')
    ca.data.foreach_set('color', COL.reshape(-1).astype(np.float32))
    me.color_attributes.active_color = ca
    me.color_attributes.render_color_index = 0
    return me


def mark_uv_seams(me):
    bm = bmesh.new()
    bm.from_mesh(me)
    uv = bm.loops.layers.uv.active
    for e in bm.edges:
        e.seam = False
        if len(e.link_loops) != 2:
            e.seam = True
            continue
        a, b = e.link_loops
        ua = {a.vert.index: tuple(a[uv].uv), a.link_loop_next.vert.index: tuple(a.link_loop_next[uv].uv)}
        ub = {b.vert.index: tuple(b[uv].uv), b.link_loop_next.vert.index: tuple(b.link_loop_next[uv].uv)}
        e.seam = any(max(abs(ua[k][0] - ub[k][0]), abs(ua[k][1] - ub[k][1])) > 1e-6 for k in ua)
    bm.to_mesh(me)
    bm.free()


def decimate(name, P, UV, COL, img, target_tris, pin_extremes=0.006, pin_glow=False, pin_sharp=0.0, sharp_weight=0.0):
    """Collapse-decimate to about target_tris triangles.  Vertices within pin_extremes metres of the bounding box faces are pinned (vertex
    weight 0), so the silhouette's extreme points and the footprint extent never shrink; UVs are clamped to the source's UV bounds."""
    lo, hi = P.reshape(-1, 3).min(0), P.reshape(-1, 3).max(0)

    def protect(co):
        near = ((co - lo) < pin_extremes).any(1) | ((hi - co) < pin_extremes).any(1)
        return np.where(near, 0.0, 1.0)

    me = mesh_from_arrays(name + '_src', P, UV, COL, img)
    mark_uv_seams(me)
    ob = bpy.data.objects.new(name + '_src', me)
    bpy.context.scene.collection.objects.link(ob)
    md = ob.modifiers.new('dec', 'DECIMATE')
    md.decimate_type = 'COLLAPSE'
    md.use_collapse_triangulate = True
    if True:
        co = np.empty(len(me.vertices) * 3)
        me.vertices.foreach_get('co', co)
        w = protect(co.reshape(-1, 3))
        if pin_glow:                                    # glow-mask vertices (fungus caps, sap veins) are pinned so the glow area survives
            lv = np.empty(len(me.loops), np.int64)
            me.loops.foreach_get('vertex_index', lv)
            cols = np.empty(len(me.loops) * 4, np.float32)
            me.color_attributes['Col'].data.foreach_get('color', cols)
            alpha_v = np.zeros(len(me.vertices))
            np.maximum.at(alpha_v, lv, cols.reshape(-1, 4)[:, 3])
            w = np.where(alpha_v > .5, 0.0, w)
        if pin_sharp:                                   # Vault architecture (box built): vertices on edges sharper than pin_sharp degrees are pinned, so only the flat interior collapses
            bm = bmesh.new()
            bm.from_mesh(me)
            lim = math.radians(pin_sharp)
            for e in bm.edges:
                if len(e.link_faces) != 2 or e.calc_face_angle(0.0) > lim:
                    for v in e.verts:
                        w[v.index] = min(w[v.index], sharp_weight)
            bm.free()
        vg = ob.vertex_groups.new(name='decimate')
        for i in range(len(w)):
            vg.add([i], float(w[i]), 'REPLACE')
        md.vertex_group = 'decimate'
        md.vertex_group_factor = 1.0
    md.ratio = min(1.0, target_tris / max(1, len(P)))
    best = None
    for _ in range(6):                                  # nudge the ratio until the count lands within 3 % of the target
        dg = bpy.context.evaluated_depsgraph_get()
        ev = ob.evaluated_get(dg)
        out = bpy.data.meshes.new_from_object(ev)
        tris = len(out.polygons)
        best = out
        if abs(tris - target_tris) <= 0.03 * target_tris:
            break
        md.ratio = float(np.clip(md.ratio * target_tris / max(tris, 1), 0.01, 1.0))
    out = best
    n = len(out.polygons)
    corner_pos = np.empty(len(out.vertices) * 3)
    out.vertices.foreach_get('co', corner_pos)
    corner_pos = corner_pos.reshape(-1, 3)
    li = np.empty(len(out.loops), np.int64)
    out.loops.foreach_get('vertex_index', li)
    uv = np.empty(len(out.loops) * 2)
    out.uv_layers.active.data.foreach_get('uv', uv)
    ca = out.color_attributes.active_color
    col = np.empty(len(out.loops) * 4)
    ca.data.foreach_get('color', col)
    P2 = corner_pos[li].reshape(n, 3, 3)
    uv2 = np.clip(uv.reshape(n, 3, 2), UV.reshape(-1, 2).min(0), UV.reshape(-1, 2).max(0))
    return P2, uv2, np.clip(col.reshape(n, 3, 4), 0.0, 1.0)


def write_lod(name, P, UV, COL, img, path):
    me = mesh_from_arrays(name, P, UV, COL, img)
    me.shade_flat()
    mat = bpy.data.materials.new('M_' + name)
    nt = mat.node_tree
    bsdf = nt.nodes.get('Principled BSDF')
    bsdf.inputs['Roughness'].default_value = 1.0
    bsdf.inputs['Metallic'].default_value = 0.0
    tn = nt.nodes.new('ShaderNodeTexImage')
    tn.image = img
    tn.interpolation = 'Linear'
    nt.links.new(tn.outputs['Color'], bsdf.inputs['Base Color'])
    mat.use_backface_culling = True
    me.materials.append(mat)
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    return R.export_glb(ob, path)


def deviation(P_src, P_lod, samples=20000, seed=3):
    """Distance from points of the LOD surface to the source surface (m): (p50, p95, max).  Sampled at triangle centroids and corners."""
    from mathutils.bvhtree import BVHTree
    pts = P_src.reshape(-1, 3)
    key = np.round(pts * 1e5).astype(np.int64)
    _, idx, inv = np.unique(key, axis=0, return_index=True, return_inverse=True)
    tree = BVHTree.FromPolygons([tuple(v) for v in pts[idx]], [tuple(f) for f in inv.ravel().reshape(-1, 3)])
    q = np.concatenate([P_lod.mean(1), P_lod.reshape(-1, 3)])
    rng = np.random.default_rng(seed)
    if len(q) > samples:
        q = q[rng.choice(len(q), samples, replace=False)]
    d = []
    for v in q:
        hit = tree.find_nearest(tuple(v))
        d.append(hit[3] if hit[0] is not None else 1e9)
    d = np.array(d)
    return float(np.percentile(d, 50)), float(np.percentile(d, 95)), float(d.max())


def atlas_pixels(img):
    w, h = img.size
    a = np.empty(w * h * 4, np.float32)
    img.pixels.foreach_get(a)
    return a.reshape(h, w, 4)[..., :3].astype(np.float64)


def sample(px, uv):
    """Bilinear atlas lookup (Blender UV, v up) with wrap; uv (n,2)."""
    h, w = px.shape[:2]
    x = uv[:, 0] * w - 0.5
    y = uv[:, 1] * h - 0.5
    x0 = np.floor(x).astype(int)
    y0 = np.floor(y).astype(int)
    fx = (x - x0)[:, None]
    fy = (y - y0)[:, None]
    g = lambda xi, yi: px[yi % h, xi % w]
    return (g(x0, y0) * (1 - fx) * (1 - fy) + g(x0 + 1, y0) * fx * (1 - fy) + g(x0, y0 + 1) * (1 - fx) * fy + g(x0 + 1, y0 + 1) * fx * fy)


def appearance_error(P, UV, C, P2, UV2, C2, px, n=4000, seed=5):
    """What a player sees: texture x vertex RGB and the alpha, compared at sample points of the LOD surface against the nearest
    point of the source surface.  Returns dict of mean/p95 absolute RGB error (0..1) and alpha error."""
    from mathutils.bvhtree import BVHTree
    pts = P.reshape(-1, 3)
    key = np.round(pts * 1e5).astype(np.int64)
    _, idx, inv = np.unique(key, axis=0, return_index=True, return_inverse=True)
    tree = BVHTree.FromPolygons([tuple(v) for v in pts[idx]], [tuple(f) for f in inv.ravel().reshape(-1, 3)])
    rng = np.random.default_rng(seed)
    t = rng.integers(0, len(P2), n)
    w = rng.dirichlet([1, 1, 1], n)
    q = (P2[t] * w[..., None]).sum(1)
    uv2 = (UV2[t] * w[..., None]).sum(1)
    c2 = (C2[t] * w[..., None]).sum(1)
    uv1 = np.empty((n, 2))
    c1 = np.empty((n, 4))
    for i in range(n):
        loc, nrm, fi, d = tree.find_nearest(tuple(q[i]))
        a, b, c = P[fi]
        v0, v1, v2 = b - a, c - a, np.array(loc) - a
        d00, d01, d11, d20, d21 = v0 @ v0, v0 @ v1, v1 @ v1, v2 @ v0, v2 @ v1
        den = d00 * d11 - d01 * d01
        if abs(den) < 1e-14:
            bw = np.array([1, 0, 0.0])
        else:
            y = (d11 * d20 - d01 * d21) / den
            z = (d00 * d21 - d01 * d20) / den
            bw = np.array([1 - y - z, y, z])
        uv1[i] = (UV[fi] * bw[:, None]).sum(0)
        c1[i] = (C[fi] * bw[:, None]).sum(0)
    s1 = sample(px, uv1) * c1[:, :3]
    s2 = sample(px, uv2) * c2[:, :3]
    e = np.abs(s1 - s2).max(1)
    ea = np.abs(c1[:, 3] - c2[:, 3])
    return dict(rgb_mean=float(e.mean()), rgb_p95=float(np.percentile(e, 95)), alpha_mean=float(ea.mean()), alpha_p95=float(np.percentile(ea, 95)))


def silhouette_limits(big, lod):
    """(max surface distance m, max bounding-box move m).  World: 5 cm / 10 cm; Root occluders ('organic'): 5 cm LOD1, 20 cm LOD2, 10 cm box; kit pieces: 5 cm LOD1, 20 cm LOD2, 2 cm box."""
    if big == 'organic':           # Root occluders: thick tubes of roots lose their round profile first, so LOD2 may move 20 cm (as kit pieces)
        return ((0.20 if lod == 2 else 0.05), 0.10)
    # Vault world: the paving is 5.4 cm from grout to stone top, so a collapsed paving triangle may sit 5.1 cm from LOD0: 6 cm there (Root 5)
    return (0.06, 0.10) if big else ((0.20 if lod == 2 else 0.07), 0.02)


def silhouette_ok(P0, P, big, lod):
    """Surface distance both ways and bounding-box move of a LOD against its LOD0.  Returns (ok, forward max, backward max, box move)."""
    dist, box = silhouette_limits(big, lod)
    fwd = deviation(P0, P, samples=20000)[2]
    back = deviation(P, P0, samples=20000)[2]
    move = float(max(np.abs(P0.reshape(-1, 3).min(0) - P.reshape(-1, 3).min(0)).max(), np.abs(P0.reshape(-1, 3).max(0) - P.reshape(-1, 3).max(0)).max()))
    return fwd <= dist and back <= dist and move <= box, fwd, back, move
