# Recipes run from the repository root with Blender (`blender -b --python <script>.py`).
# Working files live under .local/ (override with THEME_LOCAL); exports go to .local/theme-build/.

"""Shared helpers for the theme asset build. Runs inside Blender 5.2 (numpy only, no Pillow).

Conventions
- A "soup" is a dict of numpy arrays: P (n,3,3) positions, UV (n,3,2), C (n,3,3) light or None,
  mat (n,) material names. Triangles only.
- Texture arrays are float32 RGBA, stored top-down (row 0 = image top). The original raster
  pipeline samples row = frac(v) * h from the top, so source UVs follow that convention.
"""
import math
import os

import bpy
import numpy as np

LOCAL = __import__('os').environ.get('THEME_LOCAL', __import__('os').path.join(__import__('os').getcwd(), '.local'))
SRC = LOCAL + "/ps1-src/out/worlds/source"
BUILD = LOCAL + "/theme-build"
WORK = LOCAL + "/theme-tools/work"
os.makedirs(BUILD, exist_ok=True)
os.makedirs(WORK, exist_ok=True)


# ------------------------------------------------------------------ source access
def open_blend(name):
    bpy.ops.wm.open_mainfile(filepath=f"{SRC}/{name}.blend")


def img_array(img):
    w, h = img.size
    a = np.empty(w * h * 4, np.float32)
    img.pixels.foreach_get(a)
    return a.reshape(h, w, 4)[::-1].copy()


def swatch(rgb, n=8):
    t = np.ones((n, n, 4), np.float32)
    t[..., :3] = rgb
    return t


def lin2srgb(c):
    c = np.clip(c, 0, None)
    return np.where(c <= .0031308, c * 12.92, 1.055 * np.power(c, 1 / 2.4) - .055)


def srgb2lin(c):
    c = np.clip(c, 0, None)
    return np.where(c <= .04045, c / 12.92, np.power((c + .055) / 1.055, 2.4))


def material_texture(mat):
    """Texture array for a source material; solid swatch when it has no image."""
    if mat.node_tree:
        for n in mat.node_tree.nodes:
            if n.type == "TEX_IMAGE" and n.image:
                return img_array(n.image)
        for n in mat.node_tree.nodes:
            if n.type == "EMISSION":
                return swatch(lin2srgb(np.array(n.inputs[0].default_value[:3])))
    return swatch((.5, .5, .5))


def soup_from_object(ob, light=None):
    """Triangles in world space. light(c_raw)->display light, for baked colour attributes."""
    me = ob.data
    me.calc_loop_triangles()
    lt = me.loop_triangles
    n = len(lt)
    loops = np.empty(n * 3, np.int64)
    lt.foreach_get("loops", loops)
    vidx = np.empty(n * 3, np.int64)
    lt.foreach_get("vertices", vidx)
    mi = np.empty(n, np.int64)
    lt.foreach_get("material_index", mi)
    co = np.empty(len(me.vertices) * 3)
    me.vertices.foreach_get("co", co)
    co = co.reshape(-1, 3)
    M = np.array(ob.matrix_world)
    P = (co[vidx] @ M[:3, :3].T + M[:3, 3]).reshape(n, 3, 3)
    uvd = np.empty(len(me.loops) * 2, np.float64)
    me.uv_layers.active.data.foreach_get("uv", uvd)
    UV = uvd.reshape(-1, 2)[loops].reshape(n, 3, 2)
    C = None
    if light is not None and len(me.color_attributes):
        ca = me.color_attributes[0]
        c = np.empty(len(ca.data) * 4, np.float32)
        ca.data.foreach_get("color", c)
        c = c.reshape(-1, 4)[:, :3]
        idx = vidx if ca.domain == "POINT" else loops
        C = light(c[idx]).reshape(n, 3, 3)
    names = [m.name if m else "none" for m in me.materials] or ["none"]
    mat = np.array([names[i] for i in mi], dtype=object)
    texs = {m.name: material_texture(m) for m in me.materials if m}
    return dict(P=P, UV=UV, C=C, mat=mat), texs


def cat(*soups):
    out = {}
    for k in ("P", "UV", "mat"):
        out[k] = np.concatenate([s[k] for s in soups])
    if all(s["C"] is not None for s in soups):
        out["C"] = np.concatenate([s["C"] for s in soups])
    else:
        out["C"] = None
    return out


def sub(s, mask):
    return dict(P=s["P"][mask], UV=s["UV"][mask], C=None if s["C"] is None else s["C"][mask], mat=s["mat"][mask])


def flipped(s, eps=0.0):
    """Back faces for formerly two-sided parts: reversed winding, nudged eps along the new normal."""
    o = [0, 2, 1]
    P = s["P"][:, o].copy()
    if eps:
        n = np.cross(P[:, 1] - P[:, 0], P[:, 2] - P[:, 0])
        n /= np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-12)
        P += n[:, None, :] * eps
    return dict(P=P, UV=s["UV"][:, o], C=None if s["C"] is None else s["C"][:, o], mat=s["mat"].copy())


def xform(s, M):
    M = np.asarray(M, float)
    P = s["P"].reshape(-1, 3) @ M[:3, :3].T + M[:3, 3]
    out = dict(s)
    out["P"] = P.reshape(-1, 3, 3)
    if np.linalg.det(M[:3, :3]) < 0:
        out = flipped(out)
    return out


def Rz(deg):
    c, s = math.cos(math.radians(deg)), math.sin(math.radians(deg))
    M = np.eye(4)
    M[:2, :2] = ((c, -s), (s, c))
    return M


def T(x=0, y=0, z=0):
    M = np.eye(4)
    M[:3, 3] = (x, y, z)
    return M


def S(x, y=None, z=None):
    return np.diag([x, x if y is None else y, x if z is None else z, 1.0])


def knee(L, k=.8):
    """Compress a 0..2 display-light multiplier into 0..1 with a soft shoulder."""
    L = np.clip(L, 0, None)
    return np.where(L < k, L, k + (1 - k) * (1 - np.exp(-(L - k) / (1 - k))))


def bake_light(exposure, gamma):
    """Same transfer as world_core.World.bake: clip((c*exposure))**gamma, clipped to 2."""
    def f(c):
        return np.clip(np.clip(np.maximum(c, 0) * exposure, 0, 8) ** gamma, 0, 2)
    return f


# ------------------------------------------------------------------ geometry clipping
def _clip_poly(poly, axis, k, keep_less):
    """Sutherland-Hodgman against the line attr[axis] = k. poly: (m, D)."""
    out = []
    m = len(poly)
    for i in range(m):
        a, b = poly[i], poly[(i + 1) % m]
        da, db = a[axis] - k, b[axis] - k
        ina = da <= 0 if keep_less else da >= 0
        inb = db <= 0 if keep_less else db >= 0
        if ina:
            out.append(a)
        if ina != inb:
            t = da / (da - db)
            out.append(a + (b - a) * t)
    return out


def _fan(poly):
    return [(poly[0], poly[i], poly[i + 1]) for i in range(1, len(poly) - 1)]


def _pack_tri(s, i):
    cols = [s["P"][i], s["UV"][i]]
    if s["C"] is not None:
        cols.append(s["C"][i])
    return np.concatenate(cols, axis=1)  # (3, D)


def _unpack(tris, s_has_c, mats):
    A = np.array(tris, np.float64)
    out = dict(P=A[:, :, 0:3], UV=A[:, :, 3:5], C=A[:, :, 5:8] if s_has_c else None, mat=np.array(mats, dtype=object))
    return out


def split_uv_tiles(s, ru=None, rv=None, eps=1e-6):
    """Cut triangles so each piece spans at most (ru, rv) texture tiles (default 1x1)."""
    n = len(s["P"])
    ru = np.ones(n) if ru is None else np.asarray(ru, float)
    rv = np.ones(n) if rv is None else np.asarray(rv, float)
    uv = s["UV"]
    lo = np.floor(uv.min(1) + eps)
    hi = np.ceil(uv.max(1) - eps)
    single = (hi[:, 0] - lo[:, 0] <= ru) & (hi[:, 1] - lo[:, 1] <= rv)
    keep = sub(s, single)
    multi = np.nonzero(~single)[0]
    if not len(multi):
        return keep
    tris, mats = [], []
    for i in multi:
        polys = [list(_pack_tri(s, i))]
        for axis, r in ((3, ru[i]), (4, rv[i])):
            done = []
            while polys:
                p = polys.pop()
                vals = [q[axis] for q in p]
                a, b = math.floor(min(vals) + eps), math.ceil(max(vals) - eps)
                if b - a <= r:
                    done.append(p)
                    continue
                k = a + int(r)
                for side in (True, False):
                    q = _clip_poly(p, axis, k, side)
                    if len(q) >= 3:
                        polys.append(q)
            polys = done
        for p in polys:
            for t in _fan(p):
                tris.append(t)
                mats.append(s["mat"][i])
    extra = _unpack(tris, s["C"] is not None, mats)
    extra = drop_degenerate(extra)
    return cat(keep, extra) if len(extra["P"]) else keep


def clip_rect(s, x0, x1, y0, y1, keep_inside=False):
    """Clip triangles against an axis-aligned XY rectangle; keep the outside (default) or inside."""
    P = s["P"]
    inside_all = np.all((P[:, :, 0] >= x0) & (P[:, :, 0] <= x1) & (P[:, :, 1] >= y0) & (P[:, :, 1] <= y1), axis=1)
    out_any_side = (np.all(P[:, :, 0] <= x0, 1) | np.all(P[:, :, 0] >= x1, 1) |
                    np.all(P[:, :, 1] <= y0, 1) | np.all(P[:, :, 1] >= y1, 1))
    straddle = ~inside_all & ~out_any_side
    if keep_inside:
        base = sub(s, inside_all)
    else:
        base = sub(s, out_any_side)
    tris, mats = [], []
    for i in np.nonzero(straddle)[0]:
        poly = list(_pack_tri(s, i))
        if keep_inside:
            for axis, k, less in ((0, x0, False), (0, x1, True), (1, y0, False), (1, y1, True)):
                poly = _clip_poly(poly, axis, k, less)
                if len(poly) < 3:
                    break
            pieces = [poly] if len(poly) >= 3 else []
        else:
            pieces = []
            rest = poly
            for axis, k, less in ((0, x0, True), (0, x1, False), (1, y0, True), (1, y1, False)):
                outp = _clip_poly(rest, axis, k, less)
                if len(outp) >= 3:
                    pieces.append(outp)
                rest = _clip_poly(rest, axis, k, not less)
                if len(rest) < 3:
                    break
        for p in pieces:
            for t in _fan(p):
                tris.append(t)
                mats.append(s["mat"][i])
    if tris:
        extra = drop_degenerate(_unpack(tris, s["C"] is not None, mats))
        return cat(base, extra)
    return base


def drop_degenerate(s, tol=1e-10):
    P = s["P"]
    a = np.linalg.norm(np.cross(P[:, 1] - P[:, 0], P[:, 2] - P[:, 0]), axis=1)
    return sub(s, a > tol)


def components(s, scale=500):
    """Connected components by shared (rounded) vertex position."""
    key = np.round(s["P"].reshape(-1, 3) * scale).astype(np.int64)
    _, inv = np.unique(key, axis=0, return_inverse=True)
    inv = inv.ravel()
    n = len(s["P"])
    parent = np.arange(inv.max() + 1)

    def find(x):
        root = x
        while parent[root] != root:
            root = parent[root]
        while parent[x] != root:
            parent[x], x = root, parent[x]
        return root

    tv = inv.reshape(n, 3)
    for a, b, c in tv:
        ra, rb, rc = find(a), find(b), find(c)
        parent[rb] = ra
        parent[find(rc)] = ra
    roots = np.array([find(v) for v in tv[:, 0]])
    _, comp = np.unique(roots, return_inverse=True)
    return comp.ravel()


# ------------------------------------------------------------------ atlas
def build_atlas(s, texs, size=1024, pad=4, tints=None, solid=(), downscale=(), repeat=(), bg=(.06, .05, .05)):
    """Pack every used material texture into one atlas and remap UVs (top-down source convention).
    solid: material names that get a flat colour (UVs collapse to the swatch centre).
    downscale: {name: factor} integer box downsample before packing.
    repeat: {name: (ru, rv)} store the tile repeated so triangles spanning several tiles need fewer cuts."""
    tints = tints or {}
    downscale = dict(downscale) if downscale else {}
    repeat = dict(repeat) if repeat else {}
    used = list(dict.fromkeys(s["mat"].tolist()))
    texs = {k: texs[k] for k in used}
    base = {}
    for k in used:
        t = texs[k].copy()
        f = downscale.get(k, 1)
        if f > 1:
            h, w = t.shape[:2]
            t = t[: h // f * f, : w // f * f].reshape(h // f, f, w // f, f, 4).mean((1, 3))
        if k in tints:
            t[..., :3] = np.clip(t[..., :3] * np.array(tints[k]), 0, 1)
        base[k] = t.shape[:2]
        ru, rv = repeat.get(k, (1, 1))
        texs[k] = np.tile(t, (rv, ru, 1))
    solid = set(solid) | {k for k in used if texs[k].shape[0] <= 8 and texs[k].shape[1] <= 8}
    tiled = np.array([m not in solid for m in s["mat"]])
    before = {k: int((s["mat"] == k).sum()) for k in used}
    if tiled.any():
        ts = sub(s, tiled)
        ru = np.array([repeat.get(m, (1, 1))[0] for m in ts["mat"]], float)
        rv = np.array([repeat.get(m, (1, 1))[1] for m in ts["mat"]], float)
        s = cat(split_uv_tiles(ts, ru, rv), sub(s, ~tiled))
    print("SPLIT", {k: (before[k], int((s["mat"] == k).sum())) for k in used})
    # skyline pack (bottom-left fit), largest area first
    items = sorted(used, key=lambda k: -(texs[k].shape[0] * texs[k].shape[1]))
    sky = np.zeros(size, int)
    place = {}
    for k in items:
        h, w = texs[k].shape[:2]
        W, H = w + 2 * pad, h + 2 * pad
        best = None
        for x in range(0, size - W + 1, 4):
            y = sky[x:x + W].max()
            if y + H <= size and (best is None or y < best[1]):
                best = (x, y)
        if best is None:
            raise ValueError(f"atlas overflow at {k}; add downscale")
        x, y = best
        sky[x:x + W] = y + H
        place[k] = (x + pad, y + pad, w, h)
    A = np.zeros((size, size, 4), np.float32)
    A[..., :3] = bg
    A[..., 3] = 1
    for k, (x0, y0, w, h) in place.items():
        A[y0 - pad:y0 + h + pad, x0 - pad:x0 + w + pad] = np.pad(texs[k], ((pad, pad), (pad, pad), (0, 0)), mode="wrap")
    A[..., 3] = 1
    UV = s["UV"]
    out = np.empty_like(UV)
    for k, (x0, y0, w, h) in place.items():
        m = s["mat"] == k
        if not m.any():
            continue
        if k in solid:
            out[m, :, 0] = (x0 + w / 2) / size
            out[m, :, 1] = 1 - (y0 + h / 2) / size
            continue
        bh, bw = base[k]
        ru, rv = repeat.get(k, (1, 1))
        uv = UV[m]
        org = np.floor(uv.min(1, keepdims=True) + 1e-6)
        f = uv - org
        f[..., 0] = np.clip(f[..., 0], 0, ru)
        f[..., 1] = np.clip(f[..., 1], 0, rv)
        out[m, :, 0] = (x0 + f[..., 0] * bw) / size
        out[m, :, 1] = 1 - (y0 + f[..., 1] * bh) / size
    s = dict(s)
    s["UV"] = out
    used_px = sum((w + 2 * pad) * (h + 2 * pad) for (_, _, w, h) in place.values())
    print(f"ATLAS {size}px, {len(place)} regions, fill {used_px / size / size:.0%}")
    return s, A, place


# ------------------------------------------------------------------ output
def new_image(name, A):
    """A: top-down float RGBA -> packed Blender image (PNG)."""
    h, w = A.shape[:2]
    if name in bpy.data.images:
        bpy.data.images.remove(bpy.data.images[name])
    img = bpy.data.images.new(name, w, h, alpha=False)
    img.pixels.foreach_set(np.ascontiguousarray(A[::-1]).ravel().astype(np.float32))
    img.file_format = "PNG"
    img.pack()
    return img


def save_png(A, path):
    img = new_image("tmp_save", A)
    img.filepath_raw = path
    img.file_format = "PNG"
    img.save()
    bpy.data.images.remove(img)


def make_object(name, s, A, tex_name=None, color=False):
    s = drop_degenerate(s)
    n = len(s["P"])
    P = s["P"].reshape(-1, 3)
    key = np.round(P * 1e5).astype(np.int64)
    uniq, idx, inv = np.unique(key, axis=0, return_index=True, return_inverse=True)
    inv = inv.ravel()
    verts = P[idx]
    faces = inv.reshape(n, 3)
    ok = (faces[:, 0] != faces[:, 1]) & (faces[:, 1] != faces[:, 2]) & (faces[:, 0] != faces[:, 2])
    if not ok.all():
        s = sub(s, ok)
        faces = faces[ok]
        n = len(faces)
    me = bpy.data.meshes.new(name)
    me.from_pydata(verts.tolist(), [], faces.tolist())
    me.update()
    if len(me.polygons) != n:
        raise RuntimeError(f"{name}: face count changed {n} -> {len(me.polygons)}")
    uvl = me.uv_layers.new(name="UVMap")
    uvl.data.foreach_set("uv", s["UV"].reshape(-1).astype(np.float32))
    if color and s["C"] is not None:
        ca = me.color_attributes.new("Col", "FLOAT_COLOR", "CORNER")
        c = srgb2lin(np.clip(s["C"], 0, 1)).reshape(-1, 3)
        c4 = np.concatenate([c, np.ones((len(c), 1))], 1).astype(np.float32)
        ca.data.foreach_set("color", c4.ravel())
        me.color_attributes.active_color = ca
        me.color_attributes.render_color_index = 0
    me.shade_flat()
    img = new_image(tex_name or ("T_" + name[3:] if name.startswith("SM_") else "T_" + name), A)
    mat = bpy.data.materials.new("M_" + (name[3:] if name.startswith("SM_") else name))
    nt = mat.node_tree
    bsdf = nt.nodes.get("Principled BSDF")
    bsdf.inputs["Roughness"].default_value = 1.0
    bsdf.inputs["Metallic"].default_value = 0.0
    tn = nt.nodes.new("ShaderNodeTexImage")
    tn.image = img
    tn.interpolation = "Closest"
    nt.links.new(tn.outputs["Color"], bsdf.inputs["Base Color"])
    mat.use_backface_culling = True
    me.materials.append(mat)
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    return ob


def export_glb(ob, color=False):
    for o in bpy.context.scene.objects:
        o.select_set(False)
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    path = f"{BUILD}/{ob.name}.glb"
    bpy.ops.export_scene.gltf(
        filepath=path, export_format="GLB", use_selection=True, export_yup=True, export_apply=True,
        export_texcoords=True, export_normals=True, export_tangents=False, export_materials="EXPORT",
        export_image_format="AUTO", export_vertex_color="ACTIVE" if color else "NONE",
        export_all_vertex_colors=False, export_active_vertex_color_when_no_material=color,
        export_animations=False, export_skins=False, export_morph=False, export_cameras=False, export_lights=False)
    P = np.array([v.co[:] for v in ob.data.vertices])
    print(f"EXPORT {ob.name}: {len(ob.data.polygons)} tris, bbox {P.min(0).round(3)} {P.max(0).round(3)}, colour={color}")
    return path


def fresh_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def bbox(s):
    P = s["P"].reshape(-1, 3)
    return P.min(0), P.max(0)


def clip_z(s, z0=0.0):
    """Keep the parts of triangles at or above z0."""
    P = s["P"]
    above = np.all(P[:, :, 2] >= z0, 1)
    below = np.all(P[:, :, 2] <= z0, 1)
    base = sub(s, above)
    tris, mats = [], []
    for i in np.nonzero(~above & ~below)[0]:
        q = _clip_poly(list(_pack_tri(s, i)), 2, z0, False)
        if len(q) >= 3:
            for t in _fan(q):
                tris.append(t)
                mats.append(s["mat"][i])
    if tris:
        return cat(base, drop_degenerate(_unpack(tris, s["C"] is not None, mats)))
    return base


def direct_uv(s):
    """Source top-down v -> Blender bottom-up v, for single-texture assets kept as-is."""
    s = dict(s)
    uv = s["UV"].copy()
    uv[..., 1] = 1 - uv[..., 1]
    s["UV"] = uv
    return s
