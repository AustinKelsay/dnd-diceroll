"""Shared kit for the #147 rigged character pilot. Runs inside Blender 5.2 (bpy + numpy).

Everything here is original and procedural: the shared 25-bone skeleton, the atlas painters, the mesh primitives with
per-vertex skin weights, vertex colour RGBA (glow mask in alpha), FK evaluation, per-frame ground contact, clip sampling and
the glTF exporter. Character recipes live in characters.py, clip poses in clips.py.

Conventions (see ../README.md): metres, +X forward, +Y left, Z up, foot pivot at the origin, 30 fps.
"""
import math
import os

import bpy
import numpy as np
from mathutils import Matrix, Vector

FPS = 30
ATLAS = 1024
TILE = 256
LOD = 0            # 0, 1, 2; characters read this through sd()/dn()/detail()


# ------------------------------------------------------------------ skeleton contract
def build_joints():
    """name -> (parent, head, tail) in armature space (= model space). Rest pose: arms hang 22 degrees out."""
    J = {}
    J['root'] = (None, (0, 0, 0), (0, 0, .15))
    J['pelvis'] = ('root', (0, 0, .95), (0, 0, 1.02))
    J['spine1'] = ('pelvis', (0, 0, 1.02), (0, 0, 1.16))
    J['spine2'] = ('spine1', (0, 0, 1.16), (0, 0, 1.30))
    J['chest'] = ('spine2', (0, 0, 1.30), (0, 0, 1.46))
    J['neck'] = ('chest', (0, 0, 1.46), (0, 0, 1.54))
    J['head'] = ('neck', (0, 0, 1.54), (0, 0, 1.76))
    for sg, s in ((1, 'L'), (-1, 'R')):
        d = np.array((0, sg * math.sin(math.radians(22)), -math.cos(math.radians(22))))
        sh = np.array((0, sg * .22, 1.42))
        el = sh + .30 * d
        wr = el + .27 * d
        J[f'clavicle.{s}'] = ('chest', (0, sg * .05, 1.42), (0, sg * .20, 1.44))
        J[f'upperarm.{s}'] = (f'clavicle.{s}', tuple(sh), tuple(el))
        J[f'forearm.{s}'] = (f'upperarm.{s}', tuple(el), tuple(wr))
        J[f'hand.{s}'] = (f'forearm.{s}', tuple(wr), tuple(wr + .10 * d))
        J[f'thigh.{s}'] = ('pelvis', (0, sg * .10, .93), (0, sg * .10, .50))
        J[f'shin.{s}'] = (f'thigh.{s}', (0, sg * .10, .50), (0, sg * .10, .09))
        J[f'foot.{s}'] = (f'shin.{s}', (0, sg * .10, .09), (.15, sg * .10, .04))
        J[f'toe.{s}'] = (f'foot.{s}', (.15, sg * .10, .04), (.24, sg * .10, .03))
    J['weapon_socket'] = ('hand.R', (0, -.47, .80), (.12, -.47, .86))
    J['shield_socket'] = ('hand.L', (0, .50, .82), (0, .58, .82))
    return J


JOINTS = build_joints()
BONE_ORDER = list(JOINTS)


def deg(x):
    return math.radians(x)


def euler_matrix(e):
    """Degrees about model X, Y, Z, applied X then Y then Z (Blender 'XYZ')."""
    rx, ry, rz = (deg(a) for a in e)
    cx, sx, cy, sy, cz, sz = math.cos(rx), math.sin(rx), math.cos(ry), math.sin(ry), math.cos(rz), math.sin(rz)
    X = np.array(((1, 0, 0), (0, cx, -sx), (0, sx, cx)))
    Y = np.array(((cy, 0, sy), (0, 1, 0), (-sy, 0, cy)))
    Z = np.array(((cz, -sz, 0), (sz, cz, 0), (0, 0, 1)))
    return Z @ Y @ X


class Rig:
    """Rest matrices read from the Blender armature, plus a numpy FK for posing, ground contact and held-item placement."""

    def __init__(self, ao):
        self.rest = {b.name: np.array(b.matrix_local) for b in ao.data.bones}
        self.length = {b.name: b.length for b in ao.data.bones}

    def basis(self, bone, rot=None, loc=None):
        Rr = self.rest[bone][:3, :3]
        B = np.eye(4)
        if rot is not None:
            B[:3, :3] = Rr.T @ euler_matrix(rot) @ Rr
        if loc is not None:
            B[:3, 3] = Rr.T @ np.asarray(loc, float)
        return B

    def fk(self, rot, loc):
        """rot: bone -> (rx, ry, rz) degrees about model axes in the parent's posed frame; loc: bone -> metres (model axes).
        Returns bone -> 4x4 posed armature-space matrix."""
        out = {}
        for n in BONE_ORDER:
            par = JOINTS[n][0]
            B = self.basis(n, rot.get(n), loc.get(n))
            if par is None:
                out[n] = self.rest[n] @ B
            else:
                out[n] = out[par] @ np.linalg.inv(self.rest[par]) @ self.rest[n] @ B
        return out

    def head(self, M, bone):
        return M[bone][:3, 3].copy()

    def tail(self, M, bone):
        return (M[bone] @ np.array((0, self.length[bone], 0, 1.0)))[:3]

    def to_bind(self, M, bone):
        """4x4 taking a point authored in the posed world to the bind pose for a part skinned rigidly to `bone`."""
        return self.rest[bone] @ np.linalg.inv(M[bone])


# ------------------------------------------------------------------ atlas
REGIONS = ['plate', 'trim', 'cloth', 'leather', 'blade', 'glow', 'rune', 'skin', 'bone', 'accent', 'dark', 'emblem', 'emblem2']
GLOW_REGIONS = {'glow', 'rune'}


def smooth_noise(rng, n, cells):
    a = rng.random((cells, cells))
    a = np.kron(a, np.ones((n // cells, n // cells)))
    f = np.fft.fft2(a)
    fx = np.fft.fftfreq(n)[:, None]
    fy = np.fft.fftfreq(n)[None, :]
    k = np.exp(-(fx ** 2 + fy ** 2) * (n / cells) ** 2 * 6)
    nz = np.real(np.fft.ifft2(f * k))
    return (nz - nz.min()) / (np.ptp(nz) + 1e-9)


def _line(t, p0, p1, w, col):
    n = t.shape[0]
    yy, xx = np.mgrid[0:n, 0:n]
    p0, p1 = np.array(p0, float) * n, np.array(p1, float) * n
    d = p1 - p0
    u = np.clip(((xx - p0[0]) * d[0] + (yy - p0[1]) * d[1]) / max(d @ d, 1e-9), 0, 1)
    dist = np.hypot(xx - (p0[0] + u * d[0]), yy - (p0[1] + u * d[1]))
    t[dist <= w * n, :3] = col


def paint(kind, rng, nz, **k):
    n = TILE
    yy, xx = np.mgrid[0:n, 0:n]
    t = np.ones((n, n, 4), np.float32)
    col = lambda c: np.array(c, np.float32)
    if kind == 'bands':           # r001 look: base plate with pale horizontal lines and scratches
        t[..., :3] = col(k['base']) * (.82 + .36 * nz[..., None])
        sp = k.get('spacing', 64)
        for y0 in range(0, n, sp):
            t[y0:y0 + 2, :, :3] = col(k['line']) * (.75 + .25 * nz[y0:y0 + 2, :, None])
            t[y0 + 2:y0 + 4, :, :3] *= .8
        for _ in range(k.get('scratches', 40)):
            x0, y0, ln = rng.integers(0, n), rng.integers(0, n), rng.integers(10, 40)
            t[y0 % n, x0:x0 + ln, :3] = np.minimum(1, t[y0 % n, x0:x0 + ln, :3] * 1.5)
    elif kind == 'scales':
        cell = k.get('cell', 16)
        c = ((xx // cell + yy // cell) % 2)[..., None]
        t[..., :3] = np.where(c == 0, col(k['a']), col(k['b'])) * (.88 + .24 * nz[..., None])
    elif kind == 'folds':
        fold = .5 + .5 * np.sin(xx * 2 * math.pi / k.get('period', 32) + 2.5 * nz)
        t[..., :3] = col(k['base']) * (.62 + .5 * fold[..., None]) * (.92 + .16 * nz[..., None])
        if 'hem' in k:
            t[-int(n * .10):, :, :3] = col(k['hem'])
            t[-int(n * .10) - 3:-int(n * .10), :, :3] = col(k.get('hem2', k['hem'])) * .8
    elif kind == 'weave':
        t[..., :3] = col(k['base']) * (.78 + .3 * nz[..., None])
        t[::k.get('step', 24), :, :3] *= .7
    elif kind == 'flat':
        t[..., :3] = col(k['base']) * (.94 + .12 * nz[..., None])
    elif kind == 'blade':
        t[..., :3] = col(k['base']) * (.88 + .22 * nz[..., None])
        t[:, n // 2 - 2:n // 2 + 2, :3] = col(k.get('ridge', (.95, .97, 1.0)))
        t[:, :6, :3] = col(k.get('edge', (.97, .98, 1.0)))
        t[:, -6:, :3] = col(k.get('edge', (.97, .98, 1.0)))
    elif kind == 'rune':          # dark inlaid plaque with a bright glyph (the whole quad carries the glow mask)
        t[..., :3] = col(k['bg'])
        g = rng.integers(0, 1 << 30)
        r2 = np.random.default_rng(g)
        pts = r2.random((5, 2)) * .6 + .2
        for a, b in ((0, 1), (1, 2), (2, 3), (3, 4), (1, 3), (0, 4)):
            if r2.random() < .8:
                _line(t, pts[a], pts[b], .035, col(k['fg']))
        _line(t, (.5, .08), (.5, .92), .03, col(k['fg']))
        t[:4, :, :3] = col(k['fg']) * .8
        t[-4:, :, :3] = col(k['fg']) * .8
    elif kind == 'emblem':
        u = (xx + .5) / n * 2 - 1
        v = (yy + .5) / n * 2 - 1
        t[..., :3] = col(k['base']) * (.94 + .12 * nz[..., None])
        border = (np.abs(u) > .88) | (np.abs(v) > .93)
        t[border, :3] = col(k['border'])
        sym = k.get('symbol', 'cross')
        if sym == 'cross':
            m = ((np.abs(u) < .16) & (np.abs(v) < .66)) | ((np.abs(v + .12) < .14) & (np.abs(u) < .5))
        elif sym == 'star':
            m = (np.abs(u) ** .6 + np.abs(v) ** .6) < .55
        else:                     # 'sigil': ring with a bar
            r = np.hypot(u, v * .8)
            m = ((r > .38) & (r < .52)) | ((np.abs(u) < .06) & (np.abs(v) < .6))
        t[m, :3] = col(k['mark'])
    else:
        raise ValueError(kind)
    return t


def make_atlas(spec, seed):
    rng = np.random.default_rng(seed)
    A = np.zeros((ATLAS, ATLAS, 4), np.float32)
    A[..., :3] = (.30, .30, .34)
    A[..., 3] = 1
    nz = smooth_noise(rng, TILE, 16)
    for i, name in enumerate(REGIONS):
        c, r = i % 4, i // 4
        kind, kw = spec[name]
        A[r * TILE:(r + 1) * TILE, c * TILE:(c + 1) * TILE] = paint(kind, rng, nz, **kw)
    return A


def region_uv(name, u, v):
    """u, v in 0..1 inside the region -> atlas uv (Blender bottom-up v); 2 px inset against bleeding."""
    i = REGIONS.index(name)
    c, r = i % 4, i // 4
    inset = 3.0 / ATLAS
    x0, y0 = c * TILE / ATLAS + inset, r * TILE / ATLAS + inset
    w = TILE / ATLAS - 2 * inset
    return (x0 + np.clip(u, 0, 1) * w, 1 - (y0 + (1 - np.clip(v, 0, 1)) * w))


# ------------------------------------------------------------------ LOD helpers
def sd(n):
    """Ring/side count at the current LOD."""
    return max(4, round(n * (1, .66, .5)[LOD]))


def dn(n, lod1=None):
    return (n, lod1 or max(1, n // 2), 1)[LOD]


def detail(max_lod):
    """True while the current LOD is at or below max_lod: gates small details."""
    return LOD <= max_lod


# ------------------------------------------------------------------ geometry accumulation
class Body:
    def __init__(self):
        self.V, self.UV, self.F, self.W, self.G, self.AO = [], [], [], [], [], []
        self.xfs = []

    def push(self, M):
        self.xfs.append(np.asarray(M, float))

    def pop(self):
        self.xfs.pop()

    def add(self, P, UV, F, W, region, convex=True):
        P = np.asarray(P, float)
        F = np.asarray(F, int)
        for M in self.xfs:
            P = P @ M[:3, :3].T + M[:3, 3]
        if convex:
            cen = P.mean(0)
            a, b, c = P[F[:, 0]], P[F[:, 1]], P[F[:, 2]]
            n = np.cross(b - a, c - a)
            flip = (n * ((a + b + c) / 3 - cen)).sum(1) < 0
            F = F.copy()
            F[flip] = F[flip][:, [0, 2, 1]]
        base = len(self.V)
        self.V.extend(P.tolist())
        self.UV.extend(UV)
        self.W.extend(W)
        self.G.extend([1.0 if region in GLOW_REGIONS else 0.0] * len(P))
        self.F.extend((F + base).tolist())


def blend_chain(axis_fn, links, sgn=1):
    """links: [(bone, breakpoint, halfwidth)] along axis_fn(p). Linear ramp of width 2*halfwidth around each later
    breakpoint; halfwidth 0 is a rigid split. sgn=-1 when the parameter decreases along the chain (height down a leg)."""
    def f(p):
        t = sgn * axis_fn(p)
        k = []
        for _, bp, h in links[1:]:
            bp = sgn * bp
            k.append((1.0 if t >= bp else 0.0) if h == 0 else float(np.clip((t - bp + h) / (2 * h), 0, 1)))
        w = {}
        prev = 1.0
        for (b, _, _), ki in zip(links, k + [0.0]):
            w[b] = prev - ki
            prev = ki
        return {b: x for b, x in w.items() if x > 1e-4}
    return f


def weights_for(spec, p):
    return spec(p) if callable(spec) else {spec: 1.0}


def frame(t, ref):
    t = t / np.linalg.norm(t)
    s = np.cross(t, ref)
    if np.linalg.norm(s) < 1e-6:
        s = np.cross(t, (0, 1, 0))
    s /= np.linalg.norm(s)
    return s, np.cross(s, t)


def loft(body, path, ra, rb, sides, region, wspec, caps=(True, True), ref=(1, 0, 0), vrange=(0, 1), dense=1, phase=0.0):
    """Rings along a polyline. ra: radius along the side axis, rb along the forward axis."""
    path = [np.array(p, float) for p in path]
    if LOD == 2 and len(path) >= 5 and dense == 1:                  # drop every other interior ring on the smallest LOD
        keep = [0] + list(range(2, len(path) - 1, 2)) + [len(path) - 1]
        path, ra, rb = [path[i] for i in keep], [ra[i] for i in keep], [rb[i] for i in keep]
    if dense > 1:
        def up(a):
            a = list(a)
            out = []
            for x, y in zip(a, a[1:]):
                out += [x + (y - x) * i / dense for i in range(dense)]
            return out + [a[-1]]
        path, ra, rb = up(path), up(ra), up(rb)
    n = len(path)
    length = [0.0]
    for a, b in zip(path, path[1:]):
        length.append(length[-1] + np.linalg.norm(b - a))
    P, UV, W = [], [], []
    for i in range(n):
        tan = path[min(i + 1, n - 1)] - path[max(i - 1, 0)]
        s, f = frame(tan, np.array(ref, float))
        for k in range(sides + 1):
            a = 2 * math.pi * k / sides + deg(phase)
            p = path[i] + f * rb[i] * math.cos(a) + s * ra[i] * math.sin(a)
            P.append(p)
            vv = vrange[0] + (vrange[1] - vrange[0]) * length[i] / (length[-1] or 1)
            UV.append(region_uv(region, k / sides, vv))
            W.append(weights_for(wspec, p))
    F = []
    row = sides + 1
    for i in range(n - 1):
        for k in range(sides):
            a, b, c, d = i * row + k, i * row + k + 1, (i + 1) * row + k + 1, (i + 1) * row + k
            F += [(a, b, c), (a, c, d)]
    for cap, idx in ((caps[0], 0), (caps[1], n - 1)):
        if cap:
            ci = len(P)
            P.append(path[idx])
            UV.append(region_uv(region, .5, .5))
            W.append(weights_for(wspec, path[idx]))
            for k in range(sides):
                F.append((ci, idx * row + k + 1, idx * row + k) if idx == 0 else (ci, idx * row + k, idx * row + k + 1))
    body.add(P, UV, F, W, region, False)


def box(body, c, size, region, wspec, rot=None, uvr=(0, 0, 1, 1)):
    sx, sy, sz = (np.array(size) / 2)
    c = np.array(c, float)
    R = np.eye(3) if rot is None else np.array(rot)
    faces = [((1, 0, 0), (0, 1, 0), (0, 0, 1)), ((-1, 0, 0), (0, -1, 0), (0, 0, 1)),
             ((0, 1, 0), (-1, 0, 0), (0, 0, 1)), ((0, -1, 0), (1, 0, 0), (0, 0, 1)),
             ((0, 0, 1), (1, 0, 0), (0, 1, 0)), ((0, 0, -1), (1, 0, 0), (0, -1, 0))]
    P, UV, W, F = [], [], [], []
    ext = np.array((sx, sy, sz))
    for nrm, ua, va in faces:
        nrm, ua, va = np.array(nrm), np.array(ua), np.array(va)
        for du, dv in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
            p = c + R @ (nrm * ext + ua * du * ext + va * dv * ext)
            P.append(p)
            UV.append(region_uv(region, uvr[0] + (du + 1) / 2 * (uvr[2] - uvr[0]), uvr[1] + (dv + 1) / 2 * (uvr[3] - uvr[1])))
            W.append(weights_for(wspec, p))
        b = len(P) - 4
        F += [(b, b + 1, b + 2), (b, b + 2, b + 3)]
    body.add(P, UV, F, W, region)


def quad(body, c, u_axis, v_axis, region, wspec, double=False):
    """Flat quad centred at c spanning +-u_axis and +-v_axis (vectors); facing u x v. double adds the back face."""
    c, ua, va = np.array(c, float), np.array(u_axis, float), np.array(v_axis, float)
    P = [c - ua - va, c + ua - va, c + ua + va, c - ua + va]
    UV = [region_uv(region, 0, 0), region_uv(region, 1, 0), region_uv(region, 1, 1), region_uv(region, 0, 1)]
    W = [weights_for(wspec, p) for p in P]
    F = [(0, 1, 2), (0, 2, 3)]
    if double:
        F += [(0, 2, 1), (0, 3, 2)]
    body.add(P, UV, F, W, region, False)


def panel(body, p0, p1, p2, p3, nx, nz, region, wspec, vrange=(0, 1)):
    """Bilinear sheet through four corners (p0 p1 along the top-left->top-right, p2 p3 the bottom right/left), double sided.
    Face winding makes the front face the side the (p1-p0) x (p3-p0) normal points to."""
    p0, p1, p2, p3 = (np.array(p, float) for p in (p0, p1, p2, p3))
    P, UV, W, F = [], [], [], []
    for i in range(nz + 1):
        t = i / nz
        for k in range(nx + 1):
            u = k / nx
            p = (p0 * (1 - u) + p1 * u) * (1 - t) + (p3 * (1 - u) + p2 * u) * t
            P.append(p)
            UV.append(region_uv(region, u, 1 - (vrange[0] + (vrange[1] - vrange[0]) * t)))
            W.append(weights_for(wspec, p))
    for i in range(nz):
        for k in range(nx):
            a = i * (nx + 1) + k
            F += [(a, a + 1, a + nx + 2), (a, a + nx + 2, a + nx + 1)]
    F = np.array(F)
    body.add(P, UV, F, W, region, False)
    body.add(np.array(P), UV, F[:, [0, 2, 1]], W, region, False)


def prism(body, outline, c, u, v, n, half, front, side, wspec, taper=.7):
    """Extruded flat polygon: outline (list of (x, y) in the u/v plane around centre c), thickness 2*half along n.
    The front face (normal +n, shrunk by `taper` towards the centre on the back) uses region `front`; the rim uses `side`."""
    c, u, v, n = (np.array(a, float) for a in (c, u, v, n))
    m = len(outline)
    xs = np.array([p[0] for p in outline])
    ys = np.array([p[1] for p in outline])
    x0, x1, y0, y1 = xs.min(), xs.max(), ys.min(), ys.max()
    P, UV, W, F = [], [], [], []
    def uvf(reg, x, y):
        return region_uv(reg, (x - x0) / max(x1 - x0, 1e-6), (y - y0) / max(y1 - y0, 1e-6))
    cx, cy = xs.mean(), ys.mean()
    for (x, y) in outline:
        P.append(c + u * x + v * y + n * half)
        UV.append(uvf(front, x, y))
    P.append(c + u * cx + v * cy + n * half)
    UV.append(uvf(front, cx, cy))
    for i in range(m):
        F.append((m, i, (i + 1) % m))
    base = len(P)
    for (x, y) in outline:
        P.append(c + u * (cx + (x - cx) * taper) + v * (cy + (y - cy) * taper) - n * half)
        UV.append(uvf(side, x, y))
    P.append(c + u * cx + v * cy - n * half)
    UV.append(uvf(side, cx, cy))
    for i in range(m):
        F.append((base + m, base + (i + 1) % m, base + i))
    for i in range(m):
        j = (i + 1) % m
        F += [(i, base + i, base + j), (i, base + j, j)]
    W = [weights_for(wspec, p) for p in P]
    body.add(P, UV, F, W, front, True)


def spike(body, base, direction, length, r, region, wspec, sides=5):
    d = np.array(direction, float)
    d /= np.linalg.norm(d)
    sides = 4 if LOD else sides
    loft(body, [base, np.array(base) + d * length], [r, 0.0], [r, 0.0], sides, region, wspec, caps=(True, False), ref=(0, 0, 1))


def frame_matrix(d, ref=(0, 0, 1), axis=0):
    """3x3 whose column `axis` is d (unit), other columns orthonormal and right-handed."""
    d = np.array(d, float) / np.linalg.norm(d)
    ref = np.array(ref, float)
    a = np.cross(ref, d)
    if np.linalg.norm(a) < 1e-6:
        a = np.cross((0, 1, 0), d)
    a /= np.linalg.norm(a)
    b = np.cross(d, a)
    cols = [None] * 3
    cols[axis] = d
    o = [i for i in range(3) if i != axis]
    cols[o[0]], cols[o[1]] = (a, b) if axis == 0 else (b, a)
    M = np.array(cols).T
    if np.linalg.det(M) < 0:
        M[:, o[0]] *= -1
    return M


# ------------------------------------------------------------------ Blender objects
def new_image(name, A):
    h, w = A.shape[:2]
    if name in bpy.data.images:
        bpy.data.images.remove(bpy.data.images[name])
    img = bpy.data.images.new(name, w, h, alpha=False)
    img.pixels.foreach_set(np.ascontiguousarray(A[::-1]).ravel().astype(np.float32))
    img.file_format = 'PNG'
    img.pack()
    return img


def make_armature(sc):
    arm = bpy.data.armatures.new('Skeleton_Humanoid')
    ao = bpy.data.objects.new('Skeleton_Humanoid', arm)
    sc.collection.objects.link(ao)
    bpy.context.view_layer.objects.active = ao
    bpy.ops.object.mode_set(mode='EDIT')
    for name in BONE_ORDER:
        _, h, t = JOINTS[name]
        eb = arm.edit_bones.new(name)
        eb.head, eb.tail = Vector(h), Vector(t)
    for name in BONE_ORDER:
        par = JOINTS[name][0]
        if par:
            arm.edit_bones[name].parent = arm.edit_bones[par]
    bpy.ops.object.mode_set(mode='OBJECT')
    for pb in ao.pose.bones:
        pb.rotation_mode = 'QUATERNION'
    return ao


def make_material(img, name):
    mat = bpy.data.materials.new(name)
    nt = mat.node_tree
    bsdf = nt.nodes['Principled BSDF']
    bsdf.inputs['Roughness'].default_value = 1.0
    bsdf.inputs['Metallic'].default_value = 0.0
    tn = nt.nodes.new('ShaderNodeTexImage')
    tn.image = img
    tn.interpolation = 'Closest'
    nt.links.new(tn.outputs['Color'], bsdf.inputs['Base Color'])
    mat.use_backface_culling = True
    return mat


def make_mesh_object(sc, ao, B, name, mat, ao_fn=None):
    """Skinned mesh from a Body. Vertex colour 'Color': RGB = soft height-based ambient shade, A = glow mask."""
    V = np.array(B.V)
    me = bpy.data.meshes.new(name)
    me.from_pydata(V.tolist(), [], B.F)
    me.update()
    uvl = me.uv_layers.new(name='UVMap')
    uv = np.array(B.UV)
    uvl.data.foreach_set('uv', uv[np.array(B.F).reshape(-1)].reshape(-1).astype(np.float32))
    me.shade_flat()
    z = V[:, 2]
    shade = .80 + .20 * np.clip(z / 1.1, 0, 1)
    col = np.zeros((len(V), 4), np.float32)
    col[:, :3] = shade[:, None]
    col[:, 3] = np.array(B.G, np.float32)
    ca = me.color_attributes.new('Color', 'BYTE_COLOR', 'POINT')
    ca.data.foreach_set('color', col.reshape(-1))
    me.color_attributes.active_color = ca
    me.materials.append(mat)
    ob = bpy.data.objects.new(name, me)
    sc.collection.objects.link(ob)
    groups = {n: ob.vertex_groups.new(name=n) for n in BONE_ORDER}
    acc = {n: ([], []) for n in BONE_ORDER}
    for vi, w in enumerate(B.W):
        items = sorted(w.items(), key=lambda kv: -kv[1])[:4]
        tot = sum(x for _, x in items)
        for b, x in items:
            acc[b][0].append(vi)
            acc[b][1].append(x / tot)
    for n, (idx, ws) in acc.items():
        for i, w in zip(idx, ws):
            groups[n].add([i], w, 'REPLACE')
    ob.parent = ao
    mod = ob.modifiers.new('Armature', 'ARMATURE')
    mod.object = ao
    return ob


# ------------------------------------------------------------------ animation
def ease(kind, t):
    if kind == 'in':
        return t * t
    if kind == 'out':
        return 1 - (1 - t) ** 2
    if kind == 'lin':
        return t
    return t * t * (3 - 2 * t)     # 'smooth'


def blend_pose(a, b, t):
    """Interpolate two compiled poses (rot: bone->(3,), loc: bone->(3,)) with factor t."""
    rot = {}
    for k in set(a[0]) | set(b[0]):
        va = np.array(a[0].get(k, (0, 0, 0)), float)
        vb = np.array(b[0].get(k, (0, 0, 0)), float)
        rot[k] = va + (vb - va) * t
    loc = {}
    for k in set(a[1]) | set(b[1]):
        va = np.array(a[1].get(k, (0, 0, 0)), float)
        vb = np.array(b[1].get(k, (0, 0, 0)), float)
        loc[k] = va + (vb - va) * t
    return rot, loc


def sample_keys(keys, f):
    """keys: [(frame, pose, ease_into_key)] sorted by frame. Pose at frame f (held before the first and after the last)."""
    if f <= keys[0][0]:
        return keys[0][1]
    for (f0, p0, _), (f1, p1, e) in zip(keys, keys[1:]):
        if f0 <= f <= f1:
            return blend_pose(p0, p1, ease(e, (f - f0) / (f1 - f0)))
    return keys[-1][1]


GROUND_POINTS = [('toe.L', 'tail', .03), ('toe.R', 'tail', .03), ('foot.L', 'head', .07), ('foot.R', 'head', .07),
                 ('shin.L', 'head', .06), ('shin.R', 'head', .06), ('hand.L', 'tail', .05), ('hand.R', 'tail', .05),
                 ('head', 'tail', .10), ('head', 'head', .10), ('chest', 'head', .11), ('pelvis', 'head', .10),
                 ('upperarm.L', 'head', .08), ('upperarm.R', 'head', .08)]


def ground_offset(rig, rot, loc):
    """Vertical shift for the pelvis so that no contact point is below the floor and the lowest one touches it."""
    M = rig.fk(rot, loc)
    off = -1e9
    for bone, end, clr in GROUND_POINTS:
        z = (rig.head(M, bone) if end == 'head' else rig.tail(M, bone))[2]
        off = max(off, clr - z)
    return off


def key_pose(ao, rig, f, rot, loc):
    """Keyframe every bone in `rot` / `loc` (model-axis degrees / metres) at frame f."""
    for b in BONE_ORDER:
        if b == 'root':
            continue                                    # root never moves: clips are in place
        pb = ao.pose.bones[b]
        Bm = rig.basis(b, rot.get(b, (0, 0, 0)), loc.get(b, (0, 0, 0)))
        q = Matrix(Bm[:3, :3].tolist()).to_quaternion()
        pb.rotation_quaternion = q
        pb.keyframe_insert('rotation_quaternion', frame=f)
        if b in loc:
            pb.location = Vector(Bm[:3, 3].tolist())
            pb.keyframe_insert('location', frame=f)


def author(ao, rig, name, frames, pose_at, ground=True, loop=False):
    """Create action `name` with a key on every bone at every frame 0..frames at 30 fps. pose_at(f) -> (rot, loc)."""
    act = bpy.data.actions.new(name)
    act.use_fake_user = True
    ao.animation_data_create()
    ao.animation_data.action = act
    bpy.context.view_layer.objects.active = ao
    bpy.ops.object.mode_set(mode='POSE')
    for pb in ao.pose.bones:
        pb.rotation_mode = 'QUATERNION'
    for f in range(frames + 1):
        rot, loc = pose_at(f)
        rot = {k: tuple(v) for k, v in rot.items()}
        loc = {k: np.array(v, float) for k, v in loc.items()}
        if ground:
            p = loc.get('pelvis', np.zeros(3)).copy()
            loc['pelvis'] = p
            p[2] += ground_offset(rig, rot, loc)
        key_pose(ao, rig, f, rot, loc)
    bpy.ops.object.mode_set(mode='OBJECT')
    return act


def export_glb(path, objs, ao, clips=None):
    """Export objs + armature. clips: action names, each exported as its own animation through an NLA track."""
    for o in bpy.context.scene.objects:
        o.select_set(False)
    for o in list(objs) + [ao]:
        o.select_set(True)
    bpy.context.view_layer.objects.active = ao
    if ao.animation_data:
        ao.animation_data.action = None
        for tr in list(ao.animation_data.nla_tracks):
            ao.animation_data.nla_tracks.remove(tr)
    if clips:
        ao.animation_data_create()
        for name in clips:
            tr = ao.animation_data.nla_tracks.new()
            tr.name = name
            st = tr.strips.new(name, 0, bpy.data.actions[name])
            st.name = name
    os.makedirs(os.path.dirname(path), exist_ok=True)
    bpy.ops.export_scene.gltf(
        filepath=path, export_format='GLB', use_selection=True, export_yup=True, export_apply=False,
        export_texcoords=True, export_normals=True, export_materials='EXPORT', export_image_format='AUTO',
        export_skins=True, export_def_bones=False, export_animations=bool(clips),
        export_animation_mode='NLA_TRACKS', export_frame_range=False, export_force_sampling=True,
        export_optimize_animation_size=False, export_morph=False, export_cameras=False, export_lights=False,
        export_vertex_color='ACTIVE', export_active_vertex_color_when_no_material=True)
    return path
